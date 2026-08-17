#!/usr/bin/env bash
# Starts the duet-daw agent sandbox. THIS SCRIPT RUNS ON THE HOST.
# A sandboxed agent can edit this file, and the edits run on your host the
# next time that you start it. Review `git diff -- sandbox/` before each
# start.
#
# The security flags (--cap-drop, --security-opt, the mount list) are the
# sandbox boundary. Do not weaken them. The resource limits below them only
# protect the host. Edit those freely.
#
# --memory=12g is the one limit worth keeping: a JUCE + Tracktion translation
# unit costs about 2 GB, so a build that forgets `-j 4` (AGENTS.md) exhausts
# this 15 GB machine. Inside the cap the kernel kills the build and the
# iteration fails; on the host the same mistake freezes the desktop.
#
# --dns is here because rootless podman hands the container its own resolver
# at 10.0.2.3, which does not answer on this host; the LAN router is not
# reachable from the container's namespace either. Without this flag every
# FetchContent download fails while raw IPs still work. Change it if your
# network wants a different upstream.
set -euo pipefail

cd "$(dirname "$0")/.."
REPO="$(pwd)"
NAME="sandbox-duet-daw"

tty_flags="-i"
[ -t 0 ] && tty_flags="-it"

exec podman run \
  --rm $tty_flags \
  --name "$NAME" \
  --cap-drop=all \
  --security-opt=no-new-privileges \
  --userns=keep-id:uid=1000,gid=1000 \
  --dns 1.1.1.1 \
  --pids-limit=2048 \
  --memory=12g \
  --cpus=12 \
  --volume "$REPO:/workspace" \
  --volume "$NAME-home:/home/agent" \
  --env "XDG_RUNTIME_DIR=/home/agent/.xdg-runtime" \
  --env "GIT_AUTHOR_NAME=$(git config user.name)" \
  --env "GIT_AUTHOR_EMAIL=$(git config user.email)" \
  --env "GIT_COMMITTER_NAME=$(git config user.name)" \
  --env "GIT_COMMITTER_EMAIL=$(git config user.email)" \
  --workdir /workspace \
  "$NAME" \
  "${@:-bash}"
