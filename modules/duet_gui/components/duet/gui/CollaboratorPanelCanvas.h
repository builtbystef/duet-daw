#pragma once

#include <duet/gui/Appearance.h>
#include <duet/gui/CollaboratorPanel.h>

#include <juce_gui_basics/juce_gui_basics.h>

#include <functional>
#include <memory>

namespace duet::gui
{
/** The names the panel's own areas carry, so that a test can find one. */
namespace collaboratorId
{
    inline constexpr const char* conversation = "collaboratorConversation";
    inline constexpr const char* taskRun = "collaboratorTaskRun";
    inline constexpr const char* composer = "collaboratorComposer";
    inline constexpr const char* quickPrompts = "collaboratorQuickPrompts";
    inline constexpr const char* send = "collaboratorSend";
    inline constexpr const char* cancel = "collaboratorCancel";
} // namespace collaboratorId

/** The right dock: the surface the Collaborator speaks from.

    A scrolling conversation over a composer, quick-prompt chips between them,
    and the Task Run card above the composer while a run is on. It paints what
    the panel's view-model says and hands it the producer's gestures, and it is
    the one place in the interface the reserved teal and the ✦ badge are drawn.

    The selection the context chip records is not this panel's to know, so it
    asks for it: the shell answers from the one current selection.
*/
class CollaboratorPanelCanvas final : public juce::Component,
                                      private juce::Timer,
                                      private juce::FocusChangeListener,
                                      private Appearance::Listener
{
public:
    CollaboratorPanelCanvas (Appearance& lookAndScale, CollaboratorPanel& panelModel);
    ~CollaboratorPanelCanvas() override;

    [[nodiscard]] CollaboratorPanel& model() { return panel; }
    [[nodiscard]] const CollaboratorPanel& model() const { return panel; }

    /** Where the panel reads the current selection from. Asked for on every
        tick, so that a chip offered above the composer follows the producer's
        selection without the shell pushing it.
    */
    void setSelectionContextSource (std::function<SelectionContext()> currentSelection);

    /** Takes the panel's own state onto the screen: the conversation, the chips
        and the card. Called whenever any of them can have changed.
    */
    void refresh();

    /** What Escape in the composer does: hands the keyboard back to the surface
        the producer came from, and leaves what they have typed where it is.
    */
    void returnFocusFromComposer();

    /** The surface the keyboard goes back to, or nothing when the composer is
        the first thing that has had it.
    */
    [[nodiscard]] juce::Component* focusReturnSurface() const { return lastFocused; }

    /** Told whenever the keyboard moves, which is how the panel knows where to
        hand it back to. Public because a test drives it the same way the
        desktop does.
    */
    void globalFocusChanged (juce::Component* focused) override;

    void paint (juce::Graphics& g) override;
    void resized() override;

    /** The panel's own chrome, in logical units. */
    static constexpr int headerHeight = 28;
    static constexpr int taskCardHeight = 76;
    static constexpr int quickPromptRowHeight = 26;
    static constexpr int composerHeight = 66;

    /** How often the panel looks at itself: fast enough for the Task Run card's
        spinner while a run is on, and no faster than a selection poll needs
        when none is.
    */
    static constexpr int runningTickHz = 30;
    static constexpr int idleTickHz = 8;

private:
    class Conversation;
    class TaskRunCard;

    void timerCallback() override;

    /** A scale the producer has changed is a layout here, and not only a
        repaint: the panel's chrome is measured in logical units, and the dock
        it is in keeps the pixel width the producer dragged it to, so nothing
        else calls `resized()` for it.
    */
    void appearanceChanged() override;

    void sendComposer();
    void layOutQuickPrompts();

    Appearance& appearance;
    CollaboratorPanel& panel;
    std::function<SelectionContext()> selectionSource;

    juce::Viewport scroller;
    std::unique_ptr<Conversation> conversation;
    std::unique_ptr<TaskRunCard> card;
    juce::OwnedArray<juce::TextButton> promptChips;
    juce::TextEditor composer;
    juce::TextButton sendButton { "Send" };

    juce::Component::SafePointer<juce::Component> lastFocused;
    std::vector<std::string> shownPrompts;
    std::size_t shownEntries = 0;
    bool shownRunning = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CollaboratorPanelCanvas)
};
} // namespace duet::gui
