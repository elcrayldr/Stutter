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
    {
        const juce::SpinLock::ScopedLockType lock(previewLock);
        previewBuffer.setSize(2, 0);
    }
    capturedSamples.store(0);
    previewSamples.store(0);
    previewPosition.store(0);
    previewPlaying.store(false);
    capturing.store(false);
}

void StutterLabAudioProcessor::releaseResources()
{
    capturing.store(false);
    previewPlaying.store(false);
}

bool StutterLabAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    const auto in = layouts.getMainInputChannelSet();
    const auto out = layouts.getMainOutputChannelSet();
    return (in == juce::AudioChannelSet::mono() || in == juce::AudioChannelSet::stereo()) && in == out;
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

    if (capturing.load(std::memory_order_relaxed) && captureLock.tryEnter())
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

    // Preview replaces the plugin output only while explicitly auditioning the render.
    // If the preview buffer is busy, the live input passes through for this block.
    if (previewPlaying.load(std::memory_order_acquire) && previewLock.tryEnter())
    {
        const int total = previewSamples.load(std::memory_order_acquire);
        int position = previewPosition.load(std::memory_order_relaxed);
        const int amount = juce::jmin(buffer.getNumSamples(), juce::jmax(0, total - position));
        if (amount > 0 && previewBuffer.getNumSamples() >= total)
        {
            const int outChannels = buffer.getNumChannels();
            for (int ch = 0; ch < outChannels; ++ch)
            {
                const int sourceChannel = juce::jmin(ch, previewBuffer.getNumChannels() - 1);
                buffer.copyFrom(ch, 0, previewBuffer, sourceChannel, position, amount);
                if (amount < buffer.getNumSamples())
                    buffer.clear(ch, amount, buffer.getNumSamples() - amount);
            }
            position += amount;
            previewPosition.store(position, std::memory_order_release);
        }
        if (position >= total || total <= 0)
            previewPlaying.store(false, std::memory_order_release);
        previewLock.exit();
    }
}

juce::AudioProcessorEditor* StutterLabAudioProcessor::createEditor()
{
    return new StutterLabAudioProcessorEditor(*this);
}

void StutterLabAudioProcessor::startCapture()
{
    previewPlaying.store(false, std::memory_order_release);
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
    previewPlaying.store(false, std::memory_order_release);

    juce::AudioBuffer<float> capturedAudio;
    int captured = 0;
    if (!copyCaptureSnapshot(capturedAudio, captured) || captured < 64)
    {
        error = "Capture at least a short audio phrase before rendering.";
        return false;
    }

    const int start = juce::jlimit(0, captured - 1, (int) std::floor(selectionStart.load() * captureSampleRate));
    const int end = juce::jlimit(start + 1, captured, (int) std::ceil(selectionEnd.load() * captureSampleRate));
    const int sourceLength = end - start;
    const int denominators[] = { 2, 4, 8, 16, 32, 64 };
    const double bpm = juce::jlimit(30.0, 300.0, (double) hostBpm.load());
    const double divisionSeconds = 60.0 / bpm * 4.0 / denominators[rateDivision.load() - 1];
    const int gridSamples = juce::jmax(32, (int) std::round(divisionSeconds * captureSampleRate));
    const int targetLength = juce::jmax(32, juce::jmin(sourceLength, gridSamples));
    const int count = repeats.load();

    if ((int64) targetLength * count > (int64) (captureSampleRate * 120.0))
    {
        error = "Render exceeds the 120-second safety limit.";
        return false;
    }

    juce::AudioBuffer<float> chunk(2, targetLength);
    const bool isReverse = reverse.load();
    for (int ch = 0; ch < 2; ++ch)
        for (int i = 0; i < targetLength; ++i)
        {
            const int sourceIndex = isReverse ? end - 1 - i : start + i;
            chunk.setSample(ch, i, capturedAudio.getSample(ch, juce::jlimit(start, end - 1, sourceIndex)));
        }

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
                chunk.setSample(ch, i, std::tanh(lowPassed * drive) / std::tanh(drive));
            }
        }
    }

    const int fadeSamples = juce::jmin(targetLength / 2, (int) std::round(captureSampleRate * 0.003));
    for (int ch = 0; ch < 2; ++ch)
        for (int i = 0; i < fadeSamples; ++i)
        {
            const float gain = (float) i / (float) fadeSamples;
            chunk.setSample(ch, i, chunk.getSample(ch, i) * gain);
            chunk.setSample(ch, targetLength - 1 - i, chunk.getSample(ch, targetLength - 1 - i) * gain);
        }

    juce::AudioBuffer<float> output(2, targetLength * count);
    for (int r = 0; r < count; ++r)
        for (int ch = 0; ch < 2; ++ch)
            output.copyFrom(ch, r * targetLength, chunk, ch, 0, targetLength);

    destination.getParentDirectory().createDirectory();
    destination.deleteFile();
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

    // Publish a complete render only after both the audio buffer and WAV are ready.
    {
        const juce::SpinLock::ScopedLockType lock(previewLock);
        previewBuffer.makeCopyOf(output, true);
        previewSamples.store(output.getNumSamples(), std::memory_order_release);
        previewPosition.store(0, std::memory_order_release);
        lastRenderFile = destination;
    }
    return true;
}

