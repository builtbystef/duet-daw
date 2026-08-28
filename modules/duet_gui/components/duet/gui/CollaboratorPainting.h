#pragma once

#include <duet/gui/Appearance.h>
#include <duet/gui/ArrangementView.h>
#include <duet/gui/Mixer.h>

#include <juce_gui_basics/juce_gui_basics.h>

/** The Collaborator's reserved marks, drawn in one place.

    The teal and the ✦ badge mean one thing in this interface, so what draws
    them is gathered here rather than spread across the surfaces they appear on:
    the timeline's ghost clips, the mixer's ghost handles and A/B chips, and the
    panel's own badge all come through this file. That is also what lets the
    reservation be a rule over the module sources — the surfaces that call these
    never name the token themselves.
*/
namespace duet::gui
{
/** The ✦ badge, drawn rather than typed: the four-pointed star is not a glyph
    every copy of the typeface has, and the badge has to read the same wherever
    Duet runs.
*/
void paintCollaboratorBadge (juce::Graphics& g, juce::Rectangle<float> area, juce::Colour ink);

/** One ghost clip on the timeline: a teal wash under a dashed teal border,
    three rings of soft glow around it, and the ✦ before its name — and, while
    it is being auditioned, the border solid and the wash and the name gone,
    the clip the Audition put there being under it already.

    A ghost of an Element the producer has unticked is drawn at the excluded
    intensity, which is what the drawing's own `intensity` carries.
*/
void paintGhostClip (juce::Graphics& g,
                     const Appearance& appearance,
                     const GhostClipDrawing& ghost,
                     juce::Rectangle<int> area);

/** The suggested level on a mixer strip: a teal line with a head on it, marking
    the place the Suggestion would put the fader.

    A line and not a second handle, and the height of the whole fader row rather
    than of the handle: a suggested level is often a decibel or two from the one
    the producer is at, and the two marks have to be tellable apart where they
    meet.
*/
void paintGhostFader (juce::Graphics& g,
                      const Appearance& appearance,
                      const GhostFaderDrawing& ghost,
                      juce::Rectangle<int> marker);

/** The A/B chip a strip carries while a Suggestion that changes it is being
    auditioned. The heard side is the bright one.
*/
void paintAuditionChip (juce::Graphics& g,
                        const Appearance& appearance,
                        const AuditionChip& chip,
                        juce::Rectangle<int> area);
} // namespace duet::gui
