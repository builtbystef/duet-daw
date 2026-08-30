// The Tool Vocabulary and the one write-tool, declared for the model.
//
// This table is the whole of what the Collaborator can do. There are no built-in
// tools underneath it: the agent is constructed with these and nothing else, so
// there is no file, shell, web or terminal tool to be reached for. Every call
// made against this table is forwarded across the socket to the DAW, which owns
// the answers (ADR 0003).
//
// The DAW holds the same closed set on its side and shares no code with this
// one, which is deliberate: two independent statements of one contract is what
// makes an agreement between them evidence about the contract.
//
// A description here is prompt content, and it is charged for on every request,
// so each one says what the tool answers and what a value in the answer means —
// including the two things the spec says a description must document: the fixed
// spectral band set, and which values cross wrapped as estimates.

import { Type } from "@earendil-works/pi-ai";
import type { TSchema } from "@earendil-works/pi-ai";

/** One tool as this sidecar declares it, before it is given a body. */
export interface ToolDeclaration {
    name: string;
    label: string;
    description: string;
    parameters: TSchema;
}

const trackId = Type.String({
    description: "A track id as the read tools write it: `track-3`, or `track-master` for the master bus.",
});

const barRange = Type.Optional(
    Type.Tuple([Type.Number(), Type.Number()], {
        description: "Bars to look at, first and last, counting from 1. Omit for the whole track.",
    }),
);

const note = Type.Object({
    pitch: Type.Integer({ description: "MIDI note number, 0..127. 60 is middle C." }),
    startBeats: Type.Number({ description: "Beats from the start of the clip." }),
    lengthBeats: Type.Number({ description: "Length in beats, greater than zero." }),
    velocity: Type.Integer({ description: "MIDI velocity, 1..127." }),
});

const placeholderRef = Type.Optional(
    Type.String({
        description:
            "Names what this operation creates, so a later operation of the same element can refer to it. " +
            "Begins with `#`, which no project id ever does.",
    }),
);

const automationTarget = Type.Union(
    [
        Type.Object({ kind: Type.Literal("volume") }),
        Type.Object({ kind: Type.Literal("pan") }),
        Type.Object({
            kind: Type.Literal("pluginParam"),
            pluginId: Type.String(),
            paramId: Type.String(),
        }),
    ],
    { description: "Which curve: the track's volume, its pan, or one parameter of one of its plugins." },
);

/** One entry of the edit vocabulary, written the way the DAW reads it: the
    operation's name in `op`, and its own members beside it.
*/
function operation(name: string, members: Record<string, TSchema>, description: string) {
    return Type.Object({ op: Type.Literal(name), ...members }, { description });
}

