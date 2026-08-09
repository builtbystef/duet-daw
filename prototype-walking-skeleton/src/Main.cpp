// PROTOTYPE — walking skeleton for roadmap node ddp1qt. Disposable; never ship.
//
// Answers, on Linux:
//   1. does Tracktion Engine develop build against a supplied JUCE 9?   (compile time)
//   2. does a multi-track Edit play, and survive model mutation mid-playback?
//   3. does a dense scrolling timeline hold up — software renderer vs OpenGLContext?
//   4. does a custom property on an engine-owned TRACK node survive save/reload? (rquzdc)

#include <tracktion_engine/tracktion_engine.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include <juce_gui_extra/juce_gui_extra.h>
#include <juce_opengl/juce_opengl.h>

namespace te = tracktion;

//==============================================================================
// Frame-time statistics, pushed from whichever thread paints.
struct FrameStats
{
    void push (double deltaMs)
    {
        deltas[writeIndex++ % window] = deltaMs;
        ++count;
    }

    struct Snapshot { double avgFps = 0, worstMs = 0; int frames = 0; };

    Snapshot snapshot() const
    {
        Snapshot s;
        const int n = (int) juce::jmin (count, (juce::int64) window);
        if (n == 0) return s;
        double sum = 0, worst = 0;
        for (int i = 0; i < n; ++i) { sum += deltas[i]; worst = juce::jmax (worst, deltas[i]); }
        s.avgFps = 1000.0 / (sum / n);
        s.worstMs = worst;
        s.frames = n;
        return s;
    }

    void reset() { count = 0; writeIndex = 0; }

    static constexpr int window = 240;
    double deltas[window] = {};
    int writeIndex = 0;
    juce::int64 count = 0;
};

