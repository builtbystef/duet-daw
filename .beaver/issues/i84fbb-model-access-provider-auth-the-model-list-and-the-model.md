---
id: i84fbb
title: 'Model access: provider auth, the model list, and the model picker'
state: done
assignee: claude
priority: medium
depends_on:
    - oocnng
    - xxv9ng
parent: js437t
created: 2026-08-12T04:03:32Z
updated: 2026-08-30T04:45:00Z
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

## Notes

**claude** — 2026-08-17T04:12:08Z

Decision (2026-08-17): the recommended default model is gpt-5.6-terra (as js437t and lxwoas recorded; fod077 7/7). d8k46e's runs on gpt-5.6-luna were credential circumstance, not a revision. Selected when its provider is authenticated; otherwise the first configured provider, per spec.

**claude** — 2026-08-30T03:25:28Z

Design (2026-08-29, at the start of the build). Three decisions the criteria need and the spec does not name, taken here rather than escalated because none of them reverses anything the spec or an ADR settled:

1. What models.list contains. pi ships 40 providers and 1290 models; a picker of all of them is not a picker. So a provider contributes its models when it is SET UP — the producer has stored a credential for it, or its ambient environment already authenticates it — and every entry carries whether that provider's auth resolves right now. Providers in pi's own registration order, models in the provider's; Duet imposes no order of its own, which is what 'no provider is privileged' means here. The listing also carries the providers themselves (id, name, whether they take a key or a subscription sign-in, set up, authenticated), because the setup surface has to offer a provider before there is a credential to list models from — the spec asks models.list to back that surface.

2. Two protocol methods beyond the spec's table, both inside the auth.* family the spec names. auth.remove { provider } is what criterion 9 needs: it drops the credential and leaves the provider set up, so its models stay in the picker marked unusable rather than disappearing. auth.completeOAuth { provider, code } is what makes criterion 3 reachable: pi's OAuth flows race a local callback server against a manual code paste, and with no way to deliver the code a producer whose browser is elsewhere cannot finish. The DAW shows the URL and instructions the flow returned and hands back whatever the producer pastes.

3. Where the model is configured. The service gains a non-blocking setModel; the service thread sends configure before run.start whenever the connected sidecar has not been configured with that model. That is what makes a switch take effect on the next run with no sidecar restart, and it also configures a sidecar that was respawned after dying — the message thread never blocks on a socket round trip to start a run.

**claude** — 2026-08-30T04:45:00Z

Built (2026-08-30). Bring-your-own credentials, end to end: the sidecar's provider layer, the DAW's picker, and the Settings tab the producer sets a provider up in. All four checks pass — format clean, the changed files lint clean, a full build, and 594/594 under ctest — and the app runs.

## What landed

**The sidecar's model access.** `sidecar/src/credentials.ts` is Duet's own `CredentialStore` — pi's is in memory and its documentation says apps inject a persistent one — writing one owner-only JSON file at the path the DAW passes with `--credentials`. It keeps one thing more than pi's does: the set of providers the producer has set up, so a provider whose credential is removed stays in the picker with its models marked unusable rather than disappearing. `sidecar/src/access.ts` is `models.list` and the four `auth.*` methods over `Models`: the listing, an API key stored, a sign-in begun, a sign-in finished with the code the producer pasted, and a credential removed. `host.ts` registers them; `run.start` now waits for the `configure` still being answered, since the DAW sends the two back to back.

**The service, and when the model is sent.** `CollaboratorService::setModel` returns at once and asks the sidecar nothing; the service thread writes `configure` into the same connection immediately ahead of the `run.start` of the first run that needs it. That is what makes a switch take effect on the next run with no sidecar restart, what configures a sidecar that was replaced after dying, and what keeps the message thread from waiting on a socket to start a run. `ask` is the other half: one request-response for the setup surface, which does wait, because the producer is looking at the answer.

**The picker.** `duet::gui::ModelPicker` is paintless, with `ModelPicker::Source` standing for the provider layer, and `duet::app::ModelAccess` implements that over the service. The chosen model is app-global, under one settings key; what is in force is worked out at every refresh from what is authenticated now — the producer's own choice while it still resolves, then the recommended default when its provider is authenticated, then the first usable model of the listing. `Settings > Collaborator` is the surface: the model box, the provider box, a key, a sign-in with the address and instructions the sidecar returned, a code, and a removal. Nothing set up is the panel's setup state: the composer is held rather than offered as a send that would fail, and the button beside it opens that tab.

## Decisions a reviewer needs

1. **What `models.list` contains** (recorded in full in the design note above). pi ships 40 providers and ~1290 models; a provider contributes its models once it is *set up*, and every entry carries whether that provider's auth resolves now. Providers come in pi's registration order, models in the provider's own — Duet imposes no order, which is what "no provider is privileged" means. The listing also carries the providers themselves, because the setup surface has to offer one before there is a credential to list models from.

2. **Two protocol methods beyond the spec's table**, both inside the `auth.*` family the spec names: `auth.remove` (criterion 9 — the credential goes, the provider stays set up) and `auth.completeOAuth` (criterion 3 — pi's flows race a local callback server against a pasted code, and with no way to deliver the code a producer whose browser is elsewhere cannot finish). A code that arrives after the callback already won is treated as late, not wrong.

3. **The sidecar stays lazy.** The shell puts the stored choice in force at launch without asking the sidecar anything, and the Collaborator tab asks only when it becomes visible — so opening Settings on Interface still spawns nothing. Verified by hand: the app ran for 20 s with no `duet-sidecar` process alive.

4. **A failed `configure` leaves the session with no model**, so a run behind it fails plainly instead of quietly using the model the producer switched away from.

## How it is tested

- `tests/ModelAccessTests.cpp` — the protocol against the real sidecar, with no provider reachable: the offline script now registers a *locked twin* of the scripted provider that answers to nothing but a stored credential, so a key entered, a sign-in begun and finished, a wrong code, a credential removed, and persistence across a sidecar restart are all deterministic and cost nothing. It also holds the vertical case — the real picker on the real service on the real sidecar — and the case that a key lands in the credentials file and in nothing under a project folder.
- `tests/ModelPickerTests.cpp` — the view-model at its own seam: default resolution, the order kept, an unusable entry that cannot be chosen, the choice surviving a restart, the fallback when the credential under the chosen model goes, and a refusal surfacing as a plain message.
- `tests/TaskRunTests.cpp` — `configure` in front of `run.start`, a switch between runs on the same sidecar process, one configure for two runs on one model, and a replaced sidecar told the model again.
- `tests/gui/DialogTests.cpp` and `tests/gui/CollaboratorPanelCanvasTests.cpp` — the tab's rows reaching the picker, an unauthenticated entry offered and disabled, the sign-in address shown, and the panel's setup state holding the composer and leading to the tab.
- `tests/CollaboratorPanelTests.cpp` — the setup state at the panel's own seam.

## One thing found and left alone

The full lint sweep reports one error in `PluginScanDialog.cpp`, a file this
issue does not touch and that has not changed since zm174o landed it: a
`push_back` in a loop with no `reserve`. It is published as 1qdjq5 rather than
fixed here.
