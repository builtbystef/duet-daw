#include <duet/gui/PianoRollCanvas.h>

#include <duet/gui/GraphiteLookAndFeel.h>
#include <duet/gui/MainShell.h>
#include <duet/gui/Text.h>
#include <duet/gui/Tokens.h>
#include <duet/gui/Typography.h>

#include <algorithm>
#include <array>
#include <cmath>

namespace duet::gui
{
namespace
{
    constexpr double wheelNotch = 50.0 / 256.0;
}

PianoRollCanvas::PianoRollCanvas (Appearance& lookAndScale, PianoRoll& pianoRoll)
    : appearance (lookAndScale), view (pianoRoll)
{
    setComponentID (surfaceId::pianoRoll);
    setWantsKeyboardFocus (true);
    for (auto* control : { static_cast<juce::Component*> (&root),
                           static_cast<juce::Component*> (&scale),
                           static_cast<juce::Component*> (&noteLength),
                           static_cast<juce::Component*> (&fold) })
        addAndMakeVisible (*control);
    constexpr std::array<const char*, 12> pitchNames { "C",  "C#", "D",  "D#", "E",  "F",
                                                       "F#", "G",  "G#", "A",  "A#", "B" };
    for (int pitch = 0; pitch < 12; ++pitch)
        root.addItem (pitchNames.at (static_cast<std::size_t> (pitch)), pitch + 1);
    root.setSelectedId (1, juce::dontSendNotification);
    scale.addItem ("Chromatic", 1);
    scale.addItem ("Major", 2);
    scale.addItem ("Minor", 3);
    scale.setSelectedId (1, juce::dontSendNotification);
    noteLength.addItem ("1/4", 1);
    noteLength.addItem ("1/8", 2);
    noteLength.addItem ("1/16", 3);
    noteLength.setSelectedId (1, juce::dontSendNotification);
    const auto updateScale = [this]
    {
        auto chosen = Scale::chromatic;
        if (scale.getSelectedId() == 2)
            chosen = Scale::major;
        else if (scale.getSelectedId() == 3)
            chosen = Scale::minor;
        view.setScale (root.getSelectedId() - 1, chosen);
        repaint();
    };
    root.onChange = updateScale;
    scale.onChange = updateScale;
    noteLength.onChange = [this]
    {
        auto beats = 1.0;
        if (noteLength.getSelectedId() == 2)
            beats = 0.5;
        else if (noteLength.getSelectedId() == 3)
            beats = 0.25;
        view.setNewNoteLengthBeats (beats);
    };
    fold.onClick = [this]
    {
        view.setFolded (fold.getToggleState());
        repaint();
    };
    startTimerHz (30);
}

void PianoRollCanvas::paint (juce::Graphics& g)
{
    g.fillAll (toJuce (appearance.colour (ColourToken::surfaceCanvas)));
    const auto grid = gridArea();
    const auto keyboard = grid.withX (0).withWidth (appearance.scaled (keyboardWidth));

    g.setColour (toJuce (appearance.colour (ColourToken::surfaceRaised)));
    g.fillRect (getLocalBounds().removeFromTop (appearance.scaled (controlsHeight)));
    g.setColour (toJuce (appearance.colour (ColourToken::textPrimary)));
    g.setFont (interFont (appearance.scaled (typography::body), true));
    g.drawText (view.isOpen() ? utf8 ("Piano Roll — ") + juce::String { view.clipName() }
                              : utf8 ("Piano Roll — No MIDI clip"),
                getLocalBounds().removeFromTop (appearance.scaled (controlsHeight)).reduced (8, 0),
                juce::Justification::centredLeft);

    for (const auto& row : view.rows())
    {
        const auto y = grid.getY() + row.y;
        const auto rowArea = juce::Rectangle<int> { 0, y, getWidth(), row.height };
        const auto black = std::array { 1, 3, 6, 8, 10 };
        const auto pitchClass = row.pitch % 12;
        const auto isBlack = std::find (black.begin(), black.end(), pitchClass) != black.end();
        auto rowColour = ColourToken::surfaceCanvas;
        if (! row.inScale)
            rowColour = ColourToken::surfaceInteractive;
        else if (isBlack)
            rowColour = ColourToken::surfaceRaised;
        g.setColour (toJuce (appearance.colour (rowColour)));
        g.fillRect (rowArea);
        g.setColour (toJuce (appearance.colour (ColourToken::borderSubtle)));
        g.fillRect (rowArea.withY (rowArea.getBottom() - 1).withHeight (1));
        g.setColour (toJuce (appearance.colour (ColourToken::textMuted)));
        if (pitchClass == 0)
            g.drawText ("C" + juce::String { row.pitch / 12 - 1 },
                        keyboard.withY (y).withHeight (row.height).reduced (4, 0),
                        juce::Justification::centredLeft);
    }

    for (const auto& note : view.notes())
    {
        const juce::Rectangle<int> area { grid.getX() + note.x,
                                          grid.getY() + note.y + 1,
                                          note.width,
                                          std::max (1, view.keyHeightPx() - 2) };
        g.setColour (toJuce (appearance.colour (ColourToken::trackOrange)));
        g.fillRoundedRectangle (area.toFloat(), 2.0F);
        if (view.isNoteSelected (note.note))
        {
            g.setColour (toJuce (appearance.colour (ColourToken::textPrimary)));
            g.drawRoundedRectangle (area.toFloat(), 2.0F, 2.0F);
        }

        const auto velocity = velocityArea();
        const auto height = note.velocity * velocity.getHeight() / 127;
        g.fillRect (
            grid.getX() + note.x, velocity.getBottom() - height, std::max (3, note.width), height);
    }

    g.setColour (toJuce (appearance.colour (ColourToken::accentStrong)));
    g.fillRect (grid.getX() + view.playheadX(),
                grid.getY(),
                std::max (1, appearance.scaled (2)),
                grid.getHeight());
    if (rubberBanding)
    {
        g.setColour (toJuce (appearance.colour (ColourToken::textSecondary)));
        g.drawRect (rubberBand, 1);
    }
    g.setColour (toJuce (appearance.colour (ColourToken::borderDefault)));
    g.drawRect (getLocalBounds(), 1);
}

void PianoRollCanvas::resized()
{
    view.setWidthPx (std::max (0, getWidth() - appearance.scaled (keyboardWidth)));
    auto controls = getLocalBounds().removeFromTop (appearance.scaled (controlsHeight));
    controls.removeFromLeft (std::max (0, controls.getWidth() - appearance.scaled (330)));
    root.setBounds (controls.removeFromLeft (appearance.scaled (58)).reduced (2));
    scale.setBounds (controls.removeFromLeft (appearance.scaled (92)).reduced (2));
    noteLength.setBounds (controls.removeFromLeft (appearance.scaled (72)).reduced (2));
    fold.setBounds (controls.removeFromLeft (appearance.scaled (64)).reduced (2));
}

void PianoRollCanvas::mouseDown (const juce::MouseEvent& event)
{
    grabKeyboardFocus();
    const auto note = noteAt (event.getPosition());
    if (event.mods.isPopupMenu())
    {
        if (note.has_value())
        {
            view.clickNote (note->note, false, false);
            showNoteMenu();
        }
        return;
    }
    if (velocityArea().contains (event.getPosition()) && note.has_value())
    {
        if (! view.isNoteSelected (note->note))
            view.clickNote (note->note, false, false);
        velocityDragged = note->note;
    }
    else if (note.has_value())
    {
        view.clickNote (note->note, event.mods.isCtrlDown(), event.mods.isShiftDown());
        dragged = note->note;
        const auto right = gridArea().getX() + note->x + note->width;
        view.beginNoteGesture (dragged,
                               std::abs (event.x - right) <= appearance.scaled (5)
                                   ? NoteGestureKind::resizeRight
                                   : NoteGestureKind::move);
    }
    else if (gridArea().contains (event.getPosition()))
    {
        rubberStart = event.getPosition();
        rubberBand = { rubberStart.x, rubberStart.y, 0, 0 };
        rubberBanding = true;
        rubberExtends = event.mods.isCtrlDown();
        if (! rubberExtends)
            view.clearSelection();
    }
    repaint();
}

void PianoRollCanvas::mouseDoubleClick (const juce::MouseEvent& event)
{
    if (! gridArea().contains (event.getPosition()))
        return;
    if (const auto note = noteAt (event.getPosition()); note.has_value())
        view.removeNote (note->note);
    else
        (void) view.addNote (pitchAt (event.y), view.xToClipBeats (event.x - gridArea().getX()));
    repaint();
}

void PianoRollCanvas::mouseDrag (const juce::MouseEvent& event)
{
    dragging = true;
    if (dragged != duet::model::noNote)
        view.updateNoteGesture (view.xToClipBeats (event.x - gridArea().getX()),
                                pitchAt (event.y),
                                event.mods.isAltDown());
    else if (velocityDragged != duet::model::noNote)
    {
        const auto velocity = velocityArea();
        velocityTarget =
            127 - (event.y - velocity.getY()) * 126 / std::max (1, velocity.getHeight());
    }
    else if (rubberBanding)
    {
        rubberBand = { std::min (rubberStart.x, event.x),
                       std::min (rubberStart.y, event.y),
                       std::abs (event.x - rubberStart.x),
                       std::abs (event.y - rubberStart.y) };
    }
    repaint();
}

void PianoRollCanvas::mouseUp (const juce::MouseEvent& event)
{
    if (dragged != duet::model::noNote)
    {
        if (dragging)
        {
            view.updateNoteGesture (view.xToClipBeats (event.x - gridArea().getX()),
                                    pitchAt (event.y),
                                    event.mods.isAltDown());
            (void) view.completeNoteGesture();
        }
        else
            view.cancelNoteGesture();
    }
    if (velocityDragged != duet::model::noNote && dragging)
        view.setSelectedVelocity (velocityDragged, velocityTarget);

    if (rubberBanding)
    {
        std::vector<duet::model::NoteRef> intersected;
        const auto grid = gridArea();
        for (const auto& note : view.notes())
            if (rubberBand.intersects (juce::Rectangle<int> {
                    grid.getX() + note.x, grid.getY() + note.y, note.width, view.keyHeightPx() }))
                intersected.push_back (note.note);
        view.selectNotes (intersected, rubberExtends);
    }
    dragged = duet::model::noNote;
    velocityDragged = duet::model::noNote;
    rubberBanding = false;
    dragging = false;
    repaint();
}

void PianoRollCanvas::mouseWheelMove (const juce::MouseEvent& event,
                                      const juce::MouseWheelDetails& wheel)
{
    const auto notches = wheel.deltaY / wheelNotch;
    if (event.mods.isCtrlDown() && event.mods.isShiftDown())
        view.verticalZoom (std::pow (1.25, notches));
    else if (event.mods.isCtrlDown())
        view.geometry().zoomAt (event.x - gridArea().getX(), std::pow (1.25, notches));
    else if (event.mods.isShiftDown())
        view.geometry().scrollByPixels (static_cast<int> (std::lround (-notches * 60.0)));
    else
        view.scrollVertically (static_cast<int> (std::lround (-notches * 60.0)));
    repaint();
}

void PianoRollCanvas::timerCallback() { repaint(); }

std::optional<PianoNoteDrawing> PianoRollCanvas::noteAt (juce::Point<int> point) const
{
    const auto grid = gridArea();
    for (const auto& note : view.notes())
    {
        const juce::Rectangle<int> area {
            grid.getX() + note.x, grid.getY() + note.y, note.width, view.keyHeightPx()
        };
        const auto velocity = velocityArea();
        const auto velocityBar = juce::Rectangle<int> {
            grid.getX() + note.x, velocity.getY(), std::max (3, note.width), velocity.getHeight()
        };
        if (area.contains (point) || velocityBar.contains (point))
            return note;
    }
    return {};
}

int PianoRollCanvas::pitchAt (int y) const
{
    for (const auto& row : view.rows())
        if (y >= gridArea().getY() + row.y && y < gridArea().getY() + row.y + row.height)
            return row.pitch;
    return 60;
}

juce::Rectangle<int> PianoRollCanvas::gridArea() const
{
    auto area = getLocalBounds()
                    .withTrimmedTop (appearance.scaled (controlsHeight))
                    .withTrimmedBottom (appearance.scaled (velocityHeight));
    area.removeFromLeft (appearance.scaled (keyboardWidth));
    return area;
}

juce::Rectangle<int> PianoRollCanvas::velocityArea() const
{
    auto area = getLocalBounds().removeFromBottom (appearance.scaled (velocityHeight));
    area.removeFromLeft (appearance.scaled (keyboardWidth));
    return area;
}

void PianoRollCanvas::showNoteMenu()
{
    juce::PopupMenu menu;
    menu.addItem (1, "Delete");
    menu.addItem (2, "Quantize");
    menu.showMenuAsync ({},
                        [safe = juce::Component::SafePointer { this }] (int result)
                        {
                            if (safe == nullptr)
                                return;
                            if (result == 1)
                                safe->view.deleteSelected();
                            else if (result == 2)
                                safe->view.quantizeSelected();
                            safe->repaint();
                        });
}
} // namespace duet::gui
