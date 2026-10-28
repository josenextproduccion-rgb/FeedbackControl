#include "PluginProcessor.h"
#include "PluginEditor.h"

FeedbackControlAudioProcessor::FeedbackControlAudioProcessor()
    : AudioProcessor (BusesProperties()
        .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
        .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      parameters (*this, nullptr, "PARAMETERS", createParameterLayout())
{
}

bool FeedbackControlAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto input = layouts.getMainInputChannelSet();
    const auto output = layouts.getMainOutputChannelSet();
    if (input != output)
        return false;
    return input == juce::AudioChannelSet::mono() || input == juce::AudioChannelSet::stereo();
}

juce::AudioProcessorValueTreeState::ParameterLayout
FeedbackControlAudioProcessor::createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;
    for (int i = 0; i < numberOfManualNotches; ++i)
    {
        const auto n = juce::String (i + 1);
        layout.add (std::make_unique<juce::AudioParameterBool>("enable" + n, "Manual Notch " + n, false));
        layout.add (std::make_unique<juce::AudioParameterFloat>("freq" + n, "Manual Notch " + n + " Frequency",
            juce::NormalisableRange<float> (80.0f, 10000.0f, 1.0f, 0.35f), 1000.0f));
        layout.add (std::make_unique<juce::AudioParameterFloat>("depth" + n, "Manual Notch " + n + " Depth",
            juce::NormalisableRange<float> (0.0f, 18.0f, 0.1f), 6.0f));
        layout.add (std::make_unique<juce::AudioParameterFloat>("q" + n, "Manual Notch " + n + " Q",
            juce::NormalisableRange<float> (2.0f, 30.0f, 0.1f, 0.5f), 10.0f));
    }
    layout.add (std::make_unique<juce::AudioParameterBool>("autoEnable", "Automatic Detection", true));
    layout.add (std::make_unique<juce::AudioParameterFloat>("sensitivity", "Detection Sensitivity",
        juce::NormalisableRange<float> (1.5f, 5.0f, 0.1f), 2.8f));
    layout.add (std::make_unique<juce::AudioParameterBool>("bypass", "Bypass", false));
    return layout;
}

void FeedbackControlAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    juce::ignoreUnused (samplesPerBlock);
    currentSampleRate = sampleRate;
    ringWritePosition = 0;
    samplesSinceAnalysis = 0;
    samplesCollected = 0;
    candidateBin = -1;
    persistentCandidateFrames = 0;
    replacementIndex = 0;
    candidatePeak = 0.0f;
    analysisRing.fill (0.0f);
    fftData.fill (0.0f);
    detectedFrequency.store (0.0f);
    activeAutoNotches.store (0);

    for (auto& notch : manualFilters)
        for (auto& filter : notch)
            filter.reset();
    for (auto& notch : autoNotches)
        notch = AutoNotch {};
    updateManualFilters();
}

void FeedbackControlAudioProcessor::releaseResources() {}

void FeedbackControlAudioProcessor::updateManualFilters()
{
    for (int i = 0; i < numberOfManualNotches; ++i)
    {
        const auto n = juce::String (i + 1);
        const float frequency = parameters.getRawParameterValue ("freq" + n)->load();
        const float depth = parameters.getRawParameterValue ("depth" + n)->load();
        const float q = parameters.getRawParameterValue ("q" + n)->load();
        const bool enabled = parameters.getRawParameterValue ("enable" + n)->load() > 0.5f;
        auto coeff = juce::dsp::IIR::Coefficients<float>::makePeakFilter (
            currentSampleRate,
            juce::jlimit (20.0f, static_cast<float> (currentSampleRate * 0.45), frequency),
            q, juce::Decibels::decibelsToGain (enabled ? -depth : 0.0f));
        for (auto& filter : manualFilters[static_cast<size_t> (i)])
            *filter.coefficients = *coeff;
    }
}

