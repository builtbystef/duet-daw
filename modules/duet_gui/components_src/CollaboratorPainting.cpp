#include <duet/gui/CollaboratorPainting.h>

#include <duet/gui/GraphiteLookAndFeel.h>
#include <duet/gui/Suggestions.h>
#include <duet/gui/Tokens.h>
#include <duet/gui/Typography.h>

#include <array>

namespace duet::gui
{
namespace
{
    // Logical units: the interface scale is what turns one into a pixel.
    constexpr int ghostBadgeSize = 9;
    constexpr int ghostPadding = 4;
    constexpr float ghostBorderWidth = 1.2F;
    constexpr float dashLength = 4.0F;

    /** How much of the glow each ring carries, outermost first: three rings that
        fade, which is what makes the edge read as a glow rather than an outline.
    */
    constexpr float glowAlphaPerRing = 0.10F;

    void paintGlow (juce::Graphics& g,
                    juce::Colour teal,
                    juce::Rectangle<float> area,
                    float radius,
                    float intensity)
    {
        for (auto ring = Suggestions::glowRings; ring > 0; --ring)
        {
            const auto spread = static_cast<float> (ring);

            g.setColour (teal.withAlpha (glowAlphaPerRing * intensity / static_cast<float> (ring)));
            g.drawRoundedRectangle (area.expanded (spread), radius + spread, 1.0F);
        }
    }

    void strokeDashed (juce::Graphics& g, juce::Rectangle<float> area, float radius, float width)
    {
        juce::Path outline;
        juce::Path dashed;
        const std::array<float, 2> dashes { dashLength, dashLength };

        outline.addRoundedRectangle (area, radius);
        juce::PathStrokeType { width }.createDashedStroke (
            dashed, outline, dashes.data(), static_cast<int> (dashes.size()));
        g.fillPath (dashed);
    }
} // namespace

void paintCollaboratorBadge (juce::Graphics& g, juce::Rectangle<float> area, juce::Colour ink)
{
    const auto centre = area.getCentre();
    const auto arm = area.getWidth() / 2.0F;
    const auto waist = arm * 0.24F;

    juce::Path star;

    star.startNewSubPath (centre.x, centre.y - arm);
    star.lineTo (centre.x + waist, centre.y - waist);
    star.lineTo (centre.x + arm, centre.y);
    star.lineTo (centre.x + waist, centre.y + waist);
    star.lineTo (centre.x, centre.y + arm);
    star.lineTo (centre.x - waist, centre.y + waist);
    star.lineTo (centre.x - arm, centre.y);
    star.lineTo (centre.x - waist, centre.y - waist);
    star.closeSubPath();

    g.setColour (ink);
    g.fillPath (star);
}

void paintGhostClip (juce::Graphics& g,
                     const Appearance& appearance,
                     const GhostClipDrawing& ghost,
                     juce::Rectangle<int> area)
{
    const auto teal = toJuce (appearance.colour (ColourToken::collaborator));
    const auto intensity = static_cast<float> (ghost.intensity);
    const auto radius = static_cast<float> (appearance.scaled (metrics::radiusSmall));
    const auto bounds = area.toFloat().reduced (1.0F);

    paintGlow (g, teal, bounds, radius, intensity);

    // An auditioned ghost has the real clip under it and carries no wash, so
    // there is nothing to fill.
    if (ghost.fillAlpha > 0.0)
    {
        g.setColour (teal.withAlpha (static_cast<float> (ghost.fillAlpha) * intensity));
        g.fillRoundedRectangle (bounds, radius);
    }

    // The border says which state the ghost is in: dashed while it is only
    // proposed, solid while the producer is hearing it.
    g.setColour (teal.withAlpha (0.85F * intensity));

    if (ghost.auditioning)
        g.drawRoundedRectangle (bounds, radius, ghostBorderWidth);
    else
        strokeDashed (g, bounds, radius, ghostBorderWidth);

    auto inner = area.reduced (appearance.scaled (ghostPadding));

    if (inner.getWidth() <= 0)
        return;

    const auto badge = inner.removeFromLeft (appearance.scaled (ghostBadgeSize)).toFloat();

    paintCollaboratorBadge (g,
                            badge.withSizeKeepingCentre (badge.getWidth(), badge.getWidth()),
                            teal.withAlpha (intensity));

    inner.removeFromLeft (appearance.scaled (2));

    if (inner.getWidth() <= 0)
        return;

    // The clip under an auditioned ghost is already printing this name, so the
    // badge alone says whose it is.
    if (! ghost.auditioning)
    {
        g.setColour (toJuce (appearance.colour (ColourToken::textPrimary)).withAlpha (intensity));
        g.setFont (interFont (appearance.scaled (typography::eyebrow)));
        g.drawText (ghost.name, inner, juce::Justification::centredLeft, true);
    }

    if (! ghost.stale)
        return;

    // Stale keeps its own hue: it is a fact about the Suggestion rather than a
    // second meaning for the Collaborator's.
    g.setColour (toJuce (appearance.colour (ColourToken::semanticWarning)).withAlpha (intensity));
    g.setFont (interFont (appearance.scaled (typography::eyebrow), true));
    g.drawText (Suggestions::staleLabel, inner, juce::Justification::centredRight, true);
}

void paintGhostFader (juce::Graphics& g,
                      const Appearance& appearance,
                      const GhostFaderDrawing& ghost,
                      juce::Rectangle<int> marker)
{
    const auto teal = toJuce (appearance.colour (ColourToken::collaborator));
    const auto intensity = static_cast<float> (ghost.intensity);
    const auto bounds = marker.toFloat();
    const auto radius = bounds.getWidth() / 2.0F;

    paintGlow (g, teal, bounds, radius, intensity);

    g.setColour (teal.withAlpha ((ghost.auditioning ? 0.95F : 0.75F) * intensity));
    g.fillRoundedRectangle (bounds, radius);

    // A head on the line, so that the mark reads as pointing at a place on the
    // fader rather than as a stripe drawn across it.
    g.fillRoundedRectangle (
        bounds.withHeight (bounds.getWidth() * 2.0F)
            .withSizeKeepingCentre (bounds.getWidth() * 3.0F, bounds.getWidth() * 2.0F)
            .withY (bounds.getY()),
        radius);
}

void paintAuditionChip (juce::Graphics& g,
                        const Appearance& appearance,
                        const AuditionChip& chip,
                        juce::Rectangle<int> area)
{
    if (! chip.visible)
        return;

    const auto teal = toJuce (appearance.colour (ColourToken::collaborator));
    const auto radius = static_cast<float> (appearance.scaled (metrics::radiusSmall));

    g.setColour (teal.withAlpha (0.14F));
    g.fillRoundedRectangle (area.toFloat(), radius);
    g.setColour (teal.withAlpha (0.55F));
    g.drawRoundedRectangle (area.toFloat().reduced (0.5F), radius, 1.0F);

    auto sides = area;
    const auto current = sides.removeFromLeft (sides.getWidth() / 2);

    g.setFont (interFont (appearance.scaled (typography::eyebrow), true));

    // Which side is heard is the whole point of the chip, so the heard one is
    // the Collaborator's own hue and the other is muted text.
    g.setColour (chip.proposedHeard ? toJuce (appearance.colour (ColourToken::textMuted)) : teal);
    g.drawText (Suggestions::currentSideLabel, current, juce::Justification::centred, true);

    g.setColour (chip.proposedHeard ? teal : toJuce (appearance.colour (ColourToken::textMuted)));
    g.drawText (Suggestions::proposedSideLabel, sides, juce::Justification::centred, true);
}
} // namespace duet::gui
