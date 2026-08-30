// Where the producer's provider credentials are kept, and which providers they
// have set up at all.
//
// pi's own store holds credentials in memory and loses them with the process —
// its documentation says apps inject a persistent one — and a producer who has
// to paste their API key at every launch has not been given bring-your-own
// credentials. So this is Duet's: one JSON file at the path the DAW names, which
// is beside the app's own settings and never inside a project folder. A
// credential belongs to the provider layer and never to project data (spec
// js437t), and nothing in `duet_persistence` has ever heard of one.
//
// It keeps one thing more than pi's store does: the set of providers the
// producer has set up. A provider stays in that set when its credential is
// removed, which is what makes its models go on standing in the picker marked
// unusable rather than vanishing from it (issue i84fbb).

import type { AuthOperationOptions, Credential, CredentialInfo, CredentialStore } from "@earendil-works/pi-ai";
import { chmod, mkdir, writeFile } from "node:fs/promises";
import { dirname } from "node:path";

/** What the file holds for one provider: the credential, when there is one, and
    the entry itself, which says the producer set this provider up.
*/
interface Account {
    credential?: Credential;
}

interface StoredAccounts {
    version: number;
    providers: Record<string, Account>;
}

const fileVersion = 1;

/** Owner-only, because the file holds API keys and OAuth refresh tokens. */
const fileMode = 0o600;

export class ProviderAccounts implements CredentialStore {
    private readonly accounts = new Map<string, Account>();
    private writing: Promise<unknown> = Promise.resolve();

    private constructor(private readonly path: string | undefined) {}

    /** Reads what is stored, or begins empty — a first launch, and a file that
        cannot be read, are the same thing to a producer: nothing is set up yet.

        With no path the store is in memory and goes with the process, which is
        what a sidecar nobody gave a place to write is asked to be.
    */
    static async open(path: string | undefined): Promise<ProviderAccounts> {
        const accounts = new ProviderAccounts(path);

        if (path === undefined) return accounts;

        try {
            const stored = (await Bun.file(path).json()) as StoredAccounts;

            for (const [provider, account] of Object.entries(stored.providers ?? {}))
                accounts.accounts.set(provider, { credential: account.credential });
        } catch {
            // Nothing stored yet, or nothing readable. Either way there is
            // nothing to say to anyone: the producer sets a provider up and the
            // file is written then.
        }

        return accounts;
    }

    async read(providerId: string, _options?: AuthOperationOptions): Promise<Credential | undefined> {
        return this.accounts.get(providerId)?.credential;
    }

    async list(_options?: AuthOperationOptions): Promise<readonly CredentialInfo[]> {
        const held: CredentialInfo[] = [];

        for (const [providerId, account] of this.accounts)
            if (account.credential !== undefined) held.push({ providerId, type: account.credential.type });

        return held;
    }

    /** The one write path, serialized, as pi's contract asks: `fn` sees what is
        stored now, and what it returns replaces it. `undefined` leaves the entry
        as it was, which is how a refresh that decided nothing says so.
    */
    async modify(
        providerId: string,
        fn: (current: Credential | undefined) => Promise<Credential | undefined>,
        _options?: AuthOperationOptions,
    ): Promise<Credential | undefined> {
        return this.serialized(async () => {
            const replacement = await fn(this.accounts.get(providerId)?.credential);

            if (replacement !== undefined) {
                this.accounts.set(providerId, { credential: replacement });
                await this.save();
            }

            return this.accounts.get(providerId)?.credential;
        });
    }

    /** Logout: the credential goes and the provider stays set up, so that its
        models stay in the picker marked unusable (issue i84fbb, criterion 9).
    */
    async delete(providerId: string, _options?: AuthOperationOptions): Promise<void> {
        await this.serialized(async () => {
            this.accounts.set(providerId, {});
            await this.save();
        });
    }

    /** Says the producer has set this provider up, whether or not a credential
        landed. Beginning a sign-in is setting a provider up: the picker should
        show what is coming rather than nothing at all.
    */
    async remember(providerId: string): Promise<void> {
        if (this.accounts.has(providerId)) return;

        await this.serialized(async () => {
            this.accounts.set(providerId, {});
            await this.save();
        });
    }

    /** Every provider the producer has set up, in the order they set them up. */
    setUp(): string[] {
        return [...this.accounts.keys()];
    }

    private async serialized<T>(work: () => Promise<T>): Promise<T> {
        const next = this.writing.then(work, work);
        this.writing = next.catch(() => undefined);

        return next;
    }

    /** Writes the whole file, owner-only. Nothing here says anything to a
        terminal: a store that cannot be written throws to whoever asked, and the
        method that asked answers the DAW with it.
    */
    private async save(): Promise<void> {
        if (this.path === undefined) return;

        const stored: StoredAccounts = { version: fileVersion, providers: {} };

        for (const [providerId, account] of this.accounts) stored.providers[providerId] = account;

        await mkdir(dirname(this.path), { recursive: true });
        await writeFile(this.path, `${JSON.stringify(stored, null, 2)}\n`, { mode: fileMode });
        await chmod(this.path, fileMode);
    }
}
