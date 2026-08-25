#include <duet/gui/PluginEditorManager.h>

#include <duet/model/PluginEditorAccess.h>

#include <juce_audio_processors/juce_audio_processors.h>

#include <algorithm>
#include <atomic>
#include <map>
#include <utility>

namespace duet::gui
{
namespace
{
    std::optional<duet::model::PluginInfo> infoFor (duet::model::Session& session,
                                                    duet::model::PluginRef plugin)
    {
        for (const auto& track : session.tracks())
            for (const auto& candidate : track.plugins)
                if (candidate.plugin == plugin)
                    return candidate;
        for (const auto& candidate : session.master().plugins)
            if (candidate.plugin == plugin)
                return candidate;
        return std::nullopt;
    }

    std::string identityOf (const duet::model::PluginInfo& plugin)
    {
        if (! plugin.externalIdentifier.empty())
            return plugin.externalIdentifier;
        if (plugin.builtin.has_value())
            return "builtin:" + std::to_string (static_cast<int> (*plugin.builtin));
        return {};
    }

    juce::AudioProcessor* processorFor (duet::model::Session& session,
                                        duet::model::PluginRef plugin)
    {
        return duet::model::PluginEditorAccess::processorOf (session, plugin);
    }
} // namespace

struct PluginEditorManager::Impl final : private juce::Timer
{
    struct Window final : juce::DocumentWindow,
                          private juce::AudioProcessorListener,
                          private juce::AsyncUpdater
    {
        Window (Impl& owner,
                duet::model::PluginRef pluginRef,
                juce::AudioProcessor& hostedProcessor,
                duet::model::PluginInfo pluginInfo)
            : DocumentWindow (pluginInfo.name,
                              juce::Colours::darkgrey,
                              DocumentWindow::closeButton,
                              true),
              manager (owner), plugin (pluginRef), info (std::move (pluginInfo)),
              processor (hostedProcessor)
        {
            content = std::make_unique<juce::Component>();
            bypass.setButtonText ("Bypass");
            bypass.setToggleState (info.bypassed, juce::dontSendNotification);
            save.setButtonText ("Save Preset");
            preset.addItem ("Custom", 1);
            refreshPresets();

            if (processor.hasEditor())
                editor.reset (processor.createEditorAndMakeActive());
            if (editor == nullptr)
                editor = std::make_unique<juce::GenericAudioProcessorEditor> (processor);

            for (auto* child :
                 std::initializer_list<juce::Component*> { &bypass, &preset, &save, editor.get() })
                content->addAndMakeVisible (*child);

            bypass.onClick = [this]
            {
                if (manager.session != nullptr)
                    manager.session->performAction (
                        bypass.getToggleState() ? "Bypass Plugin" : "Enable Plugin",
                        [this] (auto& ops)
                        { ops.setPluginBypassed (plugin, bypass.getToggleState()); });
            };
            save.onClick = [this] { askForPresetName(); };
            preset.onChange = [this] { loadSelectedPreset(); };
            setContentOwned (content.release(), true);
            const auto width = std::max (420, editor->getWidth());
            const auto height = std::max (280, editor->getHeight()) + 34;
            centreWithSize (width, height);
            setUsingNativeTitleBar (true);
            setResizable (editor->isResizable(), false);
            processor.addListener (this);
            editor->addMouseListener (this, true);
            setVisible (true);
        }

        Window (const Window&) = delete;
        Window& operator= (const Window&) = delete;

        ~Window() override
        {
            editor->removeMouseListener (this);
            processor.removeListener (this);
            clearContentComponent();
        }

        void closeButtonPressed() override { manager.close (plugin); }

        void resized() override
        {
            DocumentWindow::resized();
            auto* contents = getContentComponent();
            if (contents == nullptr)
                return;
            auto area = contents->getLocalBounds();
            auto chrome = area.removeFromTop (34).reduced (4);
            bypass.setBounds (chrome.removeFromLeft (80));
            save.setBounds (chrome.removeFromRight (100));
            preset.setBounds (chrome.reduced (4, 0));
            editor->setBounds (area);
        }

        void refreshPresets()
        {
            preset.clear (juce::dontSendNotification);
            preset.addItem ("Custom", 1);
            const auto entries = manager.presets.presetsFor (identityOf (info));
            for (int index = 0; index < static_cast<int> (entries.size()); ++index)
                preset.addItem (entries[static_cast<std::size_t> (index)].name, index + 2);
            preset.setSelectedId (1, juce::dontSendNotification);
        }

        void askForPresetName()
        {
            auto prompt = std::make_unique<juce::AlertWindow> (
                "Save Plugin Preset", "Name this preset", juce::MessageBoxIconType::NoIcon);
            prompt->addTextEditor ("name", {});
            prompt->addButton ("Save", 1);
            prompt->addButton ("Cancel", 0);
            auto* raw = prompt.release();
            raw->enterModalState (
                true,
                juce::ModalCallbackFunction::create (
                    [safe = juce::Component::SafePointer<Window> { this }, raw] (int result)
                    {
                        std::unique_ptr<juce::AlertWindow> owned { raw };
                        if (safe == nullptr || result != 1 || safe->manager.session == nullptr)
                            return;
                        const auto name = owned->getTextEditorContents ("name").toStdString();
                        const auto identity = identityOf (safe->info);
                        const auto state = safe->manager.session->pluginOpaqueState (safe->plugin);
                        const auto saved =
                            safe->manager.presets.save (identity, name, state, false);
                        if (saved == SavePresetResult::alreadyExists)
                            safe->askToReplacePreset (name, state);
                        else if (saved == SavePresetResult::saved)
                            safe->refreshPresets();
                    }),
                true);
        }

        void askToReplacePreset (std::string name, std::string state)
        {
            const auto options = juce::MessageBoxOptions {}
                                     .withIconType (juce::MessageBoxIconType::QuestionIcon)
                                     .withTitle ("Replace preset?")
                                     .withMessage ("A preset with this name already exists.")
                                     .withButton ("Replace")
                                     .withButton ("Cancel");
            juce::AlertWindow::showAsync (options,
                                          [safe = juce::Component::SafePointer<Window> { this },
                                           name = std::move (name),
                                           state = std::move (state)] (int result)
                                          {
                                              if (safe != nullptr && result == 1)
                                              {
                                                  safe->manager.presets.save (
                                                      identityOf (safe->info), name, state, true);
                                                  safe->refreshPresets();
                                              }
                                          });
        }

        void loadSelectedPreset()
        {
            if (preset.getSelectedId() <= 1 || manager.session == nullptr)
                return;
            const auto selected =
                manager.presets.preset (identityOf (info), preset.getText().toStdString());
            if (! selected.has_value() || selected->formatVersion != 1)
                return;
            manager.session->performAction (
                "Load Plugin Preset",
                [&] (auto& ops) { ops.setPluginOpaqueState (plugin, selected->opaqueState); });
        }

        void mouseDown (const juce::MouseEvent& event) override
        {
            DocumentWindow::mouseDown (event);
            sawGestureBoundary.store (false, std::memory_order_relaxed);
            if (manager.session != nullptr)
                valuesAtMouseDown = manager.session->pluginParameters (plugin);
        }

        void mouseUp (const juce::MouseEvent& event) override
        {
            DocumentWindow::mouseUp (event);
            if (sawGestureBoundary.load (std::memory_order_relaxed) || manager.session == nullptr)
                return;
            const auto after = manager.session->pluginParameters (plugin);
            std::vector<duet::model::PluginParameterInfo> changed;
            for (const auto& parameter : after)
            {
                const auto before =
                    std::ranges::find (valuesAtMouseDown,
                                       parameter.parameterId,
                                       &duet::model::PluginParameterInfo::parameterId);
                if (before != valuesAtMouseDown.end()
                    && std::abs (before->value - parameter.value) >= 0.000001)
                    changed.push_back (parameter);
            }
            if (! changed.empty())
                manager.session->performAction (
                    "Set Plugin Parameter",
                    [&] (auto& ops)
                    {
                        for (const auto& parameter : changed)
                            ops.setPluginParameter (plugin, parameter.parameterId, parameter.value);
                    });
        }

        void
            audioProcessorParameterChanged ([[maybe_unused]] juce::AudioProcessor* changedProcessor,
                                            [[maybe_unused]] int parameterIndex,
                                            [[maybe_unused]] float newValue) override
        {
            presetDirty.store (true, std::memory_order_relaxed);
            triggerAsyncUpdate();
        }

        void audioProcessorChanged (
            [[maybe_unused]] juce::AudioProcessor* changedProcessor,
            [[maybe_unused]] const juce::AudioProcessorListener::ChangeDetails& details) override
        {
        }

        void audioProcessorParameterChangeGestureBegin (
            [[maybe_unused]] juce::AudioProcessor* changedProcessor,
            int index) override
        {
            sawGestureBoundary.store (true, std::memory_order_relaxed);
            gestureBegin.store (index, std::memory_order_release);
            triggerAsyncUpdate();
        }

        void audioProcessorParameterChangeGestureEnd (
            [[maybe_unused]] juce::AudioProcessor* changedProcessor,
            int index) override
        {
            gestureEnd.store (index, std::memory_order_release);
            triggerAsyncUpdate();
        }

        void handleAsyncUpdate() override
        {
            if (presetDirty.exchange (false, std::memory_order_relaxed))
                preset.setSelectedId (1, juce::dontSendNotification);

            const auto began = gestureBegin.exchange (-1, std::memory_order_acq_rel);
            if (began >= 0 && manager.session != nullptr)
            {
                const auto& parameters = valuesAtMouseDown.empty()
                                             ? manager.session->pluginParameters (plugin)
                                             : valuesAtMouseDown;
                if (began < static_cast<int> (parameters.size()))
                    gestureOriginal = parameters[static_cast<std::size_t> (began)].value;
            }

            const auto ended = gestureEnd.exchange (-1, std::memory_order_acq_rel);
            if (ended < 0 || manager.session == nullptr)
                return;
            const auto parameters = manager.session->pluginParameters (plugin);
            if (ended >= static_cast<int> (parameters.size()))
                return;
            const auto& parameter = parameters[static_cast<std::size_t> (ended)];
            if (std::abs (parameter.value - gestureOriginal) < 0.000001)
                return;
            manager.session->performAction (
                "Set Plugin Parameter",
                [&] (auto& ops)
                { ops.setPluginParameter (plugin, parameter.parameterId, parameter.value); });
        }

        Impl& manager;
        duet::model::PluginRef plugin;
        duet::model::PluginInfo info;
        juce::AudioProcessor& processor;
        std::unique_ptr<juce::Component> content;
        std::unique_ptr<juce::AudioProcessorEditor> editor;
        juce::ToggleButton bypass;
        juce::ComboBox preset;
        juce::TextButton save;
        std::atomic<int> gestureBegin { -1 };
        std::atomic<int> gestureEnd { -1 };
        std::atomic<bool> presetDirty { false };
        std::atomic<bool> sawGestureBoundary { false };
        std::vector<duet::model::PluginParameterInfo> valuesAtMouseDown;
        double gestureOriginal = 0.0;
    };