void FeedbackControlAudioProcessor::analyseInputBlock (const juce::AudioBuffer<float>& buffer)
{
    if (parameters.getRawParameterValue ("autoEnable")->load() < 0.5f)
        return;

    const int channels = juce::jmin (buffer.getNumChannels(), 2);
    for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
    {
        float mono = 0.0f;
        for (int channel = 0; channel < channels; ++channel)
            mono += buffer.getReadPointer (channel)[sample];
        mono /= static_cast<float> (juce::jmax (1, channels));

        analysisRing[static_cast<size_t> (ringWritePosition)] = mono;
        ringWritePosition = (ringWritePosition + 1) % fftSize;
        samplesCollected = juce::jmin (fftSize, samplesCollected + 1);
        if (++samplesSinceAnalysis >= hopSize)
        {
            samplesSinceAnalysis = 0;
            if (samplesCollected == fftSize)
                analyseSpectrum();
        }
    }
}

void FeedbackControlAudioProcessor::analyseSpectrum()
{
    // Hann-windowed 2048-point FFT. Buffer is fixed-size and preallocated.
    for (int i = 0; i < fftSize; ++i)
    {
        const int index = (ringWritePosition + i) % fftSize;
        const float hann = 0.5f - 0.5f * std::cos (juce::MathConstants<float>::twoPi
                                                    * static_cast<float> (i) / static_cast<float> (fftSize - 1));
        fftData[static_cast<size_t> (i)] = analysisRing[static_cast<size_t> (index)] * hann;
    }
    std::fill (fftData.begin() + fftSize, fftData.end(), 0.0f);
    fft.performFrequencyOnlyForwardTransform (fftData.data());

    const int firstBin = juce::jmax (2, static_cast<int> (80.0 * fftSize / currentSampleRate));
    const int lastBin = juce::jmin (fftSize / 2 - 2, static_cast<int> (8000.0 * fftSize / currentSampleRate));
    int peakBin = -1;
    float peak = 0.0f;
    float strongestNeighbourhood = 0.0f;

    for (int bin = firstBin; bin <= lastBin; ++bin)
    {
        const float value = fftData[static_cast<size_t> (bin)] / static_cast<float> (fftSize);
        const float neighbours = 0.5f * (fftData[static_cast<size_t> (bin - 1)]
                                        + fftData[static_cast<size_t> (bin + 1)]) / static_cast<float> (fftSize);
        if (value > peak && value > neighbours)
        {
            peak = value;
            peakBin = bin;
            strongestNeighbourhood = neighbours;
        }
    }

    const float sensitivity = parameters.getRawParameterValue ("sensitivity")->load();
    // Candidate must be both prominent and sufficiently strong. This is an initial heuristic,
    // not a trained classifier; music can still trigger it and it needs real-world tuning.
    const bool plausible = peakBin > 0 && peak > 0.0025f
        && peak > strongestNeighbourhood * sensitivity;

    if (!plausible)
    {
        persistentCandidateFrames = juce::jmax (0, persistentCandidateFrames - 1);
        return;
    }

    if (candidateBin >= 0 && std::abs (peakBin - candidateBin) <= 1)
        ++persistentCandidateFrames;
    else
    {
        candidateBin = peakBin;
        persistentCandidateFrames = 1;
        candidatePeak = peak;
    }

    // Require a stable narrow peak over ~100 ms before inserting a filter.
    if (persistentCandidateFrames >= 10 && peak >= candidatePeak * 0.8f)
    {
        const float frequency = static_cast<float> (candidateBin) * static_cast<float> (currentSampleRate)
                                / static_cast<float> (fftSize);
        activateAutoNotch (frequency);
        detectedFrequency.store (frequency);
        persistentCandidateFrames = 0;
        candidateBin = -1;
    }
}

void FeedbackControlAudioProcessor::calculateNotchCoefficients (AutoNotch& notch)
{
    const float frequency = juce::jlimit (40.0f, static_cast<float> (currentSampleRate * 0.45), notch.frequency);
    const float w0 = juce::MathConstants<float>::twoPi * frequency / static_cast<float> (currentSampleRate);
    const float cosW0 = std::cos (w0);
    const float sinW0 = std::sin (w0);
    const float alpha = sinW0 / (2.0f * juce::jmax (1.0f, notch.q));
    const float a0 = 1.0f + alpha;
    notch.b0 = 1.0f / a0;
    notch.b1 = (-2.0f * cosW0) / a0;
    notch.b2 = 1.0f / a0;
    notch.a1 = (-2.0f * cosW0) / a0;
    notch.a2 = (1.0f - alpha) / a0;
}

