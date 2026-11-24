#pragma once
#include <juce_dsp/juce_dsp.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include <array>
#include <vector>
#include <cmath>
#include <algorithm>
#include <atomic>

//==============================================================================
// Red de Retardos Realimentados Hadamard de 8 Canales (FDN de Alta Densidad)
//==============================================================================
class HadamardFDN
{
public:
    static constexpr int NumLines = 8;

    void prepare (double sampleRate)
    {
        sr = sampleRate > 0.0 ? sampleRate : 44100.0;
        size_t needed = static_cast<size_t> (sr * 0.25);
        bufSize = 1;
        while (bufSize < needed) bufSize <<= 1;
        mask = bufSize - 1;

        for (auto& line : lines)
            line.assign (bufSize, 0.0f);

        reset();
    }

    void reset()
    {
        for (auto& line : lines) std::fill (line.begin(), line.end(), 0.0f);
        lpState.fill (0.0f);
        dcIn.fill (0.0f);
        dcOut.fill (0.0f);
        writePos = 0;
        for (int i = 0; i < NumLines; ++i)
            lfoPhase[static_cast<size_t> (i)] = static_cast<float> (i) * 0.785f;
    }

    void setProfile (const std::array<float, NumLines>& delayMs, float rt60Seconds,
                     float dampingAmount, float modDepthMs)
    {
        rt60 = std::clamp (rt60Seconds, 0.18f, 7.5f);
        damp = std::clamp (dampingAmount, 0.05f, 0.88f);
        modDepthSamples = modDepthMs * 0.001f * static_cast<float> (sr);

        for (size_t i = 0; i < NumLines; ++i)
        {
            baseDelaySamples[i] = std::clamp (delayMs[i] * 0.001f * static_cast<float> (sr),
                                              32.0f, static_cast<float> (bufSize - 64));
            const float delaySec = baseDelaySamples[i] / static_cast<float> (sr);
            lineGain[i] = std::min (0.988f, std::pow (10.0f, -3.0f * delaySec / rt60));
        }
    }

    inline void processStereo (float inL, float inR, float& outL, float& outR) noexcept
    {
        static constexpr std::array<float, NumLines> lfoRates {
            0.17f, 0.23f, 0.29f, 0.37f, 0.41f, 0.47f, 0.53f, 0.61f
        };

        std::array<float, NumLines> delayed {};

        // 1. Lectura modulada con interpolación lineal + filtro de absorción de paredes
        for (size_t i = 0; i < NumLines; ++i)
        {
            lfoPhase[i] += juce::MathConstants<float>::twoPi * lfoRates[i] / static_cast<float> (sr);
            if (lfoPhase[i] > juce::MathConstants<float>::twoPi)
                lfoPhase[i] -= juce::MathConstants<float>::twoPi;

            const float d = baseDelaySamples[i] + std::sin (lfoPhase[i]) * modDepthSamples;
            const auto dInt = static_cast<size_t> (d);
            const float frac = d - static_cast<float> (dInt);

            const size_t idx0 = (writePos - dInt) & mask;
            const size_t idx1 = (idx0 - 1) & mask;
            float sample = lines[i][idx0] * (1.0f - frac) + lines[i][idx1] * frac;

            // Absorción de agudos (aire/madera/hormigón)
            lpState[i] = sample * (1.0f - damp) + lpState[i] * damp;
            // Bloqueo DC
            const float dc = lpState[i] - dcIn[i] + 0.996f * dcOut[i];
            dcIn[i] = lpState[i];
            dcOut[i] = dc;

            delayed[i] = dc * lineGain[i];
        }

        // 2. Transformada rápida de Walsh-Hadamard 8x8 (matriz unitaria sin coloración metálica)
        const float a0 = delayed[0] + delayed[1], a1 = delayed[0] - delayed[1];
        const float a2 = delayed[2] + delayed[3], a3 = delayed[2] - delayed[3];
        const float a4 = delayed[4] + delayed[5], a5 = delayed[4] - delayed[5];
        const float a6 = delayed[6] + delayed[7], a7 = delayed[6] - delayed[7];

        const float b0 = a0 + a2, b1 = a1 + a3, b2 = a0 - a2, b3 = a1 - a3;
        const float b4 = a4 + a6, b5 = a5 + a7, b6 = a4 - a6, b7 = a5 - a7;

        constexpr float norm = 0.35355339059f; // 1 / sqrt(8)
        std::array<float, NumLines> mixed {
            (b0 + b4) * norm, (b1 + b5) * norm, (b2 + b6) * norm, (b3 + b7) * norm,
            (b0 - b4) * norm, (b1 - b5) * norm, (b2 - b6) * norm, (b3 - b7) * norm
        };

        // 3. Inyección estéreo decorrelacionada
        const float sL = inL * 0.35f;
        const float sR = inR * 0.35f;
        lines[0][writePos] = mixed[0] + sL;
        lines[1][writePos] = mixed[1] + sR;
        lines[2][writePos] = mixed[2] - sL;
        lines[3][writePos] = mixed[3] + sR;
        lines[4][writePos] = mixed[4] + sL;
        lines[5][writePos] = mixed[5] - sR;
        lines[6][writePos] = mixed[6] - sL;
        lines[7][writePos] = mixed[7] - sR;

        writePos = (writePos + 1) & mask;

        // 4. Salida estéreo envolvente
        outL = 0.32f * (delayed[0] + delayed[2] - delayed[4] + delayed[6]);
        outR = 0.32f * (delayed[1] - delayed[3] + delayed[5] + delayed[7]);
    }

private:
    double sr = 44100.0;
    size_t bufSize = 16384;
    size_t mask = 16383;
    size_t writePos = 0;

