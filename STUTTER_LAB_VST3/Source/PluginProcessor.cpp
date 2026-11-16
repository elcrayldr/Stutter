#include "PluginProcessor.h"
#include "PluginEditor.h"

StutterLabAudioProcessor::StutterLabAudioProcessor()
    : AudioProcessor(BusesProperties()
        .withInput("Input", juce::AudioChannelSet::stereo(), true)
        .withOutput("Output", juce::AudioChannelSet::stereo(), true))
{
}

void StutterLabAudioProcessor::prepareToPlay(double sampleRate, int)
{
    captureSampleRate = sampleRate;
    const int capacity = juce::jmax(1, (int) std::ceil(sampleRate * maxCaptureSeconds));
    {
        const juce::SpinLock::ScopedLockType lock(captureLock);
        captureBuffer.setSize(2, capacity, false, true, false);
        captureBuffer.clear();
    }
    capturedSamples.store(0);
    capturing.store(false);
}

void StutterLabAudioProcessor::releaseResources()
{
    capturing.store(false);
}

bool StutterLabAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    const auto in = layouts.getMainInputChannelSet();
    const auto out = layouts.getMainOutputChannelSet();
    return (in == juce::AudioChannelSet::mono() || in == juce::AudioChannelSet::stereo())
        && in == out;
}

void StutterLabAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
            if (auto bpm = pos->getBpm())
                hostBpm.store((float) *bpm);

    if (captureStartRequested.exchange(false, std::memory_order_acq_rel))
    {
        capturedSamples.store(0, std::memory_order_release);
        capturing.store(true, std::memory_order_release);
    }

    if (capturing.load(std::memory_order_relaxed))
    {
        // Never block the real-time thread: if the UI is taking a snapshot, skip this block.
        if (captureLock.tryEnter())
        {
            const int writeAt = capturedSamples.load(std::memory_order_relaxed);
            const int available = captureBuffer.getNumSamples() - writeAt;
            const int amount = juce::jmin(buffer.getNumSamples(), available);
            if (amount > 0)
            {
                const int channels = juce::jmin(2, buffer.getNumChannels());
                for (int ch = 0; ch < channels; ++ch)
                    captureBuffer.copyFrom(ch, writeAt, buffer, ch, 0, amount);
                if (channels == 1)
                    captureBuffer.copyFrom(1, writeAt, buffer, 0, 0, amount);
                capturedSamples.store(writeAt + amount, std::memory_order_release);
            }
            if (amount < buffer.getNumSamples())
                capturing.store(false, std::memory_order_release);
            captureLock.exit();
        }
    }

    // Capture mode is non-destructive: the live signal always passes through unchanged.
}

juce::AudioProcessorEditor* StutterLabAudioProcessor::createEditor()
{
    return new StutterLabAudioProcessorEditor(*this);
}

void StutterLabAudioProcessor::startCapture()
{
    // The audio thread starts the next take at a block boundary.
    captureStartRequested.store(true, std::memory_order_release);
}

void StutterLabAudioProcessor::stopCapture()
{
    captureStartRequested.store(false, std::memory_order_release);
    capturing.store(false, std::memory_order_release);
}

bool StutterLabAudioProcessor::copyCaptureSnapshot(juce::AudioBuffer<float>& destination, int& samples) const
{
    const juce::SpinLock::ScopedLockType lock(captureLock);
    samples = capturedSamples.load(std::memory_order_acquire);
    if (samples <= 0 || captureBuffer.getNumChannels() < 2)
    {
        destination.setSize(2, 0);
        return false;
    }
    destination.setSize(2, samples, false, false, true);
    for (int ch = 0; ch < 2; ++ch)
        destination.copyFrom(ch, 0, captureBuffer, ch, 0, samples);
    return true;
}

void StutterLabAudioProcessor::setSelection(double startSeconds, double endSeconds)
{
    const double duration = capturedSamples.load() / captureSampleRate;
    const double a = juce::jlimit(0.0, duration, juce::jmin(startSeconds, endSeconds));
    const double b = juce::jlimit(0.0, duration, juce::jmax(startSeconds, endSeconds));
    selectionStart.store(a);
    selectionEnd.store(juce::jmax(a + 0.005, b));
}

