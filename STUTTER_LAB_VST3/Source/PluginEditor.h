#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h"

class StutterLabAudioProcessorEditor final : public juce::AudioProcessorEditor,
                                             private juce::Timer
{
public:
    explicit StutterLabAudioProcessorEditor(StutterLabAudioProcessor&);
    ~StutterLabAudioProcessorEditor() override = default;

    void paint(juce::Graphics&) override;
    void resized() override;
    void mouseDown(const juce::MouseEvent&) override;
    void mouseDrag(const juce::MouseEvent&) override;
    void mouseUp(const juce::MouseEvent&) override;

private:
    void timerCallback() override;
    void updateStatus();
    void renderAndStartDrag();
    void exportWav();

    StutterLabAudioProcessor& processor;
    juce::TextButton captureButton { "CAPTURE" };
    juce::TextButton stopButton { "STOP" };
    juce::TextButton renderButton { "RENDER + DRAG" };
    juce::TextButton exportButton { "EXPORT WAV" };
    std::unique_ptr<juce::FileChooser> fileChooser;
    juce::ComboBox divisionBox;
    juce::Slider repeatsSlider;
    juce::ToggleButton reverseButton { "REVERSE" };
    juce::ToggleButton phoneButton { "PHONE" };
    juce::Slider saturationSlider;
    juce::Label saturationLabel;
    juce::Label statusLabel;
    juce::Label bpmLabel;
    juce::Label repeatsLabel;
    juce::Rectangle<int> waveformBounds;
    bool draggingSelection = false;
    double dragStart = 0.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(StutterLabAudioProcessorEditor)
};