void FeedbackControlAudioProcessor::activateAutoNotch (float frequency)
{
    // Do not keep stacking filters on a frequency already being attenuated.
    for (auto& notch : autoNotches)
    {
        if (notch.active && std::abs (notch.frequency - frequency) < 80.0f)
            return;
    }

    AutoNotch* target = nullptr;
    for (auto& notch : autoNotches)
        if (!notch.active) { target = &notch; break; }
    if (target == nullptr)
    {
        // Replace the oldest slot in a simple round-robin fashion.
        target = &autoNotches[static_cast<size_t> (replacementIndex++ % numberOfAutoNotches)];
    }

    target->active = true;
    target->frequency = frequency;
    target->depthDb = 12.0f;
    target->q = 12.0f;
    target->z1.fill (0.0f);
    target->z2.fill (0.0f);
    calculateNotchCoefficients (*target);

    int count = 0;
    for (const auto& notch : autoNotches)
        if (notch.active) ++count;
    activeAutoNotches.store (count);
}

void FeedbackControlAudioProcessor::processAutoNotches (juce::AudioBuffer<float>& buffer)
{
    if (parameters.getRawParameterValue ("autoEnable")->load() < 0.5f)
        return;

    const int channels = juce::jmin (buffer.getNumChannels(), 2);
    for (auto& notch : autoNotches)
    {
        if (!notch.active)
            continue;
        for (int channel = 0; channel < channels; ++channel)
        {
            auto* data = buffer.getWritePointer (channel);
            float z1 = notch.z1[static_cast<size_t> (channel)];
            float z2 = notch.z2[static_cast<size_t> (channel)];
            for (int i = 0; i < buffer.getNumSamples(); ++i)
            {
                const float x = data[i];
                const float y = notch.b0 * x + z1;
                z1 = notch.b1 * x - notch.a1 * y + z2;
                z2 = notch.b2 * x - notch.a2 * y;
                data[i] = y;
            }
            notch.z1[static_cast<size_t> (channel)] = z1;
            notch.z2[static_cast<size_t> (channel)] = z2;
        }
    }
}

void FeedbackControlAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ignoreUnused (midiMessages);
    juce::ScopedNoDenormals noDenormals;
    const int inputChannels = getTotalNumInputChannels();
    const int outputChannels = getTotalNumOutputChannels();
    for (int ch = inputChannels; ch < outputChannels; ++ch)
        buffer.clear (ch, 0, buffer.getNumSamples());

    if (parameters.getRawParameterValue ("bypass")->load() > 0.5f)
        return;

    analyseInputBlock (buffer);
    updateManualFilters();
    for (int ch = 0; ch < juce::jmin (buffer.getNumChannels(), 2); ++ch)
    {
        for (int i = 0; i < numberOfManualNotches; ++i)
        {
            juce::dsp::AudioBlock<float> block (buffer);
            auto channelBlock = block.getSingleChannelBlock (static_cast<size_t> (ch));
            juce::dsp::ProcessContextReplacing<float> context (channelBlock);
            manualFilters[static_cast<size_t> (i)][static_cast<size_t> (ch)].process (context);
        }
    }
    processAutoNotches (buffer);
}

juce::AudioProcessorEditor* FeedbackControlAudioProcessor::createEditor()
{
    return new FeedbackControlAudioProcessorEditor (*this);
}

void FeedbackControlAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = parameters.copyState();
    std::unique_ptr<juce::XmlElement> xml (state.createXml());
    copyXmlToBinary (*xml, destData);
}

void FeedbackControlAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xml (getXmlFromBinary (data, sizeInBytes));
    if (xml != nullptr && xml->hasTagName (parameters.state.getType()))
        parameters.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new FeedbackControlAudioProcessor();
}