const editOperation = Type.Union(
    [
        operation(
            "midi.addNotes",
            { clipId: Type.String(), notes: Type.Array(Type.Intersect([note, Type.Object({ ref: placeholderRef })])) },
            "Add notes to a MIDI clip.",
        ),
        operation(
            "midi.removeNotes",
            { clipId: Type.String(), noteIds: Type.Array(Type.String()) },
            "Remove notes from a MIDI clip, by the note ids get_midi returned.",
        ),
        operation(
            "midi.modifyNotes",
            {
                clipId: Type.String(),
                changes: Type.Array(
                    Type.Object({
                        noteId: Type.String(),
                        pitch: Type.Optional(Type.Integer()),
                        startBeats: Type.Optional(Type.Number()),
                        lengthBeats: Type.Optional(Type.Number()),
                        velocity: Type.Optional(Type.Integer()),
                    }),
                ),
            },
            "Change notes that are already in a MIDI clip. Quantizing, transposing and rewriting a rhythm are all this.",
        ),
        operation(
            "clip.createMidi",
            {
                trackId: Type.String(),
                startBar: Type.Number(),
                lengthBars: Type.Number(),
                name: Type.Optional(Type.String()),
                notes: Type.Optional(Type.Array(Type.Intersect([note, Type.Object({ ref: placeholderRef })]))),
                ref: placeholderRef,
            },
            "Put a new MIDI clip on a MIDI track, optionally with its notes already in it.",
        ),
        operation("clip.delete", { clipId: Type.String() }, "Delete a clip."),
        operation(
            "clip.move",
            { clipId: Type.String(), trackId: Type.Optional(Type.String()), startBar: Type.Number() },
            "Move a clip to another bar, and optionally to another track.",
        ),
        operation(
            "clip.trim",
            { clipId: Type.String(), startBar: Type.Number(), lengthBars: Type.Number() },
            "Change where a clip starts and how long it is.",
        ),
        operation(
            "clip.setLoop",
            {
                clipId: Type.String(),
                looped: Type.Boolean(),
                loopLengthBars: Type.Optional(Type.Number()),
            },
            "Turn a clip's looping on or off, and say how long one repeat is.",
        ),
        operation(
            "clip.duplicate",
            {
                clipId: Type.String(),
                toTrackId: Type.Optional(Type.String()),
                atBar: Type.Number(),
                ref: placeholderRef,
            },
            "Copy a clip to another bar, and optionally to another track.",
        ),
        operation(
            "track.create",
            {
                kind: Type.Union([Type.Literal("midi"), Type.Literal("audio"), Type.Literal("group")]),
                name: Type.String(),
                instrument: Type.Optional(
                    Type.Union([Type.Literal("synth"), Type.Literal("sampler")], {
                        description: "A built-in instrument for a new MIDI track.",
                    }),
                ),
                ref: placeholderRef,
            },
            "Add a track. A group track is a bus other tracks can be routed into.",
        ),
        operation("track.rename", { trackId: Type.String(), name: Type.String() }, "Rename a track."),
        operation("track.delete", { trackId: Type.String() }, "Delete a track and everything on it."),
        operation(
            "track.setOutput",
            { trackId: Type.String(), busId: Type.String() },
            "Route a track into a bus — a group track, or the master.",
        ),
        operation(
            "mixer.set",
            {
                trackId: Type.String(),
                volumeDb: Type.Optional(Type.Number({ description: "Fader position in dB." })),
                pan: Type.Optional(Type.Number({ description: "-1 hard left, 0 centre, +1 hard right." })),
                mute: Type.Optional(Type.Boolean()),
                solo: Type.Optional(Type.Boolean()),
            },
            "Set any of a track's fader, pan, mute and solo. Leave out what should not move.",
        ),
        operation(
            "mixer.setSend",
            { trackId: Type.String(), busId: Type.String(), levelDb: Type.Number() },
            "Set how much of a track is sent to a bus.",
        ),
        operation(
            "plugin.add",
            {
                trackId: Type.String(),
                position: Type.Integer({ description: "Where in the producer's chain, counting from 0." }),
                plugin: Type.Union([
                    Type.Object({
                        builtin: Type.Union([
                            Type.Literal("eq"),
                            Type.Literal("compressor"),
                            Type.Literal("reverb"),
                            Type.Literal("synth"),
                            Type.Literal("sampler"),
                        ]),
                    }),
                    Type.Object({
                        external: Type.String({ description: "A plugin id from the machine's known plugins." }),
                    }),
                ]),
                ref: placeholderRef,
            },
            "Put a plugin on a track at a position in its chain.",
        ),
        operation("plugin.remove", { pluginId: Type.String() }, "Take a plugin off a track."),
        operation(
            "plugin.reorder",
            { trackId: Type.String(), pluginId: Type.String(), position: Type.Integer() },
            "Move a plugin to another position in its track's chain.",
        ),
        operation(
            "plugin.setParam",
            {
                pluginId: Type.String(),
                paramId: Type.String(),
                value: Type.Number({
                    description:
                        "A parameter takes the number get_plugin_chain reported for it: a real unit where the " +
                        "chain gave it a unit, and the normalised 0..1 where the chain gave a vendorName and a " +
                        "normalizedValue. The two are not interchangeable and a value in the wrong one is refused.",
                }),
            },
            "Set one parameter of one plugin.",
        ),
        operation(
            "plugin.setSidechainSource",
            { pluginId: Type.String(), sourceTrackId: Type.String() },
            "Point a plugin's sidechain at a track — the kick, usually.",
        ),
        operation(
            "automation.setPoints",
            {
                trackId: Type.String(),
                target: automationTarget,
                points: Type.Array(Type.Object({ timeBeats: Type.Number(), value: Type.Number() })),
            },
            "Draw points on an automation curve. Segments between them are straight.",
        ),
        operation(
            "automation.removePoints",
            {
                trackId: Type.String(),
                target: automationTarget,
                range: Type.Tuple([Type.Number(), Type.Number()], {
                    description: "The stretch to clear, in beats: first and last.",
                }),
            },
            "Clear a stretch of an automation curve.",
        ),
        operation("project.setTempo", { bpm: Type.Number() }, "Set the project's tempo."),
        operation(
            "project.setTimeSignature",
            { numerator: Type.Integer(), denominator: Type.Integer() },
            "Set the project's time signature.",
        ),
    ],
    {
        description:
            "One edit. `op` says which. Every id is one the read tools handed you, or a `#placeholder` an " +
            "earlier operation of this same element declared as its `ref`.",
    },
);

