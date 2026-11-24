#pragma once
#include <juce_dsp/juce_dsp.h>
#include <array>
#include <cmath>
#include <algorithm>
#include <atomic>

//==============================================================================
// Filtro Biquad Estéreo (cero heap allocations en audio thread)
//==============================================================================
class StereoBiquad
{
public:
    void prepare (double sampleRate)
    {
        sr = sampleRate > 0.0 ? sampleRate : 44100.0;
        reset();
    }

    void reset()
    {
        z1 = { 0.0f, 0.0f };
        z2 = { 0.0f, 0.0f };
    }

    void setBypass() { b0 = 1.0f; b1 = b2 = a1 = a2 = 0.0f; }

    void setHighPass (float freq, float q = 0.7071f)
    {
        freq = std::clamp (freq, 18.0f, static_cast<float> (sr * 0.45));
        const float w0 = juce::MathConstants<float>::twoPi * freq / static_cast<float> (sr);
        const float cosw = std::cos (w0);
        const float alpha = std::sin (w0) / (2.0f * q);
        setNormalized ((1.0f + cosw) * 0.5f, -(1.0f + cosw), (1.0f + cosw) * 0.5f,
                       1.0f + alpha, -2.0f * cosw, 1.0f - alpha);
    }

    void setLowPass (float freq, float q = 0.7071f)
    {
        freq = std::clamp (freq, 40.0f, static_cast<float> (sr * 0.45));
        const float w0 = juce::MathConstants<float>::twoPi * freq / static_cast<float> (sr);
        const float cosw = std::cos (w0);
        const float alpha = std::sin (w0) / (2.0f * q);
        setNormalized ((1.0f - cosw) * 0.5f, 1.0f - cosw, (1.0f - cosw) * 0.5f,
                       1.0f + alpha, -2.0f * cosw, 1.0f - alpha);
    }

    void setPeak (float freq, float gainDb, float q = 1.0f)
    {
        if (std::abs (gainDb) < 0.01f) { setBypass(); return; }
        freq = std::clamp (freq, 20.0f, static_cast<float> (sr * 0.45));
        const float A = std::pow (10.0f, gainDb / 40.0f);
        const float w0 = juce::MathConstants<float>::twoPi * freq / static_cast<float> (sr);
        const float cosw = std::cos (w0);
        const float alpha = std::sin (w0) / (2.0f * q);
        setNormalized (1.0f + alpha * A, -2.0f * cosw, 1.0f - alpha * A,
                       1.0f + alpha / A, -2.0f * cosw, 1.0f - alpha / A);
    }

    void setLowShelf (float freq, float gainDb)
    {
        if (std::abs (gainDb) < 0.01f) { setBypass(); return; }
        freq = std::clamp (freq, 20.0f, static_cast<float> (sr * 0.45));
        const float A = std::pow (10.0f, gainDb / 40.0f);
        const float w0 = juce::MathConstants<float>::twoPi * freq / static_cast<float> (sr);
        const float cosw = std::cos (w0);
        const float sq = std::sqrt (2.0f * A) * std::sin (w0);
        setNormalized (A * ((A + 1.0f) - (A - 1.0f) * cosw + sq),
                       2.0f * A * ((A - 1.0f) - (A + 1.0f) * cosw),
                       A * ((A + 1.0f) - (A - 1.0f) * cosw - sq),
                       (A + 1.0f) + (A - 1.0f) * cosw + sq,
                       -2.0f * ((A - 1.0f) + (A + 1.0f) * cosw),
                       (A + 1.0f) + (A - 1.0f) * cosw - sq);
    }

    void setHighShelf (float freq, float gainDb)
    {
        if (std::abs (gainDb) < 0.01f) { setBypass(); return; }
        freq = std::clamp (freq, 20.0f, static_cast<float> (sr * 0.45));
        const float A = std::pow (10.0f, gainDb / 40.0f);
        const float w0 = juce::MathConstants<float>::twoPi * freq / static_cast<float> (sr);
        const float cosw = std::cos (w0);
        const float sq = std::sqrt (2.0f * A) * std::sin (w0);
        setNormalized (A * ((A + 1.0f) + (A - 1.0f) * cosw + sq),
                       -2.0f * A * ((A - 1.0f) + (A + 1.0f) * cosw),
                       A * ((A + 1.0f) + (A - 1.0f) * cosw - sq),
                       (A + 1.0f) - (A - 1.0f) * cosw + sq,
                       2.0f * ((A - 1.0f) - (A + 1.0f) * cosw),
                       (A + 1.0f) - (A - 1.0f) * cosw - sq);
    }