    explicit Impl (Settings& settings) : presets (settings) { startTimerHz (10); }

    Impl (const Impl&) = delete;
    Impl& operator= (const Impl&) = delete;

    ~Impl() override { stopTimer(); }

    void timerCallback() override
    {
        if (session == nullptr)
            return;
        for (auto iterator = windows.begin(); iterator != windows.end();)
            if (! infoFor (*session, iterator->first).has_value())
                iterator = windows.erase (iterator);
            else
                ++iterator;
    }

    void close (duet::model::PluginRef plugin) { windows.erase (plugin); }

    duet::model::Session* session = nullptr;
    PluginPresets presets;
    std::map<duet::model::PluginRef, std::unique_ptr<Window>> windows;
};

PluginEditorManager::PluginEditorManager (Settings& settings)
    : impl (std::make_unique<Impl> (settings))
{
}

PluginEditorManager::~PluginEditorManager() = default;

void PluginEditorManager::setSession (duet::model::Session* openProject)
{
    if (impl->session == openProject)
        return;
    closeAll();
    impl->session = openProject;
}

void PluginEditorManager::open (duet::model::PluginRef plugin)
{
    if (auto found = impl->windows.find (plugin); found != impl->windows.end())
    {
        found->second->toFront (true);
        return;
    }
    if (impl->session == nullptr)
        return;
    const auto info = infoFor (*impl->session, plugin);
    auto* processor = processorFor (*impl->session, plugin);
    if (! info.has_value() || processor == nullptr)
        return;
    impl->windows.emplace (plugin,
                           std::make_unique<Impl::Window> (*impl, plugin, *processor, *info));
}

void PluginEditorManager::closeAll() { impl->windows.clear(); }

std::size_t PluginEditorManager::openWindowCount() const { return impl->windows.size(); }

bool PluginEditorManager::isOpen (duet::model::PluginRef plugin) const
{
    return impl->windows.contains (plugin);
}
} // namespace duet::gui