/** The seven read tools and the one write-tool, in the order the model meets
    them: the project as written first, then what had to be measured, then what
    had to be guessed at, then the one thing that changes anything.
*/
export const vocabulary: ToolDeclaration[] = [
    {
        name: "list_tracks",
        label: "List tracks",
        description:
            "Every track in the project, with its mixer state, its plugins and what is on it. Buses are tracks " +
            "here: the master is `track-master` and each group is itself. Start here — it is the cheapest way to " +
            "learn what the project is.",
        parameters: Type.Object({}),
    },
    {
        name: "get_arrangement",
        label: "Get arrangement",
        description:
            "The shape of the project in time: tempo, time signature, length in bars, named sections, and where " +
            "every clip sits on every track. `key` is there only when the project declares one; when it is absent " +
            "nobody has said what the key is, and estimate_audio_content is the only way to find out.",
        parameters: Type.Object({}),
    },
    {
        name: "get_midi",
        label: "Get MIDI",
        description:
            "The notes in a MIDI track's clips — pitch, position in beats, length and velocity — each with the " +
            "note id that midi.removeNotes and midi.modifyNotes take. Ask for one clip, or leave clipId out for " +
            "every clip on the track.",
        parameters: Type.Object({
            trackId,
            clipId: Type.Optional(Type.String({ description: "One clip, instead of all of the track's." })),
        }),
    },
    {
        name: "get_plugin_chain",
        label: "Get plugin chain",
        description:
            "A track's plugins in order, with their parameters. A built-in plugin's parameters are Duet's own, so " +
            "their names, units and values are facts. A scanned plugin's are the plugin's: the normalised 0..1 " +
            "value is a fact, and the text it displays for that value crosses as an estimate, because what it " +
            "means is the plugin vendor's business and not Duet's. Every hosted plugin also carries a dry and a " +
            "wet level that are Duet's own and not the vendor's — they are the two with a name and a unit rather " +
            "than a vendorName, they are in decibels, and they set how much of that plugin's output is heard. " +
            "A plugin the project names and this machine does not have is in the chain with available false: it " +
            "is part of the sound the producer is asking about, and it is never left out quietly.",
        parameters: Type.Object({ trackId }),
    },
    {
        name: "get_automation",
        label: "Get automation",
        description:
            "The automation curves drawn on a track — volume, pan, or a plugin parameter — as their points in " +
            "beats. Straight lines between the points.",
        parameters: Type.Object({ trackId }),
    },
    {
        name: "get_track_analysis",
        label: "Analyse track",
        description:
            "Measurements of what a track actually puts out, computed over its rendered audio by documented " +
            "routines, so every value is a fact and not a guess. Loudness (peak, true peak, RMS, integrated and " +
            "short-term LUFS per ITU-R BS.1770, crest factor), spectrum, stereo correlation and width, and " +
            "onset times in beats. The spectrum is the energy in seven fixed bands — " +
            "sub 20-60 Hz, low 60-250 Hz, low-mid 250-500 Hz, mid 500-2000 Hz, " +
            "high-mid 2000-4000 Hz, high 4000-10000 Hz, air 10000-20000 Hz — plus its centroid and its " +
            "flatness. " +
            "The first call on a track can take a few seconds; the producer keeps working while it does.",
        parameters: Type.Object({ trackId, barRange }),
    },
    {
        name: "estimate_audio_content",
        label: "Estimate audio content",
        description:
            "What is probably being played on an audio track, for the things no routine can measure outright: " +
            "its key, the chord in each of its bars, the notes it holds, and what it sounds like it is played " +
            "on. Every value comes back wrapped as " +
            "an estimate, with the method and a confidence, because every one of them is a guess. Use it when the " +
            "project has not declared what you need and the answer matters; say plainly that you used it. " +
            "A track that gives nothing to read — silence, or nothing pitched — is answered with nothing rather " +
            "than with a guess. The notes and the instrument are read by a model this Duet may have been built " +
            "without, and a build without it says so when you ask for them.",
        parameters: Type.Object({
            trackId,
            barRange,
            aspects: Type.Optional(
                Type.Array(
                    Type.Union([
                        Type.Literal("key"),
                        Type.Literal("chords"),
                        Type.Literal("notes"),
                        Type.Literal("instrument"),
                    ]),
                    { description: "What to estimate. Omit for all of it." },
                ),
            ),
        }),
    },
    {
        name: "suggest",
        label: "Suggest a change",
        description:
            "Offer the producer a change. Nothing happens to the project: this makes ghost clips and ghost fader " +
            "positions they can hear in place and then accept element by element, or refuse. One per turn. Each " +
            "element is one human-meaningful change, described in the producer's language, and must stand alone " +
            "— everything it needs is among its own operations. Answers with the Suggestion's id, or says where " +
            "the call was wrong so you can correct it and call again.",
        parameters: Type.Object({
            summary: Type.String({ description: "What the whole change is, in one line, for the card's heading." }),
            elements: Type.Array(
                Type.Object({
                    description: Type.String({
                        description: "What this one change does to the music, in the producer's words.",
                    }),
                    operations: Type.Array(editOperation),
                }),
                { minItems: 1 },
            ),
        }),
    },
];

/** The tool names, in declaration order. */
export const vocabularyNames: string[] = vocabulary.map((tool) => tool.name);
