// PROTOTYPE — disposable executable evidence for roadmap node xciphe.

#include "OfflineRenderPrototype.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <iostream>

TEST_CASE ("the known Tracktion Edit renders audio offline")
{
    const auto rendered = duet::prototype::renderKnownToneEdit();
    std::cout << "render: sampleRate=" << rendered.sampleRate
              << " channels=" << rendered.audio.getNumChannels()
              << " samples=" << rendered.audio.getNumSamples()
              << " peak=" << rendered.audio.getMagnitude (0, rendered.audio.getNumSamples())
              << '\n';

    REQUIRE (rendered.sampleRate == 44100.0);
    REQUIRE (rendered.audio.getNumChannels() == 2);
    REQUIRE (rendered.audio.getNumSamples() == 88200);
    REQUIRE (rendered.audio.getMagnitude (0, rendered.audio.getNumSamples()) > 0.1f);
}

TEST_CASE ("rendering the same Tracktion Edit twice is bit-exact")
{
    const auto evidence = duet::prototype::measureKnownToneDeterminism();
    CAPTURE (evidence.differingSamples, evidence.maximumAbsoluteDifference);
    std::cout << "determinism: bitExact=" << evidence.bitExact
              << " differingSamples=" << evidence.differingSamples
              << " maxAbsDifference=" << evidence.maximumAbsoluteDifference
              << " fingerprint=" << evidence.firstRenderFingerprint << '\n';

    REQUIRE (evidence.bitExact);
    REQUIRE (evidence.differingSamples == 0);
    REQUIRE (evidence.maximumAbsoluteDifference == 0.0f);
}

TEST_CASE ("known MIDI rendered through a stand-in instrument has the expected pitches and onsets")
{
    const auto evidence = duet::prototype::measureStandInInstrumentFeatures();
    constexpr auto oneRenderBlockSeconds = 512.0 / 44100.0;
    CAPTURE (evidence.firstPitchHz, evidence.secondPitchHz,
             evidence.firstOnsetSeconds, evidence.secondOnsetSeconds);
    std::cout << "instrument: firstPitchHz=" << evidence.firstPitchHz
              << " secondPitchHz=" << evidence.secondPitchHz
              << " firstOnsetSeconds=" << evidence.firstOnsetSeconds
              << " secondOnsetSeconds=" << evidence.secondOnsetSeconds << '\n';

    REQUIRE (evidence.firstPitchHz == Catch::Approx (440.0).margin (2.0));
    REQUIRE (evidence.secondPitchHz == Catch::Approx (523.251).margin (2.0));
    REQUIRE (evidence.firstOnsetSeconds <= 0.25);
    REQUIRE (evidence.firstOnsetSeconds >= 0.25 - oneRenderBlockSeconds);
    REQUIRE (evidence.secondOnsetSeconds <= 1.0);
    REQUIRE (evidence.secondOnsetSeconds >= 1.0 - oneRenderBlockSeconds);
}

TEST_CASE ("a known signal rendered through a stand-in gain effect measures six decibels quieter")
{
    const auto evidence = duet::prototype::measureStandInEffectFeatures();
    CAPTURE (evidence.dryRms, evidence.wetRms, evidence.levelChangeDb);
    std::cout << "effect: dryRms=" << evidence.dryRms
              << " wetRms=" << evidence.wetRms
              << " levelChangeDb=" << evidence.levelChangeDb << '\n';

    REQUIRE (evidence.dryRms > 0.1f);
    REQUIRE (evidence.wetRms == Catch::Approx (evidence.dryRms * 0.5f).margin (0.0001));
    REQUIRE (evidence.levelChangeDb == Catch::Approx (-6.0206).margin (0.001));
}
