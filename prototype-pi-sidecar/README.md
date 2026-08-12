# PROTOTYPE — disposable pi sidecar footprint experiment

This directory exists only on `prototype/pi-sidecar-footprint`. It is not production code.

Run the full build, structural checks, and five-run benchmark in one step:

```sh
npm run prototype
```

The live benchmark uses the provider credentials already configured for pi in
`~/.pi/agent/auth.json` and defaults to `openai-codex:gpt-5.6-luna`. Pass
`PROTOTYPE_MODEL=provider:model-id` to select a different configured model.

The generated files under `dist/` and `results/` are deliberately ignored.