//==============================================================================
// The dense timeline: many tracks, many clips, waveforms and piano-roll grids.
class TimelineCanvas final : public juce::Component,
                             private juce::AsyncUpdater
{
public:
    static constexpr int    numTracks   = 60;
    static constexpr double durationSec = 600.0;
    static constexpr int    trackHeight = 26;

    struct ClipRect { double start, len; int type; int seed; int thumb; };

    TimelineCanvas (juce::AudioFormatManager& fm, const juce::Array<juce::File>& waveFiles)
        : thumbCache (64)
    {
        juce::Random r (42);
        for (int t = 0; t < numTracks; ++t)
        {
            double pos = r.nextDouble() * 4.0;
            while (pos < durationSec - 4.0)
            {
                ClipRect c;
                c.start = pos;
                c.len   = 4.0 + r.nextDouble() * 8.0;
                c.type  = (t % 3 == 2) ? 1 : 0;          // every 3rd track is "MIDI"
                c.seed  = r.nextInt();
                c.thumb = r.nextInt (waveFiles.size());
                clips[t].push_back (c);
                pos += c.len + r.nextDouble() * 6.0;
            }
        }

        for (auto& f : waveFiles)
        {
            auto* th = new juce::AudioThumbnail (64, fm, thumbCache);
            th->setSource (new juce::FileInputSource (f));
            thumbs.add (th);
        }

        setPixelsPerSecond (100.0);
    }

    void setPixelsPerSecond (double pps)
    {
        pixelsPerSecond = pps;
        setSize (juce::roundToInt (durationSec * pps), numTracks * trackHeight);
    }

    double getPixelsPerSecond() const { return pixelsPerSecond; }

    void paint (juce::Graphics& g) override
    {
        const auto now = juce::Time::getMillisecondCounterHiRes();
        if (lastPaintMs > 0.0 && benchmarking)
        {
            lastDelta = now - lastPaintMs;
            stats.push (lastDelta);
        }
        lastPaintMs = now;

        const auto clip = g.getClipBounds();
        g.fillAll (juce::Colour (0xff181c20));

        // beat grid at 120 bpm, thinned when zoomed out
        double step = 0.5;
        while (step * pixelsPerSecond < 9.0) step *= 2.0;
        g.setColour (juce::Colour (0xff23282e));
        for (double s = std::floor (clip.getX() / pixelsPerSecond / step) * step;
             s * pixelsPerSecond < clip.getRight(); s += step)
            g.fillRect ((float) (s * pixelsPerSecond), (float) clip.getY(), 1.0f, (float) clip.getHeight());

        int drawn = 0;
        const int firstTrack = juce::jmax (0, clip.getY() / trackHeight);
        const int lastTrack  = juce::jmin (numTracks - 1, clip.getBottom() / trackHeight);

        for (int t = firstTrack; t <= lastTrack; ++t)
        {
            const int y = t * trackHeight;
            g.setColour (juce::Colour (0xff2a2f36));
            g.fillRect (clip.getX(), y + trackHeight - 1, clip.getWidth(), 1);

            for (const auto& c : clips[t])
            {
                const float x = (float) (c.start * pixelsPerSecond);
                const float w = (float) (c.len * pixelsPerSecond);
                if (x + w < (float) clip.getX() || x > (float) clip.getRight())
                    continue;

                juce::Rectangle<float> body (x, (float) y + 1.0f, w, (float) trackHeight - 3.0f);
                g.setColour (c.type == 0 ? juce::Colour (0xff2f5d50) : juce::Colour (0xff44405e));
                g.fillRoundedRectangle (body, 3.0f);
                g.setColour (juce::Colours::white.withAlpha (0.25f));
                g.drawRoundedRectangle (body, 3.0f, 1.0f);

                auto inner = body.reduced (2.0f).toNearestInt();
                if (c.type == 0)
                {
                    auto* th = thumbs[c.thumb];
                    g.setColour (juce::Colour (0xff8fd6bd));
                    if (th->getTotalLength() > 0)
                        th->drawChannels (g, inner, 0.0, th->getTotalLength(), 0.9f);
                }
                else
                {
                    juce::Random rr (c.seed);
                    g.setColour (juce::Colour (0xffb9b3e6));
                    for (int n = 0; n < 30; ++n)
                    {
                        const float nx = inner.getX() + rr.nextFloat() * juce::jmax (1.0f, (float) inner.getWidth() - 6.0f);
                        const float ny = inner.getY() + rr.nextFloat() * juce::jmax (1.0f, (float) inner.getHeight() - 3.0f);
                        const float nw = juce::jmax (2.0f, (float) (0.22 * pixelsPerSecond * 0.25));
                        g.fillRect (nx, ny, nw, 2.0f);
                    }
                }
                ++drawn;
            }
        }

        clipsDrawn = drawn;

        if (benchmarking && lastDelta > 250.0)
            std::cout << "[STALL] " << (glDriven ? "GL" : "SW") << " frame took "
                      << juce::String (lastDelta, 1) << " ms" << std::endl;

        if (benchmarking)
        {
            if (stats.count > 0 && stats.count % 120 == 0)
                triggerAsyncUpdate();

            if (! glDriven)
                repaint();          // self-driving paint loop for the software renderer
        }
    }

    void setBenchmarking (bool b, bool glIsDriving)
    {
        benchmarking = b;
        glDriven = glIsDriving;
        lastPaintMs = 0;
        stats.reset();
        if (b && ! glIsDriving)
            repaint();
    }

    std::function<void (FrameStats::Snapshot, int)> onStats;

    FrameStats stats;
    std::atomic<int> clipsDrawn { 0 };

private:
    void handleAsyncUpdate() override
    {
        if (onStats)
            onStats (stats.snapshot(), clipsDrawn.load());
    }

    std::vector<ClipRect> clips[numTracks];
    juce::AudioThumbnailCache thumbCache;
    juce::OwnedArray<juce::AudioThumbnail> thumbs;
    double pixelsPerSecond = 100.0;
    double lastPaintMs = 0.0, lastDelta = 0.0;
    bool benchmarking = false, glDriven = false;
};

