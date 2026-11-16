#pragma once
#include <JuceHeader.h>
#include <atomic>
#include <vector>

class StutterLabAudioProcessor final : public juce::AudioProcessor
{
public:
    StutterLabAudioProcessor();
    ~StutterLabAudioProcessor() override = default;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}
    void getStateInformation(juce::MemoryBlock&) override;
    void setStateInformation(const void*, int) override;

    void startCapture();
    void stopCapture();
    bool isCapturing() const { return capturing.load(); }
    int getCapturedSamples() const { return capturedSamples.load(); }
    double getCaptureSampleRate() const { return captureSampleRate; }
    float getHostBpm() const { return hostBpm.load(); }
    bool copyCaptureSnapshot(juce::AudioBuffer<float>& destination, int& samples) const;

    void setSelection(double startSeconds, double endSeconds);
    double getSelectionStart() const { return selectionStart.load(); }
    double getSelectionEnd() const { return selectionEnd.load(); }

    void setRepeats(int value) { repeats.store(juce::jlimit(2, 32, value)); }
    int getRepeats() const { return repeats.load(); }
    void setRateDivision(int value) { rateDivision.store(juce::jlimit(1, 6, value)); }
    int getRateDivision() const { return rateDivision.load(); }
    void setReverse(bool value) { reverse.store(value); }
    bool getReverse() const { return reverse.load(); }
    bool renderToWav(juce::File destination, juce::String& error);
    void setSaturation(float value) { saturation.store(juce::jlimit(0.0f, 1.0f, value)); }
    float getSaturation() const { return saturation.load(); }
    void setPhoneMode(bool value) { phoneMode.store(value); }
    bool getPhoneMode() const { return phoneMode.load(); }

    void setPreviewPlaying(bool shouldPlay);
    bool isPreviewPlaying() const { return previewPlaying.load(); }
    bool hasRenderedAudio() const { return previewSamples.load() > 0; }
    int getPreviewSamples() const { return previewSamples.load(); }
    float getPreviewProgress() const;
    std::vector<float> getCapturePeaks(int bins) const;
    std::vector<float> getPreviewPeaks(int bins) const;
    juce::File getLastRenderFile() const;

private:
    juce::AudioBuffer<float> captureBuffer;
    mutable juce::SpinLock captureLock;
    std::atomic<bool> capturing { false };
    std::atomic<bool> captureStartRequested { false };
    std::atomic<int> capturedSamples { 0 };
    std::atomic<int> repeats { 4 };
    std::atomic<int> rateDivision { 4 };
    std::atomic<bool> reverse { false };
    std::atomic<float> saturation { 0.0f };
    std::atomic<bool> phoneMode { false };
    std::atomic<float> hostBpm { 120.0f };
    std::atomic<double> selectionStart { 0.0 };
    std::atomic<double> selectionEnd { 0.5 };
    double captureSampleRate = 44100.0;
    int maxCaptureSeconds = 30;

    // Render memory is written offline and read by the audio callback for preview.
    juce::AudioBuffer<float> previewBuffer;
    mutable juce::SpinLock previewLock;
    std::atomic<int> previewSamples { 0 };
    std::atomic<int> previewPosition { 0 };
    std::atomic<bool> previewPlaying { false };
    juce::File lastRenderFile;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(StutterLabAudioProcessor)
};
