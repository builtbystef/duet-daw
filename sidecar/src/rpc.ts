// The sidecar's half of the seam: newline-delimited JSON-RPC 2.0 over the local
// socket the DAW listens on (ADR 0003).
//
// Written out here rather than taken from a library, for the same reason the
// test-double sidecar writes its own framing out: this side and the DAW side
// agreeing is only evidence about the protocol if neither of them is the other's
// implementation.
//
// Both directions travel over the one connection. The DAW asks `configure`,
// `run.start`, `run.cancel` and `shutdown`; this side asks `tool.call` and sends
// the three run notifications. Nothing here interprets any of them — that is
// `session.ts`'s work — and nothing here writes to stdout or stderr.

import type { Socket } from "bun";

/** The JSON-RPC error codes this side answers with. The first four are the
    specification's own; `runAlreadyActive` is the one Duet code that reaches
    back the other way, and it carries the same number the DAW gives it.
*/
export const rpcError = {
    parseError: -32700,
    invalidRequest: -32600,
    methodNotFound: -32601,
    invalidParams: -32602,
    internalError: -32603,
    runAlreadyActive: -32001,
} as const;

/** An error the far side should see as a JSON-RPC error member rather than as a
    crash. Anything else thrown by a handler becomes an internal error.
*/
export class RpcFailure extends Error {
    constructor(
        readonly code: number,
        message: string,
    ) {
        super(message);
        this.name = "RpcFailure";
    }
}

export type Params = Record<string, unknown>;
export type Handler = (params: Params) => Promise<unknown> | unknown;

interface Pending {
    resolve: (result: unknown) => void;
    reject: (failure: Error) => void;
}

/** One JSON-RPC peer on one connection. */
export class RpcPeer {
    private readonly handlers = new Map<string, Handler>();
    private readonly pending = new Map<number, Pending>();
    private readonly held: string[] = [];
    private buffer = "";
    private nextId = 1;
    private closed = false;
    private serving = false;
    private onClosed: (() => void) | undefined;

    private constructor(private readonly socket: Socket<undefined>) {}

    /** Connects to the socket the DAW is listening on. */
    static async connect(path: string): Promise<RpcPeer> {
        let peer: RpcPeer | undefined;

        const socket = await Bun.connect<undefined>({
            unix: path,
            socket: {
                data(_socket, data) {
                    peer?.receive(data.toString());
                },
                close() {
                    peer?.finish();
                },
                error() {
                    peer?.finish();
                },
            },
        });

        peer = new RpcPeer(socket);

        return peer;
    }

    /** Answers one method the DAW may call. */
    on(method: string, handler: Handler): void {
        this.handlers.set(method, handler);
    }

    /** Begins answering. Whatever arrived before this waited, in order.

        Connecting resolves a promise, and handlers are registered after it
        resolves, so there is a window in which the socket exists and nothing can
        answer it. Holding rather than dispatching is what stops a `configure`
        that lands in that window from being answered method-not-found by a peer
        that is one line away from being able to answer it.
    */
    serve(): void {
        if (this.serving) return;

        this.serving = true;

        const waiting = this.held.splice(0, this.held.length);

        for (const line of waiting) this.dispatch(line);
    }

    /** Called once when the connection goes, however it goes. */
    whenClosed(listener: () => void): void {
        this.onClosed = listener;
    }

    get isClosed(): boolean {
        return this.closed;
    }

    /** Asks the DAW something, and waits for the answer. Rejects with an
        `RpcFailure` when the DAW answers with an error member, and with a plain
        error when the connection goes before an answer does.
    */
    request(method: string, params: Params): Promise<unknown> {
        if (this.closed) return Promise.reject(new Error("the connection to the DAW is closed"));

        const id = this.nextId++;

        return new Promise<unknown>((resolve, reject) => {
            this.pending.set(id, { resolve, reject });
            this.write({ jsonrpc: "2.0", id, method, params });
        });
    }

    /** Tells the DAW something, expecting no answer. */
    notify(method: string, params: Params): void {
        this.write({ jsonrpc: "2.0", method, params });
    }

    close(): void {
        this.socket.end();
        this.finish();
    }

    private write(message: unknown): void {
        if (this.closed) return;

        this.socket.write(`${JSON.stringify(message)}\n`);
    }

    /** Fails every request still waiting, and says so once. */
    private finish(): void {
        if (this.closed) return;

        this.closed = true;

        for (const waiting of this.pending.values()) waiting.reject(new Error("the DAW closed the connection"));

        this.pending.clear();
        this.onClosed?.();
    }

    private receive(chunk: string): void {
        this.buffer += chunk;

        for (;;) {
            const newline = this.buffer.indexOf("\n");

            if (newline < 0) break;

            const line = this.buffer.slice(0, newline);
            this.buffer = this.buffer.slice(newline + 1);

            if (line.trim().length === 0) continue;

            if (this.serving) this.dispatch(line);
            else this.held.push(line);
        }
    }

    private dispatch(line: string): void {
        let message: Record<string, unknown>;

        try {
            message = JSON.parse(line) as Record<string, unknown>;
        } catch {
            this.write({
                jsonrpc: "2.0",
                id: null,
                error: { code: rpcError.parseError, message: "the sidecar could not parse that line" },
            });

            return;
        }

        if (typeof message !== "object" || message === null) return;

        if ("method" in message) {
            void this.answer(message);

            return;
        }

        this.settle(message);
    }

    /** Hands one response to whoever asked the question. */
    private settle(message: Record<string, unknown>): void {
        const id = message["id"];

        if (typeof id !== "number") return;

        const waiting = this.pending.get(id);

        if (waiting === undefined) return;

        this.pending.delete(id);

        const failure = message["error"];

        if (failure !== undefined && failure !== null) {
            const described = failure as { code?: number; message?: string };

            waiting.reject(new RpcFailure(described.code ?? rpcError.internalError, described.message ?? "the DAW refused"));

            return;
        }

        waiting.resolve(message["result"]);
    }

    /** Runs one method the DAW called, and answers it if it asked for an answer. */
    private async answer(message: Record<string, unknown>): Promise<void> {
        const id = message["id"];
        const wantsAnswer = typeof id === "number" || typeof id === "string";
        const method = message["method"];
        const raw = message["params"];
        const params: Params = typeof raw === "object" && raw !== null ? (raw as Params) : {};

        if (typeof method !== "string") {
            if (wantsAnswer)
                this.write({
                    jsonrpc: "2.0",
                    id,
                    error: { code: rpcError.invalidRequest, message: "a request says which method in its method" },
                });

            return;
        }

        const handler = this.handlers.get(method);

        if (handler === undefined) {
            if (wantsAnswer)
                this.write({
                    jsonrpc: "2.0",
                    id,
                    error: { code: rpcError.methodNotFound, message: `the sidecar has no method ${method}` },
                });

            return;
        }

        try {
            const result = await handler(params);

            if (wantsAnswer) this.write({ jsonrpc: "2.0", id, result: result ?? {} });
        } catch (thrown) {
            if (!wantsAnswer) return;

            const code = thrown instanceof RpcFailure ? thrown.code : rpcError.internalError;
            const described = thrown instanceof Error ? thrown.message : String(thrown);

            this.write({ jsonrpc: "2.0", id, error: { code, message: described } });
        }
    }
}
