#include "PluginEditor.h"
#include <algorithm>
#include <array>

namespace
{
    const juce::Colour bg(0xff101216), panel(0xff191e25), line(0xff343b43);
    const juce::Colour gold(0xffffc928), paleGold(0xffd4b477), muted(0xff8d96a3);
}

StutterLabAudioProcessorEditor::StutterLabAudioProcessorEditor(StutterLabAudioProcessor& p)
    : AudioProcessorEditor(&p), processor(p)
{
    setSize(1000, 650);
    setResizable(true, true);
    setResizeLimits(860, 570, 1600, 1100);

    for (int i = 0; i < 6; ++i)
        divisionBox.addItem("1/" + juce::String(1 << (i + 1)), i + 1);
    divisionBox.setSelectedId(processor.getRateDivision());
    divisionBox.onChange = [this] { processor.setRateDivision(divisionBox.getSelectedId()); };

    repeatsSlider.setRange(2, 32, 1);
    repeatsSlider.setValue(processor.getRepeats());
    repeatsSlider.setSliderStyle(juce::Slider::IncDecButtons);
    repeatsSlider.setTextBoxStyle(juce::Slider::TextBoxLeft, false, 55, 26);
    repeatsSlider.onValueChange = [this] { processor.setRepeats((int) repeatsSlider.getValue()); };

    reverseButton.setToggleState(processor.getReverse(), juce::dontSendNotification);
    reverseButton.onClick = [this] { processor.setReverse(reverseButton.getToggleState()); };
    phoneButton.setToggleState(processor.getPhoneMode(), juce::dontSendNotification);
    phoneButton.onClick = [this] { processor.setPhoneMode(phoneButton.getToggleState()); };
    saturationSlider.setRange(0.0, 1.0, 0.01);
    saturationSlider.setValue(processor.getSaturation());
    saturationSlider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    saturationSlider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 54, 18);
    saturationSlider.onValueChange = [this] { processor.setSaturation((float) saturationSlider.getValue()); };
    saturationLabel.setText("DRIVE", juce::dontSendNotification);
    saturationLabel.setJustificationType(juce::Justification::centred);

    captureButton.onClick = [this] { processor.setPreviewPlaying(false); processor.startCapture(); updateStatus(); };
    stopButton.onClick = [this] { processor.stopCapture(); updateStatus(); };
    renderButton.onClick = [this] { renderAndStartDrag(); };
    previewButton.onClick = [this]
    {
        processor.setPreviewPlaying(!processor.isPreviewPlaying());
        updateStatus();
    };
    exportButton.onClick = [this] { exportWav(); };

    for (auto* c : std::array<juce::Component*, 12>{ &captureButton, &stopButton, &renderButton,
            &previewButton, &exportButton, &divisionBox, &repeatsSlider, &reverseButton,
            &phoneButton, &saturationSlider, &saturationLabel, &statusLabel })
        addAndMakeVisible(c);
    addAndMakeVisible(bpmLabel);
    addAndMakeVisible(repeatsLabel);

    statusLabel.setJustificationType(juce::Justification::centredLeft);
    statusLabel.setColour(juce::Label::textColourId, juce::Colour(0xffb9c0c9));
    bpmLabel.setJustificationType(juce::Justification::centredRight);
    repeatsLabel.setText("REPEATS", juce::dontSendNotification);
    repeatsLabel.setJustificationType(juce::Justification::centredRight);
    updateStatus();
    startTimerHz(12);
}

