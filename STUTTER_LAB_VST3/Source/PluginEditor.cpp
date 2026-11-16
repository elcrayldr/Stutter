#include "PluginEditor.h"

StutterLabAudioProcessorEditor::StutterLabAudioProcessorEditor(StutterLabAudioProcessor& p)
    : AudioProcessorEditor(&p), processor(p)
{
    setSize(1000, 600);
    setResizable(true, true);
    setResizeLimits(860, 500, 1600, 1000);

    for (int i = 0; i < 6; ++i)
        divisionBox.addItem(juce::String("1/") + juce::String(1 << (i + 1)), i + 1);
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

    captureButton.onClick = [this] { processor.startCapture(); updateStatus(); };
    stopButton.onClick = [this] { processor.stopCapture(); updateStatus(); };
    renderButton.onClick = [this] { renderAndStartDrag(); };
    exportButton.onClick = [this] { exportWav(); };

    addAndMakeVisible(captureButton);
    addAndMakeVisible(stopButton);
    addAndMakeVisible(renderButton);
    addAndMakeVisible(exportButton);
    addAndMakeVisible(divisionBox);
    addAndMakeVisible(repeatsSlider);
    addAndMakeVisible(reverseButton);
    addAndMakeVisible(phoneButton);
    addAndMakeVisible(saturationSlider);
    addAndMakeVisible(saturationLabel);
    addAndMakeVisible(statusLabel);
    addAndMakeVisible(bpmLabel);
    addAndMakeVisible(repeatsLabel);

    statusLabel.setJustificationType(juce::Justification::centredLeft);
    statusLabel.setColour(juce::Label::textColourId, juce::Colour(0xffb9c0c9));
    bpmLabel.setJustificationType(juce::Justification::centredRight);
    repeatsLabel.setText("REPEATS", juce::dontSendNotification);
    repeatsLabel.setJustificationType(juce::Justification::centredRight);
    updateStatus();
    startTimerHz(8);
}

void StutterLabAudioProcessorEditor::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xff101216));
    g.setColour(juce::Colour(0xff191e25));
    g.fillRoundedRectangle(getLocalBounds().toFloat().reduced(12), 12.0f);
    g.setColour(juce::Colour(0xff343b43));
    g.drawRoundedRectangle(getLocalBounds().toFloat().reduced(12), 12.0f, 1.0f);
    g.setColour(juce::Colour(0xffc5a66a));
    g.fillRect(28, 17, 4, 54);
    g.setColour(juce::Colour(0xff2b3139));
    g.drawHorizontalLine(82, 28.0f, (float) getWidth() - 28.0f);

    g.setColour(juce::Colour(0xffd4b477));
    g.setFont(juce::Font(26.0f, juce::Font::bold));
    g.drawText("STUTTER LAB", 30, 20, 350, 34, juce::Justification::centredLeft);
    g.setColour(juce::Colour(0xff8d96a3));
    g.setFont(11.0f);
    g.drawText("CRAY LDR  /  CAPTURE-BASED VOCAL STUTTER", 32, 54, 420, 18, juce::Justification::centredLeft);

    g.setColour(juce::Colour(0xff252c35));
    g.fillRoundedRectangle(waveformBounds.toFloat(), 8.0f);
    g.setColour(juce::Colour(0xff48515d));
    g.drawRoundedRectangle(waveformBounds.toFloat(), 8.0f, 1.0f);

    const int captured = processor.getCapturedSamples();
    if (captured > 0 && processor.getCaptureSampleRate() > 0)
    {
        juce::AudioBuffer<float> audio;
        int snapshotSamples = 0;
        if (!processor.copyCaptureSnapshot(audio, snapshotSamples)) return;
        const int width = waveformBounds.getWidth();
        const int mid = waveformBounds.getCentreY();
        const float scale = waveformBounds.getHeight() * 0.42f;
        g.setColour(juce::Colour(0xffffc928));
        for (int x = 0; x < width; ++x)
        {
            const int a = (int)((double)x / width * snapshotSamples);
            const int b = juce::jmin(snapshotSamples, (int)((double)(x + 1) / width * snapshotSamples));
            float peak = 0.0f;
            for (int i = a; i < juce::jmax(a + 1, b); ++i)
                peak = juce::jmax(peak, juce::jmax(std::abs(audio.getSample(0, i)), std::abs(audio.getSample(1, i))));
            g.drawVerticalLine(waveformBounds.getX() + x, (float)(mid - peak * scale), (float)(mid + peak * scale));
        }

        const double sr = processor.getCaptureSampleRate();
        const int sx = waveformBounds.getX() + (int)(processor.getSelectionStart() * sr / juce::jmax(1, snapshotSamples) * width);
        const int ex = waveformBounds.getX() + (int)(processor.getSelectionEnd() * sr / juce::jmax(1, snapshotSamples) * width);
        g.setColour(juce::Colour(0x44ffffff));
        g.fillRect(juce::Rectangle<int>(sx, waveformBounds.getY(), juce::jmax(1, ex - sx), waveformBounds.getHeight()));
        g.setColour(juce::Colour(0xffffc928));
        g.drawRect(juce::Rectangle<int>(sx, waveformBounds.getY(), juce::jmax(1, ex - sx), waveformBounds.getHeight()), 1);
    }
    else
    {
        g.setColour(juce::Colour(0xff737d89));
        g.setFont(14.0f);
        g.drawText("Press CAPTURE, perform your vocal, then press STOP.", waveformBounds,
                   juce::Justification::centred);
    }

    g.setColour(juce::Colour(0xff657080));
    g.setFont(10.0f);
    g.drawText("DRAG ACROSS THE WAVEFORM TO SELECT A REGION", waveformBounds.getX(),
               waveformBounds.getBottom() + 8, waveformBounds.getWidth(), 16, juce::Justification::centred);
}