bool StutterLabAudioProcessor::renderToWav(juce::File destination, juce::String& error)
{
    stopCapture();

    juce::AudioBuffer<float> capturedAudio;
    int captured = 0;
    if (!copyCaptureSnapshot(capturedAudio, captured) || captured < 64)
    {
        error = "Capture at least a short audio phrase before rendering.";
        return false;
    }

    int start = juce::jlimit(0, captured - 1, (int) std::floor(selectionStart.load() * captureSampleRate));
    int end = juce::jlimit(start + 1, captured, (int) std::ceil(selectionEnd.load() * captureSampleRate));
    const int sourceLength = end - start;
    const int divisionIndex = rateDivision.load();
    const int beatDenominators[] = { 2, 4, 8, 16, 32, 64 };
    const double bpm = juce::jmax(30.0, (double) hostBpm.load());
    const double divisionSeconds = 60.0 / bpm * 4.0 / beatDenominators[divisionIndex - 1];
    // The rhythmic division defines the maximum stutter slice length.
    // Do not time-stretch the selected vocal: preserve its original pitch.
    const int gridSamples = (int) std::round(divisionSeconds * captureSampleRate);
    const int targetLength = juce::jmax(32, juce::jmin(sourceLength, gridSamples));
    const int count = repeats.load();
    if ((int64) targetLength * count > (int64) (captureSampleRate * 120.0))
    {
        error = "Render exceeds the 120-second safety limit. Reduce repeats or choose a shorter slice.";
        return false;
    }

    juce::AudioBuffer<float> chunk(2, targetLength);
    for (int ch = 0; ch < 2; ++ch)
    {
        for (int i = 0; i < targetLength; ++i)
        {
            const double sourcePos = (double) i;
            const int i0 = juce::jlimit(0, sourceLength - 1, (int) sourcePos);
            const int i1 = juce::jmin(sourceLength - 1, i0 + 1);
            const float frac = (float) (sourcePos - i0);
            const int actual = reverse.load() ? end - 1 - i0 : start + i0;
            const int actualNext = reverse.load() ? end - 1 - i1 : start + i1;
            const float s0 = capturedAudio.getSample(ch, actual);
            const float s1 = capturedAudio.getSample(ch, actualNext);
            chunk.setSample(ch, i, s0 + (s1 - s0) * frac);
        }
    }

    // Offline tone shaping: phone band-limit and controlled tanh saturation.
    const float drive = 1.0f + saturation.load() * 7.0f;
    const bool phone = phoneMode.load();
    if (phone || drive > 1.001f)
    {
        const float hpHz = phone ? 320.0f : 35.0f;
        const float lpHz = phone ? 3400.0f : 18000.0f;
        const float hpA = std::exp(-2.0f * juce::MathConstants<float>::pi * hpHz / (float) captureSampleRate);
        const float lpA = std::exp(-2.0f * juce::MathConstants<float>::pi * lpHz / (float) captureSampleRate);
        for (int ch = 0; ch < 2; ++ch)
        {
            float previousInput = 0.0f, highPassed = 0.0f, lowPassed = 0.0f;
            for (int i = 0; i < targetLength; ++i)
            {
                const float input = chunk.getSample(ch, i);
                highPassed = hpA * (highPassed + input - previousInput);
                previousInput = input;
                lowPassed = (1.0f - lpA) * highPassed + lpA * lowPassed;
                const float shaped = std::tanh(lowPassed * drive) / std::tanh(drive);
                chunk.setSample(ch, i, shaped);
            }
        }
    }

    // Short edge fades suppress discontinuities between repeated slices.
    const int fadeSamples = juce::jmin(targetLength / 2, (int) std::round(captureSampleRate * 0.003));
    if (fadeSamples > 0)
        for (int ch = 0; ch < 2; ++ch)
            for (int i = 0; i < fadeSamples; ++i)
            {
                const float gain = (float) i / (float) fadeSamples;
                chunk.setSample(ch, i, chunk.getSample(ch, i) * gain);
                chunk.setSample(ch, targetLength - 1 - i, chunk.getSample(ch, targetLength - 1 - i) * gain);
            }

    juce::AudioBuffer<float> output(2, targetLength * count);
    output.clear();
    for (int r = 0; r < count; ++r)
        for (int ch = 0; ch < 2; ++ch)
            output.copyFrom(ch, r * targetLength, chunk, ch, 0, targetLength);

    destination.deleteFile();
    destination.getParentDirectory().createDirectory();
    std::unique_ptr<juce::FileOutputStream> stream(destination.createOutputStream());
    if (stream == nullptr)
    {
        error = "Could not create the output WAV file.";
        return false;
    }

    juce::WavAudioFormat wav;
    auto writer = std::unique_ptr<juce::AudioFormatWriter>(
        wav.createWriterFor(stream.get(), captureSampleRate, 2, 24, {}, 0));
    if (writer == nullptr)
    {
        error = "Could not initialise the WAV writer.";
        return false;
    }
    stream.release();
    const bool ok = writer->writeFromAudioSampleBuffer(output, 0, output.getNumSamples());
    writer.reset();
    if (!ok)
    {
        error = "WAV export failed.";
        return false;
    }
    return true;
}

void StutterLabAudioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    juce::MemoryOutputStream out(destData, true);
    out.writeInt(2);
    out.writeInt(repeats.load());
    out.writeInt(rateDivision.load());
    out.writeBool(reverse.load());
    out.writeFloat(saturation.load());
    out.writeBool(phoneMode.load());
    out.writeDouble(selectionStart.load());
    out.writeDouble(selectionEnd.load());
}

void StutterLabAudioProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    juce::MemoryInputStream in(data, (size_t) sizeInBytes, false);
    const int version = in.readInt();
    if (version < 1 || version > 2) return;
    repeats.store(juce::jlimit(2, 32, in.readInt()));
    rateDivision.store(juce::jlimit(1, 6, in.readInt()));
    reverse.store(in.readBool());
    if (version >= 2) { saturation.store(juce::jlimit(0.0f, 1.0f, in.readFloat())); phoneMode.store(in.readBool()); }
    selectionStart.store(juce::jmax(0.0, in.readDouble()));
    selectionEnd.store(juce::jmax(selectionStart.load() + 0.005, in.readDouble()));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new StutterLabAudioProcessor();
}
