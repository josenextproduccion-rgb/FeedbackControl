#pragma once
#include <JuceHeader.h>
#include <array>

class FeedbackControlAudioProcessor : public juce::AudioProcessor
{
public:
    FeedbackControlAudioProcessor();
    ~FeedbackControlAudioProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Feedback Control"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState parameters;
    float getDetectedFrequency() const noexcept { return detectedFrequency.load(); }
    int getActiveAutoNotches() const noexcept { return activeAutoNotches.load(); }

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    static constexpr int numberOfManualNotches = 4;
    static constexpr int numberOfAutoNotches = 4;
    static constexpr int fftOrder = 11;
    static constexpr int fftSize = 1 << fftOrder;
    static constexpr int hopSize = 512;

    std::array<std::array<juce::dsp::IIR::Filter<float>, 2>, numberOfManualNotches> manualFilters;
    double currentSampleRate = 44100.0;
    void updateManualFilters();

    // Fixed-size analysis buffers: no heap allocations in processBlock().
    juce::dsp::FFT fft { fftOrder };
    std::array<float, fftSize * 2> fftData {};
    std::array<float, fftSize> analysisRing {};
    int ringWritePosition = 0;
    int samplesSinceAnalysis = 0;
    int samplesCollected = 0;
    int candidateBin = -1;
    int persistentCandidateFrames = 0;
    int replacementIndex = 0;
    float candidatePeak = 0.0f;
    std::atomic<float> detectedFrequency { 0.0f };
    std::atomic<int> activeAutoNotches { 0 };

    struct AutoNotch
    {
        bool active = false;
        float frequency = 0.0f;
        float depthDb = 12.0f;
        float q = 12.0f;
        // RBJ notch biquad coefficients and transposed direct-form II state.
        float b0 = 1.0f, b1 = 0.0f, b2 = 0.0f;
        float a1 = 0.0f, a2 = 0.0f;
        std::array<float, 2> z1 {}, z2 {};
    };
    std::array<AutoNotch, numberOfAutoNotches> autoNotches {};

    void analyseInputBlock (const juce::AudioBuffer<float>& buffer);
    void analyseSpectrum();
    void activateAutoNotch (float frequency);
    void calculateNotchCoefficients (AutoNotch& notch);
    void processAutoNotches (juce::AudioBuffer<float>& buffer);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FeedbackControlAudioProcessor)
};