void StutterLabAudioProcessorEditor::drawWaveform(juce::Graphics& g,
    const juce::Rectangle<int>& bounds, const std::vector<float>& peaks,
    juce::Colour colour, const juce::String& label, bool showSelection)
{
    g.setColour(juce::Colour(0xff252c35));
    g.fillRoundedRectangle(bounds.toFloat(), 8.0f);
    g.setColour(line);
    g.drawRoundedRectangle(bounds.toFloat(), 8.0f, 1.0f);

    g.setColour(muted);
    g.setFont(10.5f);
    g.drawText(label, bounds.getX() + 12, bounds.getY() + 7, bounds.getWidth() - 24, 15,
               juce::Justification::centredLeft);

    if (peaks.empty() || std::none_of(peaks.begin(), peaks.end(), [](float v) { return v > 0.00001f; }))
    {
        g.setColour(juce::Colour(0xff737d89));
        g.setFont(13.0f);
        g.drawText(label.startsWith("RENDER")
            ? "Generate a stutter to see the processed result here."
            : "Press CAPTURE, perform your vocal, then press STOP.",
            bounds.reduced(12), juce::Justification::centred);
        return;
    }

    const int top = bounds.getY() + 26;
    const int bottom = bounds.getBottom() - 10;
    const float mid = (float)(top + bottom) * 0.5f;
    const float scale = (float)(bottom - top) * 0.46f;
    g.setColour(colour);
    const int width = bounds.getWidth() - 20;
    for (int x = 0; x < width; ++x)
    {
        const size_t index = juce::jmin(peaks.size() - 1,
            (size_t)((double)x / juce::jmax(1, width - 1) * (peaks.size() - 1)));
        const float peak = juce::jlimit(0.0f, 1.0f, peaks[index]);
        g.drawVerticalLine(bounds.getX() + 10 + x, mid - peak * scale, mid + peak * scale);
    }

    if (showSelection)
    {
        const double duration = processor.getCapturedSamples() / juce::jmax(1.0, processor.getCaptureSampleRate());
        if (duration > 0.0)
        {
            const int sx = bounds.getX() + 10 + (int)(processor.getSelectionStart() / duration * width);
            const int ex = bounds.getX() + 10 + (int)(processor.getSelectionEnd() / duration * width);
            g.setColour(juce::Colour(0x33ffffff));
            g.fillRect(juce::Rectangle<int>(sx, top, juce::jmax(1, ex - sx), bottom - top));
            g.setColour(colour);
            g.drawVerticalLine(sx, (float)top, (float)bottom);
            g.drawVerticalLine(ex, (float)top, (float)bottom);
        }
    }
    else
    {
        const float progress = processor.getPreviewProgress();
        if (progress >= 0.0f && processor.isPreviewPlaying())
        {
            const int x = bounds.getX() + 10 + (int)(progress * width);
            g.setColour(juce::Colours::white);
            g.drawVerticalLine(x, (float)top, (float)bottom);
        }
    }
}

void StutterLabAudioProcessorEditor::paint(juce::Graphics& g)
{
    g.fillAll(bg);
    g.setColour(panel);
    g.fillRoundedRectangle(getLocalBounds().toFloat().reduced(12), 12.0f);
    g.setColour(line);
    g.drawRoundedRectangle(getLocalBounds().toFloat().reduced(12), 12.0f, 1.0f);
    g.setColour(paleGold);
    g.fillRect(28, 17, 4, 54);
    g.setColour(juce::Colour(0xff2b3139));
    g.drawHorizontalLine(82, 28.0f, (float)getWidth() - 28.0f);

    g.setColour(paleGold);
    g.setFont(juce::Font(26.0f, juce::Font::bold));
    g.drawText("STUTTER LAB", 30, 20, 350, 34, juce::Justification::centredLeft);
    g.setColour(muted);
    g.setFont(11.0f);
    g.drawText("CRAY LDR  /  CAPTURE  •  GENERATE  •  PREVIEW  •  DRAG", 32, 54, 520, 18,
               juce::Justification::centredLeft);

    drawWaveform(g, captureWaveformBounds, processor.getCapturePeaks(720), gold,
                 "01  /  SOURCE CAPTURE  —  DRAG TO SELECT", true);
    drawWaveform(g, renderWaveformBounds, processor.getPreviewPeaks(720), paleGold,
                 "02  /  GENERATED STUTTER  —  DRAG THIS WAVEFORM TO YOUR DAW", false);
}

void StutterLabAudioProcessorEditor::resized()
{
    auto area = getLocalBounds().reduced(28);
    area.removeFromTop(82);

    auto controls = area.removeFromTop(62);
    captureButton.setBounds(controls.removeFromLeft(112).reduced(3));
    stopButton.setBounds(controls.removeFromLeft(70).reduced(3));
    controls.removeFromLeft(8);
    repeatsLabel.setBounds(controls.removeFromLeft(58));
    repeatsSlider.setBounds(controls.removeFromLeft(105).reduced(3));
    controls.removeFromLeft(6);
    divisionBox.setBounds(controls.removeFromLeft(76).reduced(3));
    reverseButton.setBounds(controls.removeFromLeft(80).reduced(3));
    phoneButton.setBounds(controls.removeFromLeft(66).reduced(3));
    saturationLabel.setBounds(controls.removeFromLeft(48).withY(controls.getY() + 1).withHeight(18));
    saturationSlider.setBounds(controls.removeFromLeft(62).reduced(1));
    bpmLabel.setBounds(controls.removeFromRight(72));

    const int gap = 12;
    const int waveHeight = juce::jmax(100, (area.getHeight() - 118) / 2);
    captureWaveformBounds = area.removeFromTop(waveHeight).reduced(2);
    area.removeFromTop(gap);
    renderWaveformBounds = area.removeFromTop(waveHeight).reduced(2);
    area.removeFromTop(12);

    auto row = area.removeFromTop(42);
    captureButton.setEnabled(true);
    renderButton.setBounds(row.removeFromLeft(190).withSizeKeepingCentre(180, 40));
    previewButton.setBounds(row.removeFromLeft(130).withSizeKeepingCentre(120, 40));
    exportButton.setBounds(row.removeFromLeft(150).withSizeKeepingCentre(140, 40));
    statusLabel.setBounds(getLocalBounds().reduced(30).removeFromBottom(22));
}