void StutterLabAudioProcessor::setPreviewPlaying(bool shouldPlay)
{
    if (shouldPlay && hasRenderedAudio())
    {
        if (previewPosition.load(std::memory_order_acquire) >= previewSamples.load(std::memory_order_acquire))
            previewPosition.store(0, std::memory_order_release);
        previewPlaying.store(true, std::memory_order_release);
    }
    else
        previewPlaying.store(false, std::memory_order_release);
}

float StutterLabAudioProcessor::getPreviewProgress() const
{
    const int total = previewSamples.load(std::memory_order_acquire);
    if (total <= 0) return -1.0f;
    return juce::jlimit(0.0f, 1.0f, (float) previewPosition.load(std::memory_order_acquire) / (float) total);
}

std::vector<float> StutterLabAudioProcessor::getCapturePeaks(int bins) const
{
    bins = juce::jlimit(1, 4096, bins);
    std::vector<float> peaks((size_t) bins, 0.0f);
    if (!captureLock.tryEnter()) return peaks;
    const int total = capturedSamples.load(std::memory_order_acquire);
    if (total > 0)
        for (int x = 0; x < bins; ++x)
        {
            const int a = x * total / bins;
            const int b = juce::jmax(a + 1, (x + 1) * total / bins);
            float peak = 0.0f;
            for (int i = a; i < juce::jmin(total, b); ++i)
                peak = juce::jmax(peak, juce::jmax(std::abs(captureBuffer.getSample(0, i)),
                                                   std::abs(captureBuffer.getSample(1, i))));
            peaks[(size_t) x] = peak;
        }
    captureLock.exit();
    return peaks;
}

std::vector<float> StutterLabAudioProcessor::getPreviewPeaks(int bins) const
{
    bins = juce::jlimit(1, 4096, bins);
    std::vector<float> peaks((size_t) bins, 0.0f);
    if (!previewLock.tryEnter()) return peaks;
    const int total = previewSamples.load(std::memory_order_acquire);
    if (total > 0)
        for (int x = 0; x < bins; ++x)
        {
            const int a = x * total / bins;
            const int b = juce::jmax(a + 1, (x + 1) * total / bins);
            float peak = 0.0f;
            for (int i = a; i < juce::jmin(total, b); ++i)
                peak = juce::jmax(peak, juce::jmax(std::abs(previewBuffer.getSample(0, i)),
                                                   std::abs(previewBuffer.getSample(1, i))));
            peaks[(size_t) x] = peak;
        }
    previewLock.exit();
    return peaks;
}

juce::File StutterLabAudioProcessor::getLastRenderFile() const
{
    const juce::SpinLock::ScopedTryLockType lock(previewLock);
    return lock.isLocked() ? lastRenderFile : juce::File();
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
    if (version >= 2)
    {
        saturation.store(juce::jlimit(0.0f, 1.0f, in.readFloat()));
        phoneMode.store(in.readBool());
    }
    selectionStart.store(juce::jmax(0.0, in.readDouble()));
    selectionEnd.store(juce::jmax(selectionStart.load() + 0.005, in.readDouble()));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new StutterLabAudioProcessor();
}
