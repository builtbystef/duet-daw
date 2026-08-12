---
id: i84fbb
title: 'Model access: provider auth, the model list, and the model picker'
state: todo
priority: medium
depends_on:
    - oocnng
    - xxv9ng
parent: js437t
created: 2026-08-12T04:03:32Z
updated: 2026-08-12T04:03:32Z
---

## What to build

Bring-your-own credentials. A setup surface where the Target Producer enters an API key per provider or completes a subscription OAuth flow — the sidecar returns the URL and instructions, the DAW shows them — and a picker exposing whatever they configured, each entry naming its provider and whether it is authenticated. No provider is privileged in the picker; the recommended default is selected when its provider is authenticated, otherwise the first configured one is. With nothing configured, the Collaborator panel shows a setup state rather than an error.

Credentials belong to the provider layer, never to Duet's project data. The model choice is an app-global setting, not part of a project.

## Acceptance criteria

- [ ] With no credentials configured, the panel shows a setup state that leads to the settings surface, and sending a message is not offered as an action that will fail.
- [ ] Entering an API key makes that provider's models appear authenticated in the picker, and the next Task Run uses the chosen model.
- [ ] The OAuth flow shows the URL and instructions the sidecar returned, and completing it makes that provider's models authenticated without an API key.
- [ ] The picker lists every configured provider's models with their provider names, orders no provider ahead of another for being that provider, and marks unauthenticated entries as unusable.
- [ ] The recommended default is selected when its provider is authenticated; otherwise the first configured provider's model is; and the choice survives an app restart.
- [ ] Switching models between runs takes effect on the next run, with no DAW restart and no sidecar restart required.
- [ ] An invalid API key surfaces as a plain message — at entry or at the first run — never as a silent failure or a hang.
- [ ] No credential is written into a project folder or into any file the project's persistence owns.
- [ ] Removing a provider's credentials returns its models to unauthenticated, and if the selected model was one of them the picker falls back rather than leaving an unusable selection.