    float rt60 = 1.5f;
    float damp = 0.35f;
    float modDepthSamples = 8.0f;

    std::array<std::vector<float>, NumLines> lines;
    std::array<float, NumLines> baseDelaySamples {};
    std::array<float, NumLines> lineGain {};
    std::array<float, NumLines> lpState {};
    std::array<float, NumLines> dcIn {};
    std::array<float, NumLines> dcOut {};
    std::array<float, NumLines> lfoPhase {};
};

//==============================================================================
// Motor Acústico Completo: Sala 3D + Decca Tree + 4 Cámaras + IR Externo
//==============================================================================
class AcousticEngine
{
public:
    void prepare (double sampleRate, int maxBlockSize)
    {
        sr = sampleRate > 0.0 ? sampleRate : 44100.0;

        size_t needed = static_cast<size_t> (sr * 0.25);
        erBufSize = 1;
        while (erBufSize < needed) erBufSize <<= 1;
        erMask = erBufSize - 1;

        erBufL.assign (erBufSize, 0.0f);
        erBufR.assign (erBufSize, 0.0f);
        erWritePos = 0;

        roomFdn.prepare (sr);
        chamberFdn.prepare (sr);

        juce::dsp::ProcessSpec spec;
        spec.sampleRate = sr;
        spec.maximumBlockSize = static_cast<juce::uint32> (maxBlockSize);
        spec.numChannels = 2;

        preDelay.prepare (spec);
        customConvolver.prepare (spec);
        airAbsorptionL = airAbsorptionR = 0.0f;
    }

    void reset()
    {
        std::fill (erBufL.begin(), erBufL.end(), 0.0f);
        std::fill (erBufR.begin(), erBufR.end(), 0.0f);
        erWritePos = 0;
        roomFdn.reset();
        chamberFdn.reset();
        preDelay.reset();
        customConvolver.reset();
    }

    void loadCustomImpulseResponse (const juce::File& file)
    {
        if (! file.existsAsFile()) return;
        customConvolver.loadImpulseResponse (file,
                                             juce::dsp::Convolution::Stereo::yes,
                                             juce::dsp::Convolution::Trim::yes,
                                             0);
        customIRLoaded.store (true);
        loadedIRName = file.getFileNameWithoutExtension();
    }

    bool hasCustomIR() const { return customIRLoaded.load(); }
    juce::String getLoadedIRName() const { return loadedIRName; }

