#include <duet/gui/MixerCanvas.h>

#include <duet/gui/CollaboratorPainting.h>
#include <duet/gui/GraphiteLookAndFeel.h>
#include <duet/gui/Tokens.h>
#include <duet/gui/Typography.h>

#include <algorithm>
#include <cmath>

namespace duet::gui
{
namespace
{
    constexpr int stripWidth = 132;
    constexpr int nameHeight = 24;
    constexpr int panTop = 28;
    constexpr int panHeight = 24;
    constexpr int buttonsTop = 56;
    constexpr int buttonHeight = 20;
    constexpr int faderTop = 82;
    constexpr int chainTop = 112;
    constexpr int pluginRowHeight = 20;
    constexpr int outputHeight = 22;
    constexpr int insertHeight = 22;

    juce::String dbText (double db)
    {
        if (db <= duet::model::silentDb)
            return "-inf dB";
        return juce::String { db, 1 } + " dB";
    }

    /** Where a level sits along a fader's track, in pixels across it. */
    [[nodiscard]] float alongTrack (double db, juce::Rectangle<int> track)
    {
        const auto along = static_cast<float> (
            (std::clamp (db, Mixer::faderMinimumDb, Mixer::faderMaximumDb) - Mixer::faderMinimumDb)
            / (Mixer::faderMaximumDb - Mixer::faderMinimumDb));

        return juce::jmap (
            along, static_cast<float> (track.getX()), static_cast<float> (track.getRight()));
    }
} // namespace

MixerCanvas::MixerCanvas (Appearance& lookAndScale,
                          Mixer& mixerModel,
                          std::function<void (duet::model::PluginRef)> openPluginEditor,
                          std::function<void()> mixerChanged)
    : appearance (lookAndScale), mixer (mixerModel), openEditor (std::move (openPluginEditor)),
      modelChanged (std::move (mixerChanged))
{
    setComponentID ("mixer");
    setWantsKeyboardFocus (true);
    startTimerHz (30);
}

int MixerCanvas::stripWidthPx() const { return appearance.scaled (stripWidth); }

juce::Rectangle<int> MixerCanvas::stripBounds (int index) const
{
    return { index * stripWidthPx() - horizontalOffsetPx, 0, stripWidthPx(), getHeight() };
}

int MixerCanvas::stripIndexAt (juce::Point<int> position) const
{
    return (position.x + horizontalOffsetPx) / std::max (1, stripWidthPx());
}

void MixerCanvas::paintSuggestion (juce::Graphics& g,
                                   duet::model::TrackRef channel,
                                   juce::Rectangle<int> track,
                                   juce::Rectangle<int> row,
                                   juce::Rectangle<int> chip)
{
    if (const auto ghost = mixer.ghostFader (channel); ghost.has_value())
    {
        // The mark is a line the height of the whole row rather than a second
        // handle beside the producer's own: a Suggestion of -3.0 dB over a
        // fader at -6.0 puts the two within a few pixels of each other, and
        // they still have to be tellable apart.
        const auto x = juce::roundToInt (alongTrack (ghost->db, track));

        paintGhostFader (g, appearance, *ghost, { x - 1, row.getY(), 3, row.getHeight() });
    }

    paintAuditionChip (g, appearance, mixer.auditionChip (channel), chip);
}

void MixerCanvas::paint (juce::Graphics& g)
{
    g.fillAll (toJuce (appearance.colour (ColourToken::surfaceDefault)));
    const auto all = mixer.strips();
    const auto font = interFont (appearance.scaled (typography::body));
    g.setFont (font);

    for (int index = 0; index < static_cast<int> (all.size()); ++index)
    {
        auto area = stripBounds (index);
        if (! area.intersects (getLocalBounds()))
            continue;
        const auto& strip = all[static_cast<std::size_t> (index)];
        g.setColour (toJuce (appearance.colour (ColourToken::borderDefault)));
        g.drawRect (area, 1);

        if (strip.colour.has_value())
        {
            g.setColour (toJuce (
                appearance.colour (trackColourToken (static_cast<std::size_t> (*strip.colour)))));
            g.fillRect (area.removeFromTop (appearance.scaled (3)));
        }

        auto name = stripBounds (index).withHeight (appearance.scaled (nameHeight)).reduced (4, 2);
        g.setColour (toJuce (appearance.colour (ColourToken::textPrimary)));
        g.drawFittedText (strip.name, name, juce::Justification::centred, 1);

        const auto panArea = stripBounds (index)
                                 .withTrimmedTop (appearance.scaled (panTop))
                                 .withHeight (appearance.scaled (panHeight))
                                 .reduced (8, 2);
        g.setColour (toJuce (appearance.colour (ColourToken::surfaceRaised)));
        g.fillRoundedRectangle (panArea.toFloat(), 3.0F);
        const auto panX = juce::jmap (static_cast<float> (strip.pan),
                                      -1.0F,
                                      1.0F,
                                      static_cast<float> (panArea.getX()),
                                      static_cast<float> (panArea.getRight()));
        g.setColour (toJuce (appearance.colour (ColourToken::textSecondary)));
        g.fillEllipse (panX - 3.0F, static_cast<float> (panArea.getCentreY() - 3), 6.0F, 6.0F);

        auto buttons = stripBounds (index)
                           .withTrimmedTop (appearance.scaled (buttonsTop))
                           .withHeight (appearance.scaled (buttonHeight))
                           .reduced (8, 1);
        const auto mute = buttons.removeFromLeft (buttons.getWidth() / 2).reduced (1);
        const auto solo = buttons.reduced (1);
        g.setColour (toJuce (appearance.colour (strip.muted ? ColourToken::semanticWarning
                                                            : ColourToken::surfaceRaised)));
        g.fillRect (mute);
        g.setColour (toJuce (appearance.colour (ColourToken::textPrimary)));
        g.drawText ("M", mute, juce::Justification::centred);
        if (strip.canSolo)
        {
            g.setColour (toJuce (appearance.colour (strip.soloed ? ColourToken::semanticWarning
                                                                 : ColourToken::surfaceRaised)));
            g.fillRect (solo);
            g.setColour (toJuce (appearance.colour (ColourToken::textPrimary)));
            g.drawText ("S", solo, juce::Justification::centred);
        }

        auto fader = stripBounds (index)
                         .withTrimmedTop (appearance.scaled (faderTop))
                         .withHeight (appearance.scaled (26))
                         .reduced (8, 2);
        auto track = fader.withTrimmedRight (appearance.scaled (26));
        g.setColour (toJuce (appearance.colour (ColourToken::meterTrack)));
        g.fillRect (track.withHeight (4).withCentre ({ track.getCentreX(), track.getCentreY() }));
        const auto normalised = static_cast<float> (
            (std::clamp (strip.volumeDb, Mixer::faderMinimumDb, Mixer::faderMaximumDb)
             - Mixer::faderMinimumDb)
            / (Mixer::faderMaximumDb - Mixer::faderMinimumDb));
        const auto handleX = juce::jmap (
            normalised, static_cast<float> (track.getX()), static_cast<float> (track.getRight()));
        g.setColour (toJuce (appearance.colour (ColourToken::textPrimary)));
        g.fillRect (juce::Rectangle<float> {
            handleX - 3.0F, static_cast<float> (track.getCentreY() - 8), 6.0F, 16.0F });

        // What a pending Suggestion would set this fader to, beside where the
        // producer's own fader is: the real one has not moved. The A/B chip
        // stands where the pan control is, because an Audition is a moment the
        // producer spends comparing rather than panning.
        paintSuggestion (g, strip.channel, track, fader, panArea);

        const auto meter = fader.removeFromRight (appearance.scaled (8));
        g.setColour (toJuce (appearance.colour (ColourToken::meterTrack)));
        g.fillRect (meter);
        const auto meterFraction = static_cast<float> (std::clamp (
            (strip.meterDb - duet::model::silentDb) / -duet::model::silentDb, 0.0, 1.0));
        g.setColour (toJuce (appearance.colour (ColourToken::semanticSuccess)));
        g.fillRect (meter.withTrimmedTop (
            static_cast<int> (static_cast<float> (meter.getHeight()) * (1.0F - meterFraction))));
        g.setColour (toJuce (appearance.colour (ColourToken::textSecondary)));
        g.drawText (
            dbText (strip.volumeDb), fader.expanded (0, 4), juce::Justification::centredLeft);

        auto row = stripBounds (index)
                       .withTrimmedTop (appearance.scaled (chainTop))
                       .withHeight (appearance.scaled (pluginRowHeight));
        for (const auto& plugin : strip.plugins)
        {
            if (row.getBottom() >= getHeight() - appearance.scaled (outputHeight + insertHeight))
                break;
            auto content = row.reduced (4, 1);
            g.setColour (toJuce (appearance.colour (ColourToken::surfaceRaised)));
            g.fillRect (content);
            g.setColour (toJuce (appearance.colour (plugin.bypassed ? ColourToken::textDisabled
                                                                    : ColourToken::textSecondary)));
            g.drawFittedText (plugin.name,
                              content.withTrimmedRight (appearance.scaled (28)),
                              juce::Justification::centredLeft,
                              1);
            g.drawText ("B x",
                        content.removeFromRight (appearance.scaled (28)),
                        juce::Justification::centred);
            row.translate (0, appearance.scaled (pluginRowHeight));
        }
        if (drag == Drag::plugin && draggedChannel == strip.channel && pluginDropPosition >= 0)
        {
            const auto markerY =
                appearance.scaled (chainTop + pluginDropPosition * pluginRowHeight);
            g.setColour (toJuce (appearance.colour (ColourToken::textPrimary)));
            g.fillRect (stripBounds (index).withY (markerY).withHeight (2).reduced (4, 0));
        }

        const auto output = stripBounds (index)
                                .removeFromBottom (appearance.scaled (outputHeight + insertHeight))
                                .removeFromTop (appearance.scaled (outputHeight))
                                .reduced (4, 1);
        g.setColour (toJuce (appearance.colour (ColourToken::textMuted)));
        g.drawFittedText (
            strip.canRoute ? "Output v" : "Main Output", output, juce::Justification::centred, 1);
        const auto insert =
            stripBounds (index).removeFromBottom (appearance.scaled (insertHeight)).reduced (4, 1);
        g.drawText ("+ Insert", insert, juce::Justification::centred);
    }
}

void MixerCanvas::resized()
{
    const auto count = mixer.strips().size();
    const auto first = static_cast<std::size_t> (horizontalOffsetPx / std::max (1, stripWidthPx()));
    const auto visible = static_cast<std::size_t> (getWidth() / std::max (1, stripWidthPx()) + 2);
    mixer.setVisibleRange (std::min (first, count), visible);
}

void MixerCanvas::timerCallback()
{
    if (! isShowing())
        return;
    mixer.sampleMeters (juce::Time::getMillisecondCounterHiRes() / 1000.0);
    repaint();
}

void MixerCanvas::mouseDown (const juce::MouseEvent& event)
{
    grabKeyboardFocus();
    const auto all = mixer.strips();
    const auto index = stripIndexAt (event.getPosition());
    if (index < 0 || index >= static_cast<int> (all.size()))
        return;
    const auto& strip = all[static_cast<std::size_t> (index)];
    const auto localY = event.y;
    draggedChannel = strip.channel;

    if (localY >= appearance.scaled (panTop) && localY < appearance.scaled (panTop + panHeight))
    {
        drag = Drag::pan;
        mixer.beginPanGesture (strip.channel);
        return;
    }
    if (localY >= appearance.scaled (faderTop) && localY < appearance.scaled (chainTop))
    {
        drag = Drag::fader;
        mixer.beginFaderGesture (strip.channel);
        return;
    }
    if (localY >= appearance.scaled (buttonsTop)
        && localY < appearance.scaled (buttonsTop + buttonHeight))
    {
        const auto within = event.x - stripBounds (index).getX();
        if (within < stripWidthPx() / 2)
            mixer.toggleMute (strip.channel);
        else if (strip.canSolo)
            mixer.toggleSolo (strip.channel);
        modelChanged();
        repaint (stripBounds (index));
        return;
    }
    if (localY >= getHeight() - appearance.scaled (outputHeight + insertHeight)
        && localY < getHeight() - appearance.scaled (insertHeight) && strip.canRoute)
    {
        showRoutingMenu (strip.channel, this);
        return;
    }
    if (localY >= getHeight() - appearance.scaled (insertHeight))
    {
        showInsertMenu (strip, this);
        return;
    }

    if (localY >= appearance.scaled (chainTop))
    {
        const auto pluginIndex =
            (localY - appearance.scaled (chainTop)) / appearance.scaled (pluginRowHeight);
        if (pluginIndex >= 0 && pluginIndex < static_cast<int> (strip.plugins.size()))
        {
            const auto& plugin = strip.plugins[static_cast<std::size_t> (pluginIndex)];
            const auto within = event.x - stripBounds (index).getX();
            if (within > stripWidthPx() - appearance.scaled (14))
                mixer.removePlugin (plugin.plugin);
            else if (within > stripWidthPx() - appearance.scaled (34))
                mixer.toggleBypass (plugin.plugin);
            else
            {
                drag = Drag::plugin;
                draggedPlugin = plugin.plugin;
                pluginDropPosition = pluginIndex;
            }
            modelChanged();
            repaint();
        }
    }
}

void MixerCanvas::mouseDrag (const juce::MouseEvent& event)
{
    const auto bounds = stripBounds (stripIndexAt (event.getPosition()));
    if (drag == Drag::fader)
    {
        const auto fraction =
            std::clamp (static_cast<double> (event.x - bounds.getX() - appearance.scaled (8))
                            / std::max (1, bounds.getWidth() - appearance.scaled (42)),
                        0.0,
                        1.0);
        mixer.dragFaderTo (draggedChannel,
                           Mixer::faderMinimumDb
                               + fraction * (Mixer::faderMaximumDb - Mixer::faderMinimumDb));
    }
    else if (drag == Drag::pan)
    {
        const auto fraction = std::clamp (static_cast<double> (event.x - bounds.getX())
                                              / std::max (1, bounds.getWidth()),
                                          0.0,
                                          1.0);
        mixer.dragPanTo (draggedChannel, fraction * 2.0 - 1.0);
    }
    else if (drag == Drag::plugin)
    {
        const auto all = mixer.strips();
        const auto index = stripIndexAt (event.getPosition());
        if (index >= 0 && index < static_cast<int> (all.size())
            && all[static_cast<std::size_t> (index)].channel == draggedChannel
            && event.y >= appearance.scaled (chainTop))
            pluginDropPosition = std::clamp (
                (event.y - appearance.scaled (chainTop)) / appearance.scaled (pluginRowHeight),
                0,
                static_cast<int> (all[static_cast<std::size_t> (index)].plugins.size()) - 1);
        else
            pluginDropPosition = -1;
    }
    repaint();
}

void MixerCanvas::mouseUp ([[maybe_unused]] const juce::MouseEvent& event)
{
    if (drag == Drag::fader)
        mixer.endFaderGesture (draggedChannel);
    else if (drag == Drag::pan)
        mixer.endPanGesture (draggedChannel);
    else if (drag == Drag::plugin && pluginDropPosition >= 0)
        mixer.reorderPlugin (draggedPlugin, pluginDropPosition);
    drag = Drag::none;
    draggedChannel = duet::model::noTrack;
    draggedPlugin = duet::model::noPlugin;
    pluginDropPosition = -1;
    modelChanged();
    repaint();
}

void MixerCanvas::mouseDoubleClick (const juce::MouseEvent& event)
{
    const auto all = mixer.strips();
    const auto index = stripIndexAt (event.getPosition());
    if (index < 0 || index >= static_cast<int> (all.size()))
        return;
    const auto& strip = all[static_cast<std::size_t> (index)];
    if (event.y >= appearance.scaled (faderTop) && event.y < appearance.scaled (chainTop))
        mixer.resetFader (strip.channel);
    else if (event.y >= appearance.scaled (panTop)
             && event.y < appearance.scaled (panTop + panHeight))
        mixer.resetPan (strip.channel);
    else if (event.y >= appearance.scaled (chainTop))
    {
        const auto pluginIndex =
            (event.y - appearance.scaled (chainTop)) / appearance.scaled (pluginRowHeight);
        if (pluginIndex >= 0 && pluginIndex < static_cast<int> (strip.plugins.size()) && openEditor)
            openEditor (strip.plugins[static_cast<std::size_t> (pluginIndex)].plugin);
    }
    repaint();
}

void MixerCanvas::mouseWheelMove ([[maybe_unused]] const juce::MouseEvent& event,
                                  const juce::MouseWheelDetails& wheel)
{
    const auto contentWidth = static_cast<int> (mixer.strips().size()) * stripWidthPx();
    horizontalOffsetPx = std::clamp (horizontalOffsetPx - static_cast<int> (wheel.deltaY * 240.0F),
                                     0,
                                     std::max (0, contentWidth - getWidth()));
    resized();
    repaint();
}

bool MixerCanvas::keyPressed (const juce::KeyPress& key)
{
    if (key.getKeyCode() != juce::KeyPress::escapeKey)
        return false;
    mixer.cancelGesture();
    drag = Drag::none;
    draggedPlugin = duet::model::noPlugin;
    pluginDropPosition = -1;
    repaint();
    return true;
}

void MixerCanvas::showRoutingMenu (duet::model::TrackRef channel, juce::Component* target)
{
    juce::PopupMenu menu;
    const auto choices = mixer.routingDestinations (channel);
    for (int index = 0; index < static_cast<int> (choices.size()); ++index)
        menu.addItem (index + 1, choices[static_cast<std::size_t> (index)].name);
    menu.showMenuAsync (
        juce::PopupMenu::Options {}.withTargetComponent (target),
        [safe = juce::Component::SafePointer<MixerCanvas> { this }, channel, choices] (int result)
        {
            if (safe != nullptr && result > 0 && result <= static_cast<int> (choices.size()))
            {
                safe->mixer.setOutput (channel,
                                       choices[static_cast<std::size_t> (result - 1)].channel);
                safe->modelChanged();
            }
        });
}

void MixerCanvas::showInsertMenu (const MixerStrip& strip, juce::Component* target)
{
    juce::PopupMenu menu;
    const auto position = static_cast<int> (strip.plugins.size());
    menu.addItem (1, "EQ");
    menu.addItem (2, "Compressor");
    menu.addItem (3, "Reverb");
    if (strip.kind == duet::model::TrackKind::midi && strip.channel != duet::model::masterChannel)
    {
        menu.addItem (4, "4OSC");
        menu.addItem (5, "Sampler");
    }
    const auto vst3 = mixer.availableVst3For (strip.channel);
    if (! vst3.empty())
    {
        juce::PopupMenu external;
        for (int index = 0; index < static_cast<int> (vst3.size()); ++index)
            external.addItem (1000 + index, vst3[static_cast<std::size_t> (index)].name);
        menu.addSubMenu ("VST3", external);
    }
    menu.showMenuAsync (
        juce::PopupMenu::Options {}.withTargetComponent (target),
        [safe = juce::Component::SafePointer<MixerCanvas> { this }, strip, position, vst3] (
            int result)
        {
            if (safe == nullptr || result <= 0)
                return;
            if (result >= 1000 && result - 1000 < static_cast<int> (vst3.size()))
                safe->mixer.addVst3 (strip.channel,
                                     vst3[static_cast<std::size_t> (result - 1000)].identifier,
                                     position);
            else if (result <= 5)
                safe->mixer.addBuiltin (
                    strip.channel, static_cast<duet::model::BuiltinPlugin> (result - 1), position);
            safe->modelChanged();
            safe->repaint();
        });
}
} // namespace duet::gui