    inline float processSample (int ch, float x) noexcept
    {
        const auto c = static_cast<size_t> (ch & 1);
        const float y = b0 * x + z1[c];
        z1[c] = b1 * x - a1 * y + z2[c];
        z2[c] = b2 * x - a2 * y;
        return y;
    }

    void processBuffer (juce::AudioBuffer<float>& buffer) noexcept
    {
        const int numCh = std::min (2, buffer.getNumChannels());
        const int numSamples = buffer.getNumSamples();
        for (int ch = 0; ch < numCh; ++ch)
        {
            auto* data = buffer.getWritePointer (ch);
            for (int i = 0; i < numSamples; ++i)
                data[i] = processSample (ch, data[i]);
        }
    }

private:
    void setNormalized (float nb0, float nb1, float nb2, float na0, float na1, float na2)
    {
        const float inv = 1.0f / na0;
        b0 = nb0 * inv; b1 = nb1 * inv; b2 = nb2 * inv;
        a1 = na1 * inv; a2 = na2 * inv;
    }

    double sr = 44100.0;
    float b0 = 1.0f, b1 = 0.0f, b2 = 0.0f, a1 = 0.0f, a2 = 0.0f;
    std::array<float, 2> z1 { 0.0f, 0.0f };
    std::array<float, 2> z2 { 0.0f, 0.0f };
};

//==============================================================================
// Etapa Analógica con Oversampling 4x (Válvulas, Transformadores y Cono de Parlante)
//==============================================================================
class OversampledAnalogStage
{
public:
    OversampledAnalogStage()
        : oversampling (2, 2, juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR, true)
    {}

    void prepare (int maxBlockSize)
    {
        oversampling.initProcessing (static_cast<size_t> (maxBlockSize));
        reset();
    }

    void reset()
    {
        oversampling.reset();
        dcIn = { 0.0f, 0.0f };
        dcOut = { 0.0f, 0.0f };
    }

    int getLatencySamples() const { return static_cast<int> (oversampling.getLatencyInSamples()); }

    void process (juce::AudioBuffer<float>& buffer, float drive, bool speakerBreakup)
    {
        if (drive <= 0.005f && ! speakerBreakup) return;
        if (buffer.getNumChannels() < 2) return;

        juce::dsp::AudioBlock<float> block (buffer);
        auto upBlock = oversampling.processSamplesUp (block);

        const float preGain = 1.0f + drive * 3.2f + (speakerBreakup ? 0.55f : 0.0f);
        const float postGain = 1.0f / (1.0f + drive * 1.45f + (speakerBreakup ? 0.25f : 0.0f));
        const float bias = 0.08f * drive + (speakerBreakup ? 0.04f : 0.0f);
        const float biasComp = std::tanh (bias);

        const auto numCh = std::min<size_t> (2, upBlock.getNumChannels());
        const auto numSamples = upBlock.getNumSamples();

        for (size_t ch = 0; ch < numCh; ++ch)
        {
            auto* data = upBlock.getChannelPointer (ch);
            for (size_t i = 0; i < numSamples; ++i)
            {
                const float x = data[i] * preGain;
                // Saturación asimétrica (armónicos pares de válvula + impares de transformador)
                float y = std::tanh (x + bias) - biasComp;
                if (speakerBreakup)
                    y = 0.72f * std::tanh (y * 1.45f) + 0.28f * y;

                // DC Blocker suave a tasa 4x
                const float dc = y - dcIn[ch] + 0.9985f * dcOut[ch];
                dcIn[ch] = y;
                dcOut[ch] = dc;
                data[i] = dc * postGain;
            }
        }

        oversampling.processSamplesDown (block);
    }

private:
    juce::dsp::Oversampling<float> oversampling;
    std::array<float, 2> dcIn { 0.0f, 0.0f };
    std::array<float, 2> dcOut { 0.0f, 0.0f };
};

//==============================================================================
// Rack de Compresores Clásicos (1176LN FET, LA-2A Opto, Capitol Mastering Tube)
//==============================================================================
class VintageCompressor
{
public:
    void prepare (double sampleRate)
    {
        sr = sampleRate > 0.0 ? sampleRate : 44100.0;
        scHpf.prepare (sr);
        scHpf.setHighPass (120.0f, 0.7071f);
        reset();
    }