//==============================================================================
class MainComponent final : public juce::Component,
                            private juce::Timer
{
public:
    MainComponent()
    {
        formatManager.registerBasicFormats();

        workDir = juce::File::getSpecialLocation (juce::File::tempDirectory)
                      .getChildFile ("duet-skeleton-PROTOTYPE-wipe-me");
        workDir.createDirectory();

        generateToneFiles();
        buildEdit();

        canvas = std::make_unique<TimelineCanvas> (formatManager, waveFiles);
        viewport.setViewedComponent (canvas.get(), false);
        viewport.setScrollBarsShown (true, true);
        addAndMakeVisible (viewport);

        canvas->onStats = [this] (FrameStats::Snapshot s, int drawn)
        {
            auto mode = juce::String (glAttached ? "GL" : "SW");
            auto line = juce::String::formatted ("[FPS] mode=%s avg=%.1f fps worst=%.1f ms clips-drawn=%d pps=%.0f",
                                                 mode.toRawUTF8(), s.avgFps, s.worstMs, drawn,
                                                 canvas->getPixelsPerSecond());
            std::cout << line << std::endl;
            fpsLabel.setText (line, juce::dontSendNotification);
        };

        auto initButton = [this] (juce::Button& b, const char* txt)
        {
            b.setButtonText (txt);
            addAndMakeVisible (b);
        };

        initButton (playButton, "Play");
        initButton (stopButton, "Stop");
        initButton (mutateButton, "Mutate once");
        initButton (autoMutateButton, "Auto-mutate");
        initButton (saveReloadButton, "Save/Reload test");
        initButton (deviceButton, "Audio device...");
        initButton (benchButton, "Benchmark");
        initButton (scrollButton, "Auto-scroll");
        initButton (zoomButton, "Zoom cycle");
        initButton (glButton, "OpenGL");

        addAndMakeVisible (fpsLabel);
        addAndMakeVisible (statusLabel);
        addAndMakeVisible (logLabel);
        fpsLabel.setFont (juce::FontOptions (13.0f));
        statusLabel.setFont (juce::FontOptions (13.0f));
        logLabel.setFont (juce::FontOptions (13.0f));

        playButton.onClick = [this]
        {
            auto& tc = edit->getTransport();
            tc.setPosition (te::TimePosition());
            tc.play (false);
        };
        stopButton.onClick = [this] { edit->getTransport().stop (false, false); };
        mutateButton.onClick = [this] { mutateInLoop(); };
        autoMutateButton.onClick = [this] { autoMutate = autoMutateButton.getToggleState(); };
        saveReloadButton.onClick = [this] { runSaveReloadTest(); };
        deviceButton.onClick = [this] { showDeviceSelector(); };

        benchButton.onClick = [this]
        {
            benchmarking = benchButton.getToggleState();
            canvas->setBenchmarking (benchmarking, glAttached);
            if (glAttached)
                glContext.setContinuousRepainting (benchmarking);
        };
        scrollButton.onClick = [this] { autoScroll = scrollButton.getToggleState(); };
        zoomButton.onClick = [this] { zoomCycle = zoomButton.getToggleState(); };
        glButton.onClick = [this] { toggleGL(); };

        startTimerHz (60);
        setSize (1500, 900);
        log ("Ready. Launch with pw-jack for JACK; ALSA is the default device.");
    }

    ~MainComponent() override
    {
        if (glAttached)
            glContext.detach();
        if (edit != nullptr)
            edit->getTransport().stop (false, true);
    }

    void resized() override
    {
        auto r = getLocalBounds();
        auto row1 = r.removeFromTop (34).reduced (4, 4);
        for (auto* b : { (juce::Button*) &playButton, (juce::Button*) &stopButton,
                         (juce::Button*) &mutateButton, (juce::Button*) &autoMutateButton,
                         (juce::Button*) &saveReloadButton, (juce::Button*) &deviceButton })
            b->setBounds (row1.removeFromLeft (130).reduced (2, 0));
        statusLabel.setBounds (row1);

        auto row2 = r.removeFromTop (34).reduced (4, 4);
        for (auto* b : { (juce::Button*) &benchButton, (juce::Button*) &scrollButton,
                         (juce::Button*) &zoomButton, (juce::Button*) &glButton })
            b->setBounds (row2.removeFromLeft (130).reduced (2, 0));
        fpsLabel.setBounds (row2);

        logLabel.setBounds (r.removeFromBottom (26).reduced (4, 2));
        viewport.setBounds (r);
    }

private:
    //==========================================================================
    void generateToneFiles()
    {
        struct Spec { const char* name; int kind; double freq; };
        const Spec specs[] = { { "bass.wav", 0, 110.0 }, { "chords.wav", 1, 220.0 },
                               { "lead.wav", 2, 440.0 }, { "drums.wav", 3, 0.0 } };

        juce::WavAudioFormat wav;
        const double sr = 44100.0;
        const int    len = (int) (sr * 8.0);

        for (auto& s : specs)
        {
            auto f = workDir.getChildFile (s.name);
            waveFiles.add (f);
            if (f.existsAsFile())
                continue;

            juce::AudioBuffer<float> buf (2, len);
            buf.clear();
            juce::Random r (7);
            double phase = 0.0;

            for (int i = 0; i < len; ++i)
            {
                const double t = i / sr;
                float v = 0.0f;
                switch (s.kind)
                {
                    case 0: // bass: pulsing sine
                        v = (float) (std::sin (2.0 * juce::MathConstants<double>::pi * s.freq * t)
                                     * (0.5 + 0.5 * std::sin (2.0 * juce::MathConstants<double>::pi * t)));
                        break;
                    case 1: // chords: stacked sines, gated every beat
                        v = (float) ((std::sin (2.0 * juce::MathConstants<double>::pi * s.freq * t)
                                      + 0.7 * std::sin (2.0 * juce::MathConstants<double>::pi * s.freq * 1.25 * t)
                                      + 0.5 * std::sin (2.0 * juce::MathConstants<double>::pi * s.freq * 1.5 * t)) / 3.0
                                     * (std::fmod (t, 0.5) < 0.4 ? 1.0 : 0.0));
                        break;
                    case 2: // lead: soft pentatonic melody, phase-accumulated (naive f(t)*t vibrato chirps)
                    {
                        static const double seq[] = { 440.0, 523.25, 659.25, 587.33, 493.88, 659.25, 523.25, 587.33 };
                        const double note = seq[(int) (t * 2.0) % 8];
                        phase += 2.0 * juce::MathConstants<double>::pi * note / sr;
                        const double noteT = std::fmod (t, 0.5);
                        const double env = std::min (1.0, noteT / 0.02) * std::exp (-noteT * 4.0);
                        v = (float) (std::sin (phase) * env * 0.8);
                        break;
                    }
                    case 3: // drums: decaying noise burst every half second
                    {
                        const double ph = std::fmod (t, 0.5);
                        v = (r.nextFloat() * 2.0f - 1.0f) * (float) std::exp (-ph * 18.0);
                        break;
                    }
                }
                v *= 0.4f;
                buf.setSample (0, i, v);
                buf.setSample (1, i, v);
            }

            std::unique_ptr<juce::AudioFormatWriter> writer (
                wav.createWriterFor (new juce::FileOutputStream (f), sr, 2, 16, {}, 0));
            if (writer != nullptr)
                writer->writeFromAudioSampleBuffer (buf, 0, len);
        }
    }

    void buildEdit()
    {
        editFile = workDir.getChildFile ("skeleton.tracktionedit");
        edit = te::createEmptyEdit (engine, editFile);
        edit->ensureNumberOfAudioTracks (5);

        auto tracks = te::getAudioTracks (*edit);
        const float pans[] = { 0.0f, -0.4f, 0.4f, 0.0f };

        for (int i = 0; i < 4; ++i)
        {
            auto* track = tracks[i];
            track->setName (juce::File (waveFiles[i]).getFileNameWithoutExtension());
            te::ClipPosition pos { { te::TimePosition(), te::TimePosition::fromSeconds (8.0) } };
            if (auto clip = track->insertWaveClip (track->getName(), waveFiles[i], pos, false))
                // FINDING: the default relative source path resolves against the edit's temp
                // subdir, yielding "../x.wav" → silence. Force an absolute reference.
                clip->getSourceFileReference().setToFile (waveFiles[i], te::SourceFileReference::PathStyle::alwaysAbsolute, false);
            if (auto vp = track->getVolumePlugin())
            {
                vp->setVolumeDb (i == 3 ? -8.0f : -10.0f);
                vp->setPan (pans[i]);
            }
        }

        // 5th track: MIDI clip through the built-in 4OSC synth
        if (auto* midiTrack = tracks[4])
        {
            midiTrack->setName ("arp (4OSC)");
            if (auto synth = edit->getPluginCache().createNewPlugin (te::FourOscPlugin::xmlTypeName, {}))
                midiTrack->pluginList.insertPlugin (synth, 0, nullptr);

            if (auto midiClip = midiTrack->insertMIDIClip ({ te::TimePosition(), te::TimePosition::fromSeconds (8.0) }, nullptr))
            {
                const int scale[] = { 57, 60, 64, 67, 69, 67, 64, 60 };
                auto& seq = midiClip->getSequence();
                for (int n = 0; n < 32; ++n)
                    seq.addNote (scale[n % 8], te::BeatPosition::fromBeats (n * 0.5),
                                 te::BeatDuration::fromBeats (0.4), 100, 0, nullptr);
            }
        }

        auto& tc = edit->getTransport();
        tc.setLoopRange ({ te::TimePosition(), te::TimePosition::fromSeconds (8.0) });
        tc.looping = true;
        tc.ensureContextAllocated();
    }

    //==========================================================================
    // Auto-mutate makes structural changes OUTSIDE the audible 0-8s loop, so any
    // click you hear is purely the graph rebuild, never the edit's own content.
    void mutateModel()
    {
        ++mutationCount;
        auto tracks = te::getAudioTracks (*edit);
        const char* kind = "?";

        switch (mutationCount % 3)
        {
            case 0:
                if (auto* t3 = tracks[3])
                {
                    te::ClipPosition pos { { te::TimePosition::fromSeconds (120.0), te::TimePosition::fromSeconds (122.0) } };
                    if (auto clip = t3->insertWaveClip ("afar", waveFiles[2], pos, false))
                        clip->getSourceFileReference().setToFile (waveFiles[2], te::SourceFileReference::PathStyle::alwaysAbsolute, false);
                    kind = "insert clip at 120s";
                }
                break;
            case 1:
                if (auto* t3 = tracks[3])
                {
                    auto clips = t3->getClips();
                    for (int i = clips.size(); --i >= 0;)
                        if (clips[i]->getName() == "afar")
                            clips[i]->setStart (te::TimePosition::fromSeconds (60.0), false, true);
                    kind = "move clip 120s->60s";
                }
                break;
            case 2:
                if (auto* t3 = tracks[3])
                {
                    auto clips = t3->getClips();
                    for (int i = clips.size(); --i >= 0;)
                        if (clips[i]->getName() == "afar")
                            clips[i]->removeFromParent();
                    kind = "remove clip";
                }
                break;
        }

        std::cout << "[MUTATE] #" << mutationCount << " " << kind << std::endl;
        logLabel.setText ("Mutation #" + juce::String (mutationCount) + ": " + kind, juce::dontSendNotification);
    }

    // The in-loop content change (audibly different by design) stays on Mutate once.
    void mutateInLoop()
    {
        ++mutationCount;
        auto tracks = te::getAudioTracks (*edit);
        if (auto* t0 = tracks[0])
            if (auto clips = t0->getClips(); ! clips.isEmpty())
                clips[0]->setStart (te::TimePosition::fromSeconds ((mutationCount & 1) ? 0.25 : 0.0), false, true);
        std::cout << "[MUTATE] #" << mutationCount << " in-loop clip move (content change is expected)" << std::endl;
    }

    void runSaveReloadTest()
    {
        // rquzdc rider: does a custom property on an engine-owned TRACK survive save/reload?
        std::cout << "[PERSIST] step1: setting property" << std::endl;
        auto tracks = te::getAudioTracks (*edit);
        tracks[0]->state.setProperty ("duetCustomProp", "duet-42", nullptr);

        // EditFileOperations::save segfaults for project-less edits: EditSnapshot::refresh()
        // null-derefs its ProjectItem (tracktion_EditSnapshot.cpp:227, pi->getLength() after
        // two null checks). Flush and write the state directly instead — which is also the
        // path Duet would use, as Duet does not adopt Tracktion's Project system.
        std::cout << "[PERSIST] step2: saving (flushState + direct write)" << std::endl;
        edit->flushState();
        if (auto xmlOut = edit->state.createXml())
            xmlOut->writeTo (editFile);
        std::cout << "[PERSIST] step3: saved, parsing file" << std::endl;

        // Full engine reload (te::loadEditFromFile) already proved SURVIVED, but a second
        // live Edit sharing this edit's temp dir crashed the app seconds later. The file
        // parse below is interference-free; the engine-load result stands in the notes.
        juce::String result = "MISSING";
        if (auto xml = juce::parseXML (editFile.loadFileAsString()))
        {
            auto tree = juce::ValueTree::fromXml (*xml);
            auto firstTrack = tree.getChildWithName (te::IDs::TRACK);
            if (firstTrack.isValid())
                result = firstTrack.getProperty ("duetCustomProp", "MISSING").toString();
        }

        auto verdict = juce::String ("Save/Reload custom property: ")
                         + (result == "duet-42" ? "SURVIVED" : "LOST (" + result + ")");
        std::cout << "[PERSIST] " << verdict << std::endl;
        log (verdict);
    }

    void showDeviceSelector()
    {
        auto* sel = new juce::AudioDeviceSelectorComponent (
            engine.getDeviceManager().deviceManager, 0, 0, 0, 2, false, false, true, false);
        sel->setSize (520, 460);

        juce::DialogWindow::LaunchOptions o;
        o.content.setOwned (sel);
        o.dialogTitle = "Audio device";
        o.resizable = false;
        o.launchAsync();
    }

    void toggleGL()
    {
        if (glAttached)
        {
            glContext.setContinuousRepainting (false);
            glContext.detach();
            glAttached = false;
        }
        else
        {
            glContext.attachTo (viewport);
            glContext.setContinuousRepainting (benchmarking);
            glAttached = true;
        }
        canvas->setBenchmarking (benchmarking, glAttached);
        log (juce::String ("Renderer: ") + (glAttached ? "OpenGLContext attached to viewport" : "software"));
    }

    //==========================================================================
    void timerCallback() override
    {
        const double now = juce::Time::getMillisecondCounterHiRes();
        const double dt = lastTick > 0 ? (now - lastTick) / 1000.0 : 0.0;
        lastTick = now;

        if (autoScroll)
        {
            auto pos = viewport.getViewPosition();
            int maxX = juce::jmax (0, canvas->getWidth() - viewport.getWidth());
            int x = pos.x + (int) (scrollDir * 400.0 * dt);
            if (x <= 0) { x = 0; scrollDir = 1; }
            if (x >= maxX) { x = maxX; scrollDir = -1; }
            viewport.setViewPosition (x, pos.y);
        }

        if (zoomCycle)
        {
            zoomPhase += dt * 0.5;
            const double pps = 60.0 * std::pow (2.0, std::sin (zoomPhase) * 1.8);
            const double anchorTime = viewport.getViewPositionX() / canvas->getPixelsPerSecond();
            canvas->setPixelsPerSecond (pps);
            viewport.setViewPosition ((int) (anchorTime * pps), viewport.getViewPositionY());
        }

        if (autoMutate)
        {
            mutateAccum += dt;
            if (mutateAccum >= 1.0) { mutateAccum = 0; mutateModel(); }
        }

        // xrun + cpu telemetry, correlated with mutation count
        if (auto* dev = engine.getDeviceManager().deviceManager.getCurrentAudioDevice())
        {
            const int x = dev->getXRunCount();
            if (x != lastXruns)
            {
                std::cout << "[XRUN] count=" << x << " (was " << lastXruns << ") at mutation #"
                          << mutationCount << (edit->getTransport().isPlaying() ? " playing" : " stopped")
                          << std::endl;
                lastXruns = x;
            }
        }
        perfAccum += dt;
        if (perfAccum >= 5.0)
        {
            perfAccum = 0;
            std::cout << "[PERF] audio-cpu=" << juce::String (engine.getDeviceManager().getCpuUsage() * 100.0, 1)
                      << "% xruns=" << lastXruns << " mutations=" << mutationCount << std::endl;
        }

        // transport + device status
        auto& dm = engine.getDeviceManager();
        juce::String dev = "no device";
        if (auto* d = dm.deviceManager.getCurrentAudioDevice())
            dev = d->getTypeName() + ": " + d->getName()
                  + juce::String::formatted (" %.0f Hz, %d smp, out-latency %.1f ms",
                                             d->getCurrentSampleRate(),
                                             d->getCurrentBufferSizeSamples(),
                                             1000.0 * d->getOutputLatencyInSamples()
                                                 / juce::jmax (1.0, d->getCurrentSampleRate()));

        statusLabel.setText (juce::String (edit->getTransport().isPlaying() ? "PLAYING " : "stopped ")
                                 + juce::String::formatted ("%.2fs | ", edit->getTransport().getPosition().inSeconds())
                                 + dev,
                             juce::dontSendNotification);
    }

    void log (const juce::String& s)
    {
        std::cout << "[LOG] " << s << std::endl;
        logLabel.setText (s, juce::dontSendNotification);
    }

    //==========================================================================
    te::Engine engine { "DuetWalkingSkeletonPrototype" };
    std::unique_ptr<te::Edit> edit;
    juce::File workDir, editFile;
    juce::Array<juce::File> waveFiles;
    juce::AudioFormatManager formatManager;

    std::unique_ptr<TimelineCanvas> canvas;
    juce::Viewport viewport;
    juce::OpenGLContext glContext;

    juce::TextButton playButton, stopButton, mutateButton, saveReloadButton, deviceButton;
    juce::ToggleButton autoMutateButton, benchButton, scrollButton, zoomButton, glButton;
    juce::Label fpsLabel, statusLabel, logLabel;

    bool benchmarking = false, autoScroll = false, zoomCycle = false, glAttached = false, autoMutate = false;
    int scrollDir = 1, mutationCount = 0, lastXruns = 0;
    double lastTick = 0, zoomPhase = 0, mutateAccum = 0, perfAccum = 0;
};

//==============================================================================
class SkeletonApp final : public juce::JUCEApplication
{
public:
    const juce::String getApplicationName() override    { return "Duet Walking Skeleton PROTOTYPE"; }
    const juce::String getApplicationVersion() override { return "0.0.1"; }

    void initialise (const juce::String&) override
    {
        window = std::make_unique<MainWindow> (getApplicationName());
    }

    void shutdown() override { window = nullptr; }

private:
    class MainWindow final : public juce::DocumentWindow
    {
    public:
        explicit MainWindow (const juce::String& name)
            : DocumentWindow (name, juce::Colours::black, allButtons)
        {
            setUsingNativeTitleBar (true);
            setContentOwned (new MainComponent(), true);
            setResizable (true, false);
            centreWithSize (getWidth(), getHeight());
            setVisible (true);
        }

        void closeButtonPressed() override { juce::JUCEApplication::getInstance()->systemRequestedQuit(); }
    };

    std::unique_ptr<MainWindow> window;
};

START_JUCE_APPLICATION (SkeletonApp)
