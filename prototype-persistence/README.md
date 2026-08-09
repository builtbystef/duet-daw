# PROTOTYPE — persistence layering (roadmap node rquzdc)

Disposable code. Answers: where does Duet's own project data live relative to
Tracktion's Edit file, and how does it version? Never ship any of this.

Experiments, each printing full state:

- **E1** — `EditItemID` stability across Duet's save path (flushState + direct
  XML write) and a full engine reload; plus the forced-duplicate-ID case, to
  see when reassignment actually happens.
- **E2** — the layering options in practice: a `DUET` child tree under
  `edit.state` vs a sidecar file, with the undo interplay demonstrated
  (nullptr-UM writes survive engine undo; through-UM writes revert with it).
- **E3** — the `flushState` undo-pollution hazard (skb4tp) reproduced, then
  candidate save strategies: no-flush save (does it lose params?), and
  flush-at-transaction-boundary.
- **E4** — versioning: what the engine stamps on the EDIT node, a Duet schema
  stamp, a v1→v2 migration, and too-new detection.

## Run

```sh
sudo apt install -y libasound2-dev libjack-jackd2-dev libglu1-mesa-dev mesa-common-dev

cmake --preset default
cmake --build --preset release -j 4   # -j 4: this machine freezes above that

./build/duet_persist_spike_artefacts/Release/duet_persist_spike
```