void StutterLabAudioProcessorEditor::timerCallback()
{
    updateStatus();
    repaint(captureWaveformBounds.expanded(2));
    repaint(renderWaveformBounds.expanded(2));
}

void StutterLabAudioProcessorEditor::updateStatus()
{
    const double seconds = processor.getCapturedSamples() / juce::jmax(1.0, processor.getCaptureSampleRate());
    const juce::String state = processor.isCapturing() ? "RECORDING" : "CAPTURE";
    statusLabel.setText(state + "  •  " + juce::String(seconds, 1) + " s" +
        (processor.isPreviewPlaying() ? "     /     PREVIEW PLAYING" : ""), juce::dontSendNotification);
    bpmLabel.setText(juce::String(processor.getHostBpm(), 1) + " BPM", juce::dontSendNotification);
    previewButton.setButtonText(processor.isPreviewPlaying() ? "STOP PREVIEW" : "PREVIEW");
    previewButton.setEnabled(processor.hasRenderedAudio());
}

void StutterLabAudioProcessorEditor::renderAndStartDrag()
{
    const auto out = juce::File::getSpecialLocation(juce::File::tempDirectory)
        .getChildFile("STUTTER_LAB_Render.wav");
    juce::String error;
    if (!processor.renderToWav(out, error))
    {
        statusLabel.setText(error, juce::dontSendNotification);
        return;
    }
    processor.setPreviewPlaying(true);
    statusLabel.setText("Rendered. Preview ready — drag the lower waveform to the DAW.", juce::dontSendNotification);
    repaint(renderWaveformBounds);
    juce::DragAndDropContainer::performExternalDragDropOfFiles({ out.getFullPathName() }, false);
}

void StutterLabAudioProcessorEditor::mouseDown(const juce::MouseEvent& e)
{
    if (captureWaveformBounds.contains(e.getPosition()) && processor.getCapturedSamples() > 0)
    {
        draggingSelection = true;
        dragStart = juce::jlimit(0.0, 1.0,
            (double)(e.x - captureWaveformBounds.getX() - 10) / juce::jmax(1, captureWaveformBounds.getWidth() - 20));
    }
    else if (renderWaveformBounds.contains(e.getPosition()) && processor.hasRenderedAudio())
    {
        draggingRender = true;
        renderDragStarted = false;
        renderDragStart = e.getPosition();
    }
}

void StutterLabAudioProcessorEditor::mouseDrag(const juce::MouseEvent& e)
{
    if (draggingSelection)
    {
        const double end = juce::jlimit(0.0, 1.0,
            (double)(e.x - captureWaveformBounds.getX() - 10) / juce::jmax(1, captureWaveformBounds.getWidth() - 20));
        const double duration = processor.getCapturedSamples() / processor.getCaptureSampleRate();
        processor.setSelection(dragStart * duration, end * duration);
    }
    else if (draggingRender && !renderDragStarted &&
             e.getDistanceFromDragStart() > 5)
    {
        const auto file = processor.getLastRenderFile();
        if (file.existsAsFile())
        {
            renderDragStarted = true;
            juce::DragAndDropContainer::performExternalDragDropOfFiles({ file.getFullPathName() }, false);
        }
    }
}

void StutterLabAudioProcessorEditor::mouseUp(const juce::MouseEvent&)
{
    draggingSelection = false;
    draggingRender = false;
    renderDragStarted = false;
}

void StutterLabAudioProcessorEditor::exportWav()
{
    const auto defaultFile = juce::File::getSpecialLocation(juce::File::userDocumentsDirectory)
        .getChildFile("STUTTER_LAB_Render.wav");
    fileChooser = std::make_unique<juce::FileChooser>("Export STUTTER LAB WAV", defaultFile, "*.wav");
    fileChooser->launchAsync(juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles,
        [this](const juce::FileChooser& chooser)
        {
            auto destination = chooser.getResult();
            if (destination == juce::File()) return;
            if (destination.getFileExtension().isEmpty())
                destination = destination.withFileExtension(".wav");
            juce::String error;
            if (!processor.renderToWav(destination, error))
                statusLabel.setText(error, juce::dontSendNotification);
            else
                statusLabel.setText("Exported: " + destination.getFileName(), juce::dontSendNotification);
            repaint(renderWaveformBounds);
        });
}