    void reset()
    {
        envFast = 0.0f;
        envSlow = 0.0f;
        scHpf.reset();
        gainReductionDb.store (0.0f);
    }

    void process (juce::AudioBuffer<float>& buffer, int compType, float thresholdDb)
    {
        if (compType == 0)
        {
            gainReductionDb.store (0.0f);
            return;
        }

        const int numCh = std::min (2, buffer.getNumChannels());
        const int numSamples = buffer.getNumSamples();

        float ratio = 4.0f, attackMs = 0.6f, relFastMs = 55.0f, relSlowMs = 55.0f;
        float kneeDb = 3.0f, makeupDb = 0.0f;
        bool useScHpf = false, isOpto = false;

        if (compType == 1) // 1176LN (FET)
        {
            ratio = 6.0f; attackMs = 0.45f; relFastMs = 55.0f; kneeDb = 2.0f; makeupDb = 2.0f;
        }
        else if (compType == 2) // LA-2A (Opto)
        {
            ratio = 3.2f; attackMs = 10.0f; relFastMs = 65.0f; relSlowMs = 520.0f;
            kneeDb = 6.0f; makeupDb = 1.5f; isOpto = true;
        }
        else // Capitol Mastering Compressor (Tube CM5511)
        {
            ratio = 2.2f; attackMs = 18.0f; relFastMs = 165.0f; kneeDb = 8.0f;
            makeupDb = 1.0f; useScHpf = true;
        }

        const float atkCoeff = std::exp (-1.0f / (0.001f * attackMs * static_cast<float> (sr)));
        const float relFastCoeff = std::exp (-1.0f / (0.001f * relFastMs * static_cast<float> (sr)));
        const float relSlowCoeff = std::exp (-1.0f / (0.001f * relSlowMs * static_cast<float> (sr)));
        const float makeupLin = juce::Decibels::decibelsToGain (makeupDb);

        float maxGr = 0.0f;
        auto* left  = buffer.getWritePointer (0);
        auto* right = numCh > 1 ? buffer.getWritePointer (1) : nullptr;

        for (int i = 0; i < numSamples; ++i)
        {
            float detL = left[i];
            float detR = right ? right[i] : detL;

            if (useScHpf)
            {
                detL = scHpf.processSample (0, detL);
                detR = scHpf.processSample (1, detR);
            }

            const float peak = std::max (std::abs (detL), std::abs (detR));
            const float levelDb = juce::Decibels::gainToDecibels (peak + 1e-6f);

            // Cálculo Soft-Knee
            float overDb = 0.0f;
            const float diff = levelDb - thresholdDb;
            if (2.0f * diff < -kneeDb)
                overDb = 0.0f;
            else if (2.0f * std::abs (diff) <= kneeDb)
                overDb = (diff + kneeDb * 0.5f) * (diff + kneeDb * 0.5f) / (2.0f * kneeDb);
            else
                overDb = diff;

            const float targetGrDb = overDb * (1.0f - 1.0f / ratio);

            if (targetGrDb > envFast)
                envFast = atkCoeff * envFast + (1.0f - atkCoeff) * targetGrDb;
            else
                envFast = relFastCoeff * envFast + (1.0f - relFastCoeff) * targetGrDb;

            float finalGrDb = envFast;
            if (isOpto)
            {
                if (targetGrDb > envSlow)
                    envSlow = atkCoeff * envSlow + (1.0f - atkCoeff) * targetGrDb;
                else
                    envSlow = relSlowCoeff * envSlow + (1.0f - relSlowCoeff) * targetGrDb;
                finalGrDb = 0.55f * envFast + 0.45f * envSlow;
            }

            const float gainLin = juce::Decibels::decibelsToGain (-finalGrDb) * makeupLin;
            left[i] *= gainLin;
            if (right) right[i] *= gainLin;

            if (finalGrDb > maxGr) maxGr = finalGrDb;
        }

        gainReductionDb.store (maxGr);
    }

    float getGainReductionDb() const { return gainReductionDb.load(); }

private:
    double sr = 44100.0;
    float envFast = 0.0f;
    float envSlow = 0.0f;
    StereoBiquad scHpf;
    std::atomic<float> gainReductionDb { 0.0f };
};
