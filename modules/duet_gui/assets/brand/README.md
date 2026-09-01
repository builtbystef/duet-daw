# Duet brand assets

The Duet mark (`duet-mark.svg`) and wordmark (`duet-wordmark.svg`): the charcoal
D beside the play triangle in the Collaborator's teal, and the same pair
spelling "Duet DAW".

Two colours, and what they mean:

| Colour         | Hex       | Meaning                                        |
| -------------- | --------- | ---------------------------------------------- |
| Charcoal Black | `#1C1F26` | The producer's half — print ink, not a token   |
| Muted Teal     | `#4AA294` | The Collaborator — the `collaborator` token    |

The teal is byte-identical to the `collaborator` colour token, and that is the
whole brand story: the interface reserves the hue for the Collaborator, and the
mark is the producer and the Collaborator side by side.

These files are compiled into the binary (`duet_gui_brand`) and drawn on
branding surfaces only — the window's icon and the About window. The charcoal is
re-inked to the palette's text colour where it sits on a themed surface; the
teal is never re-inked.
