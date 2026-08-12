#!/bin/sh
# PROTOTYPE — disposable measurement code. Do not ship.
set -eu

cd "$(dirname "$0")/.."
mkdir -p dist results

npm run build
npm run build:coding-agent

du -b dist/duet-pi-core-prototype dist/duet-pi-coding-agent-prototype
du -sb node_modules

./dist/duet-pi-core-prototype --inspect

: > results/first-provider-events.jsonl
: > results/first-text.jsonl
run=1
while [ "$run" -le 5 ]; do
  ./dist/duet-pi-core-prototype | tee "results/run-$run.jsonl"
  grep '"label":"first-provider-event"' "results/run-$run.jsonl" >> results/first-provider-events.jsonl
  grep '"label":"first-text"' "results/run-$run.jsonl" >> results/first-text.jsonl
  run=$((run + 1))
done

node ./scripts/median.mjs results/first-provider-events.jsonl firstProviderEventMilliseconds | tee results/summary.json
node ./scripts/median.mjs results/first-text.jsonl firstTextMilliseconds | tee -a results/summary.json