    //--------------------------------------------------------------------------
    // Procesamiento de Sala (Studio A/B + Reflexiones 3D + Decca Tree L-C-R)
    //--------------------------------------------------------------------------
    void processRoomAndDecca (juce::AudioBuffer<float>& buffer,
                              int modeIdx, int sourceIdx, int roomIdx, int wallsLive,
                              float micX, float micY, float deccaCenter, float deccaSides)
    {
        if (buffer.getNumChannels() < 2) return;

        // Perfiles acústicos de Studio A (Grande) y Studio B (Más contenido)
        static constexpr std::array<float, 8> studioADelays { 29.3f, 37.1f, 43.7f, 53.3f, 61.9f, 71.1f, 83.3f, 97.7f };
        static constexpr std::array<float, 8> studioBDelays { 17.3f, 21.1f, 26.9f, 31.7f, 37.9f, 43.1f, 51.3f, 59.7f };

        const float baseRt60 = (roomIdx == 0) ? (wallsLive ? 1.85f : 1.15f)
                                              : (wallsLive ? 1.05f : 0.62f);
        const float damp = wallsLive ? 0.28f : 0.56f;
        roomFdn.setProfile (roomIdx == 0 ? studioADelays : studioBDelays, baseRt60, damp, 0.35f);

        // Geometría virtual de la sala (metros)
        const float roomW = (roomIdx == 0) ? 16.0f : 10.5f;
        const float roomD = (roomIdx == 0) ? 12.0f : 8.5f;
        const float roomH = (roomIdx == 0) ? 7.5f  : 4.8f;

        const float srcX = 0.0f;
        const float srcY = 1.5f;
        const float mX = micX * (roomW * 0.36f);
        const float mY = juce::jmap (micY, 0.0f, 1.0f, 2.0f, roomD - 1.5f);

        // Cápsulas Decca Tree: Izquierda, Centro adelantado 1.0m, Derecha (separación 1.8m)
        const float dLeft   = std::hypot ((mX - 0.9f) - srcX, mY - srcY);
        const float dCenter = std::hypot (mX - srcX, std::max (0.3f, (mY - 0.9f) - srcY));
        const float dRight  = std::hypot ((mX + 0.9f) - srcX, mY - srcY);

        const float speedOfSound = 343.0f;
        const float minD = std::min ({ dLeft, dCenter, dRight });

        // Retardos relativos de llegada (alineados a la cápsula más cercana para fase musical)
        const float delL = (dLeft   - minD) / speedOfSound * static_cast<float> (sr);
        const float delC = (dCenter - minD) / speedOfSound * static_cast<float> (sr);
        const float delR = (dRight  - minD) / speedOfSound * static_cast<float> (sr);

        // 6 reflexiones tempranas primarias (paredes izq/der, frontal, trasera, suelo, techo)
        const float wallRef = wallsLive ? 0.68f : 0.42f;
        const std::array<float, 6> erDistMeters {
            std::hypot (mX + roomW * 0.5f, mY - srcY),         // Pared izquierda
            std::hypot (roomW * 0.5f - mX, mY - srcY),         // Pared derecha
            std::hypot (mX, mY + srcY),                        // Pared trasera de fuente
            std::hypot (mX, (2.0f * roomD - mY) - srcY),       // Pared fondo
            std::hypot (mY - srcY, 2.6f),                      // Suelo
            std::hypot (mY - srcY, 2.0f * roomH - 2.6f)        // Techo
        };

        std::array<size_t, 6> erDelays {};
        std::array<float, 6>  erGains {};
        for (size_t k = 0; k < 6; ++k)
        {
            const float relDist = std::max (0.4f, erDistMeters[k] - minD);
            erDelays[k] = std::min (erBufSize - 8, static_cast<size_t> (relDist / speedOfSound * static_cast<float> (sr)));
            erGains[k]  = (wallRef / (1.0f + relDist * 0.38f));
        }

        // Balance Directo vs Sala según Modo (Reverb vs Re-Mic) y distancia
        const float directGain = (modeIdx == 1) ? juce::jmap (micY, 0.0f, 1.0f, 0.92f, 0.24f) : 1.0f;
        const float erLevel    = (modeIdx == 1) ? juce::jmap (micY, 0.0f, 1.0f, 0.30f, 0.85f)
                                                : juce::jmap (micY, 0.0f, 1.0f, 0.12f, 0.50f);
        const float tailLevel  = (modeIdx == 1) ? juce::jmap (micY, 0.0f, 1.0f, 0.18f, 0.82f)
                                                : juce::jmap (micY, 0.0f, 1.0f, 0.08f, 0.52f);

        // Absorción de agudos por aire según distancia
        const float airCoeff = juce::jmap (micY, 0.0f, 1.0f, 0.05f, 0.42f);

        auto* left  = buffer.getWritePointer (0);
        auto* right = buffer.getWritePointer (1);
        const int numSamples = buffer.getNumSamples();

        for (int i = 0; i < numSamples; ++i)
        {
            float inL = left[i];
            float inR = right[i];

            // Ajuste espacial según fuente (Vocals centrado, Ensemble/Drums/Monitors estéreo)
            if (sourceIdx == 0) // Vocals: fuente puntual
            {
                const float mono = 0.5f * (inL + inR);
                inL = inR = mono;
            }

            erBufL[erWritePos] = inL;
            erBufR[erWritePos] = inR;

            // Captura directa Decca Tree (L, C, R con retardos físicos reales)
            const float capL = readFrac (erBufL, delL) * deccaSides;
            const float capC = 0.5f * (readFrac (erBufL, delC) + readFrac (erBufR, delC)) * deccaCenter;
            const float capR = readFrac (erBufR, delR) * deccaSides;

            float dirL = (capL + capC * 0.707f) * 0.65f;
            float dirR = (capR + capC * 0.707f) * 0.65f;

            // Reflexiones tempranas 3D
            float erL = 0.0f, erR = 0.0f;
            erL += erBufL[(erWritePos - erDelays[0]) & erMask] * erGains[0];
            erR += erBufR[(erWritePos - erDelays[1]) & erMask] * erGains[1];
            erL += erBufR[(erWritePos - erDelays[2]) & erMask] * erGains[2] * 0.7f;
            erR += erBufL[(erWritePos - erDelays[2]) & erMask] * erGains[2] * 0.7f;
            erL += erBufL[(erWritePos - erDelays[3]) & erMask] * erGains[3] * 0.8f;
            erR += erBufR[(erWritePos - erDelays[3]) & erMask] * erGains[3] * 0.8f;
            erL += erBufL[(erWritePos - erDelays[4]) & erMask] * erGains[4];
            erR += erBufR[(erWritePos - erDelays[4]) & erMask] * erGains[4];
            erL += erBufR[(erWritePos - erDelays[5]) & erMask] * erGains[5];
            erR += erBufL[(erWritePos - erDelays[5]) & erMask] * erGains[5];

            // Cola difusa de la sala (Hadamard FDN)
            float tailL = 0.0f, tailR = 0.0f;
            roomFdn.processStereo (inL + erL * 0.35f, inR + erR * 0.35f, tailL, tailR);

            // Mezcla final de sala + absorción de aire en modo Re-Mic
            float outL = (modeIdx == 1 ? dirL : inL) * directGain + erL * erLevel + tailL * tailLevel;
            float outR = (modeIdx == 1 ? dirR : inR) * directGain + erR * erLevel + tailR * tailLevel;

            if (modeIdx == 1)
            {
                airAbsorptionL = outL * (1.0f - airCoeff) + airAbsorptionL * airCoeff;
                airAbsorptionR = outR * (1.0f - airCoeff) + airAbsorptionR * airCoeff;
                outL = airAbsorptionL;
                outR = airAbsorptionR;
            }

            left[i]  = outL;
            right[i] = outR;

            erWritePos = (erWritePos + 1) & erMask;
        }
    }

