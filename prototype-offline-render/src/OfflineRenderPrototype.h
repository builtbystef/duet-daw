#pragma once

#include <juce_audio_basics/juce_audio_basics.h>

#include <cstdint>

namespace duet::prototype
{

struct RenderResult
{
    double sampleRate = 0.0;
    juce::AudioBuffer<float> audio;
};

struct DeterminismEvidence
{
    bool bitExact = false;
    int differingSamples = 0;
    float maximumAbsoluteDifference = 0.0f;
    std::uint64_t firstRenderFingerprint = 0;
};

struct InstrumentEvidence
{
    double firstPitchHz = 0.0;
    double secondPitchHz = 0.0;
    double firstOnsetSeconds = 0.0;
    double secondOnsetSeconds = 0.0;
};

struct EffectEvidence
{
    float dryRms = 0.0f;
    float wetRms = 0.0f;
    double levelChangeDb = 0.0;
};

RenderResult renderKnownToneEdit();
DeterminismEvidence measureKnownToneDeterminism();
InstrumentEvidence measureStandInInstrumentFeatures();
EffectEvidence measureStandInEffectFeatures();

} // namespace duet::prototype