void StutterLabAudioProcessorEditor::resized()
{
    auto area = getLocalBounds().reduced(28);
    area.removeFromTop(82);
    auto controls = area.removeFromTop(66);
    captureButton.setBounds(controls.removeFromLeft(118).reduced(3));
    stopButton.setBounds(controls.removeFromLeft(76).reduced(3));
    controls.removeFromLeft(12);
    repeatsLabel.setBounds(controls.removeFromLeft(62));
    repeatsSlider.setBounds(controls.removeFromLeft(112).reduced(3));
    controls.removeFromLeft(8);
    divisionBox.setBounds(controls.removeFromLeft(82).reduced(3));
    reverseButton.setBounds(controls.removeFromLeft(88).reduced(3));
    phoneButton.setBounds(controls.removeFromLeft(72).reduced(3));
    saturationLabel.setBounds(controls.removeFromLeft(52).withY(controls.getY() + 1).withHeight(18));
    saturationSlider.setBounds(controls.removeFromLeft(66).reduced(1));
    bpmLabel.setBounds(controls.removeFromRight(72));
    waveformBounds = area.removeFromTop((int)(area.getHeight() * 0.70f)).reduced(2);
    area.removeFromTop(34);
    auto renderRow = area.removeFromTop(46);
    renderButton.setBounds(renderRow.withSizeKeepingCentre(220, 42).translated(-116, 0));
    exportButton.setBounds(renderRow.withSizeKeepingCentre(170, 42).translated(116, 0));
    statusLabel.setBounds(getLocalBounds().reduced(30).removeFromBottom(24));
}

void StutterLabAudioProcessorEditor::timerCallback()
{
    updateStatus();
    repaint(waveformBounds.expanded(2));
}

void StutterLabAudioProcessorEditor::updateStatus()
{
    const double seconds = processor.getCapturedSamples() / juce::jmax(1.0, processor.getCaptureSampleRate());
    statusLabel.setText(processor.isCapturing()
        ? "RECORDING  •  " + juce::String(seconds, 1) + " s"
        : "CAPTURE  •  " + juce::String(seconds, 1) + " s", juce::dontSendNotification);
    bpmLabel.setText(juce::String(processor.getHostBpm(), 1) + " BPM", juce::dontSendNotification);
}

void StutterLabAudioProcessorEditor::renderAndStartDrag()
{
    juce::File out = juce::File::getSpecialLocation(juce::File::tempDirectory)
        .getChildFile("STUTTER_LAB_Render.wav");
    juce::String error;
    if (!processor.renderToWav(out, error))
    {
        statusLabel.setText(error, juce::dontSendNotification);
        return;
    }
    statusLabel.setText("Rendered: " + out.getFileName(), juce::dontSendNotification);
    juce::DragAndDropContainer::performExternalDragDropOfFiles({ out.getFullPathName() }, false);
}

void StutterLabAudioProcessorEditor::mouseDown(const juce::MouseEvent& e)
{
    if (waveformBounds.contains(e.getPosition()) && processor.getCapturedSamples() > 0)
    {
        draggingSelection = true;
        dragStart = (double)(e.x - waveformBounds.getX()) / waveformBounds.getWidth();
    }
}

void StutterLabAudioProcessorEditor::mouseDrag(const juce::MouseEvent& e)
{
    if (!draggingSelection) return;
    const double end = juce::jlimit(0.0, 1.0, (double)(e.x - waveformBounds.getX()) / waveformBounds.getWidth());
    const double duration = processor.getCapturedSamples() / processor.getCaptureSampleRate();
    processor.setSelection(dragStart * duration, end * duration);
}

void StutterLabAudioProcessorEditor::mouseUp(const juce::MouseEvent&)
{
    draggingSelection = false;
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
        });
}