    //--------------------------------------------------------------------------
    // Procesamiento de Cámaras Subterráneas (1, 3, 5, 8 o Custom WAV IR)
    //--------------------------------------------------------------------------
    void processChamber (juce::AudioBuffer<float>& mainBuffer,
                         juce::AudioBuffer<float>& scratchBuffer,
                         int chamberIdx, float sendMix, float decayNorm,
                         float preDelayMs, float stereoWidth)
    {
        if (sendMix <= 0.001f || mainBuffer.getNumChannels() < 2) return;

        const int numSamples = mainBuffer.getNumSamples();
        scratchBuffer.setSize (2, numSamples, false, false, true);
        scratchBuffer.copyFrom (0, 0, mainBuffer, 0, 0, numSamples);
        scratchBuffer.copyFrom (1, 0, mainBuffer, 1, 0, numSamples);

        // Pre-Delay
        const float delaySamples = std::clamp (preDelayMs * 0.001f * static_cast<float> (sr), 0.0f, 47999.0f);
        preDelay.setDelay (delaySamples);

        for (int ch = 0; ch < 2; ++ch)
        {
            auto* data = scratchBuffer.getWritePointer (ch);
            for (int i = 0; i < numSamples; ++i)
            {
                preDelay.pushSample (ch, data[i]);
                data[i] = preDelay.popSample (ch);
            }
        }

        // Si eligió Custom WAV IR y hay uno cargado, usa Convolución real
        if (chamberIdx == 4 && customIRLoaded.load())
        {
            juce::dsp::AudioBlock<float> block (scratchBuffer);
            juce::dsp::ProcessContextReplacing<float> ctx (block);
            customConvolver.process (ctx);
        }
        else
        {
            // 4 Cámaras Subterráneas de hormigón (Chambers 1, 3, 5 y 8)
            static constexpr std::array<std::array<float, 8>, 4> chamberDelays {{
                { 23.1f, 29.9f, 38.3f, 46.7f, 56.3f, 67.1f, 76.9f, 89.3f },  // Chamber 1 (Bright)
                { 31.7f, 39.3f, 48.1f, 58.7f, 68.3f, 79.7f, 92.9f, 107.3f }, // Chamber 3 (Classic)
                { 35.9f, 44.3f, 54.7f, 66.1f, 78.7f, 91.9f, 106.3f, 121.7f },// Chamber 5 (Deep)
                { 26.3f, 33.7f, 41.9f, 49.9f, 60.7f, 72.7f, 85.3f, 99.1f }   // Chamber 8 (Dense)
            }};

            static constexpr std::array<float, 4> naturalRt60 { 2.9f, 4.2f, 5.1f, 3.6f };
            static constexpr std::array<float, 4> chamberDamp { 0.24f, 0.34f, 0.44f, 0.30f };

            const size_t idx = static_cast<size_t> (chamberIdx == 4 ? 1 : std::clamp (chamberIdx, 0, 3));
            const float targetRt60 = naturalRt60[idx] * decayNorm;

            chamberFdn.setProfile (chamberDelays[idx], targetRt60, chamberDamp[idx], 0.55f);

            auto* chL = scratchBuffer.getWritePointer (0);
            auto* chR = scratchBuffer.getWritePointer (1);
            for (int i = 0; i < numSamples; ++i)
            {
                float outL = 0.0f, outR = 0.0f;
                chamberFdn.processStereo (chL[i], chR[i], outL, outR);
                chL[i] = outL;
                chR[i] = outR;
            }
        }

        // Control de Stereo Width (Mid/Side) + Suma al bus principal
        auto* sL = scratchBuffer.getWritePointer (0);
        auto* sR = scratchBuffer.getWritePointer (1);
        auto* mL = mainBuffer.getWritePointer (0);
        auto* mR = mainBuffer.getWritePointer (1);

        for (int i = 0; i < numSamples; ++i)
        {
            const float mid  = 0.5f * (sL[i] + sR[i]);
            const float side = 0.5f * (sL[i] - sR[i]) * stereoWidth;
            mL[i] += (mid + side) * sendMix;
            mR[i] += (mid - side) * sendMix;
        }
    }

private:
    inline float readFrac (const std::vector<float>& buf, float delaySamples) const noexcept
    {
        const float d = std::clamp (delaySamples, 0.0f, static_cast<float> (erBufSize - 4));
        const auto dInt = static_cast<size_t> (d);
        const float frac = d - static_cast<float> (dInt);
        const size_t idx0 = (erWritePos - dInt) & erMask;
        const size_t idx1 = (idx0 - 1) & erMask;
        return buf[idx0] * (1.0f - frac) + buf[idx1] * frac;
    }

    double sr = 44100.0;
    size_t erBufSize = 16384;
    size_t erMask = 16383;
    size_t erWritePos = 0;

    std::vector<float> erBufL, erBufR;
    float airAbsorptionL = 0.0f, airAbsorptionR = 0.0f;

    HadamardFDN roomFdn;
    HadamardFDN chamberFdn;

    juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::Linear> preDelay { 48000 };
    juce::dsp::Convolution customConvolver;
    std::atomic<bool> customIRLoaded { false };
    juce::String loadedIRName { "No IR Loaded" };
};
