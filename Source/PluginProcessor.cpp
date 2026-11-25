#include "PluginProcessor.h"
#include "PluginEditor.h"

CapitolStudioProProcessor::CapitolStudioProProcessor()
    : AudioProcessor (BusesProperties()
        .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
        .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "Parameters", createParameterLayout())
{
}

juce::AudioProcessorValueTreeState::ParameterLayout CapitolStudioProProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> p;

    // Modo y Fuente
    p.push_back (std::make_unique<juce::AudioParameterChoice> (
        "mode", "Mode", juce::StringArray { "Reverb", "Re-Mic" }, 1));
    p.push_back (std::make_unique<juce::AudioParameterChoice> (
        "source", "Source", juce::StringArray { "Vocals", "Drums", "Ensemble", "Monitors (Swedien)" }, 3));
    p.push_back (std::make_unique<juce::AudioParameterChoice> (
        "monitorType", "Monitor", juce::StringArray { "Altec Main", "NS-10 Nearfield", "JBL PA Wedge" }, 0));

    // Sala, Paredes y Mic Locker
    p.push_back (std::make_unique<juce::AudioParameterChoice> (
        "room", "Room", juce::StringArray { "Studio A (Large)", "Studio B (Tight)" }, 0));
    p.push_back (std::make_unique<juce::AudioParameterChoice> (
        "walls", "Walls", juce::StringArray { "Tight / Controlled", "Live / Reflective" }, 1));
    p.push_back (std::make_unique<juce::AudioParameterChoice> (
        "mic", "Mic Locker", juce::StringArray { "U48 (Tube)", "C12 (Air)", "U67 (Warm)", "M50 (Decca)", "C800G (Modern)" }, 0));

    // Posicionamiento 2D y Decca Tree
    p.push_back (std::make_unique<juce::AudioParameterFloat> ("micX", "Mic Pan X", -1.0f, 1.0f, 0.0f));
    p.push_back (std::make_unique<juce::AudioParameterFloat> ("micY", "Mic Distance", 0.0f, 1.0f, 0.35f));
    p.push_back (std::make_unique<juce::AudioParameterFloat> ("deccaCenter", "Decca Center", 0.0f, 1.5f, 1.0f));
    p.push_back (std::make_unique<juce::AudioParameterFloat> ("deccaSides", "Decca Sides", 0.0f, 1.5f, 1.0f));

    // Cámaras Subterráneas (1, 3, 5, 8 + Custom WAV IR)
    p.push_back (std::make_unique<juce::AudioParameterChoice> (
        "chamberType", "Chamber", juce::StringArray {
            "Chamber 1 (Bright)", "Chamber 3 (Classic)", "Chamber 5 (Deep)", "Chamber 8 (Dense)", "Custom WAV IR"
        }, 1));
    p.push_back (std::make_unique<juce::AudioParameterFloat> ("chamberMix", "Chamber Send", 0.0f, 1.0f, 0.25f));
    p.push_back (std::make_unique<juce::AudioParameterFloat> ("chamberDecay", "Chamber Decay", 0.15f, 1.0f, 0.72f));
    p.push_back (std::make_unique<juce::AudioParameterFloat> ("chamberPreDelay", "Pre-Delay (ms)", 0.0f, 150.0f, 22.0f));
    p.push_back (std::make_unique<juce::AudioParameterFloat> ("chamberWidth", "Chamber Width", 0.0f, 1.5f, 1.0f));

    // Consola Británica, Saturación 4x y Outboard
    p.push_back (std::make_unique<juce::AudioParameterFloat> ("consoleDrive", "Tube / Console Drive", 0.0f, 1.0f, 0.22f));
    p.push_back (std::make_unique<juce::AudioParameterFloat> ("eqHpf", "HPF (Hz)", 20.0f, 300.0f, 20.0f));
    p.push_back (std::make_unique<juce::AudioParameterFloat> ("eqLow", "Low (dB)", -12.0f, 12.0f, 0.0f));
    p.push_back (std::make_unique<juce::AudioParameterFloat> ("eqMid", "Mid (dB)", -12.0f, 12.0f, 0.0f));
    p.push_back (std::make_unique<juce::AudioParameterFloat> ("eqHigh", "High (dB)", -12.0f, 12.0f, 0.0f));

    p.push_back (std::make_unique<juce::AudioParameterChoice> (
        "compType", "Outboard Comp", juce::StringArray { "Bypass", "1176LN (FET)", "LA-2A (Opto)", "Mastering (Tube)" }, 1));
    p.push_back (std::make_unique<juce::AudioParameterFloat> ("compThresh", "Threshold (dB)", -40.0f, 0.0f, -16.0f));

    p.push_back (std::make_unique<juce::AudioParameterChoice> (
        "chainOrder", "Routing", juce::StringArray { "EQ -> Comp -> Chamber", "Comp -> EQ -> Chamber", "Chamber -> Comp -> EQ" }, 0));
    p.push_back (std::make_unique<juce::AudioParameterFloat> ("outputGain", "Output (dB)", -18.0f, 12.0f, 0.0f));

    return { p.begin(), p.end() };
}

void CapitolStudioProProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate;

    srcHpf.prepare (sampleRate); srcLpf.prepare (sampleRate); srcMid.prepare (sampleRate);
    micLow.prepare (sampleRate); micPres.prepare (sampleRate); micAir.prepare (sampleRate);
    chHpf.prepare (sampleRate);  chLpf.prepare (sampleRate);
    eqHpf.prepare (sampleRate);  eqLow.prepare (sampleRate);  eqMid.prepare (sampleRate); eqHigh.prepare (sampleRate);

    analogStage.prepare (samplesPerBlock);
    compressor.prepare (sampleRate);
    acousticEngine.prepare (sampleRate, samplesPerBlock);
    chamberScratch.setSize (2, samplesPerBlock);

    setLatencySamples (analogStage.getLatencySamples());
    updateFilters();
}

void CapitolStudioProProcessor::releaseResources() {}

bool CapitolStudioProProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    return layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
}

void CapitolStudioProProcessor::updateFilters()
{
    // 1. Perfil de Monitores (Bruce Swedien Worldizing)
    const int srcIdx = static_cast<int> (apvts.getRawParameterValue ("source")->load());
    const int monIdx = static_cast<int> (apvts.getRawParameterValue ("monitorType")->load());

    if (srcIdx == 3) // Monitors
    {
        if (monIdx == 0)      { srcHpf.setHighPass (48.0f);  srcLpf.setLowPass (16500.0f); srcMid.setPeak (2200.0f, 1.5f, 1.0f); }
        else if (monIdx == 1) { srcHpf.setHighPass (115.0f); srcLpf.setLowPass (10200.0f); srcMid.setPeak (1750.0f, 4.0f, 1.1f); }
        else                  { srcHpf.setHighPass (82.0f);  srcLpf.setLowPass (13800.0f); srcMid.setPeak (3100.0f, 3.0f, 1.3f); }
    }
    else
    {
        srcHpf.setBypass(); srcLpf.setBypass(); srcMid.setBypass();
    }

    // 2. Mic Locker + Efecto de Proximidad
    const int micIdx = static_cast<int> (apvts.getRawParameterValue ("mic")->load());
    const float dist = apvts.getRawParameterValue ("micY")->load();
    const float proxDb = (1.0f - dist) * 3.5f;

    float presFreq = 3500.0f, presDb = 0.0f, airDb = 0.0f;
    switch (micIdx)
    {
        case 0: presFreq = 3200.0f; presDb = 2.2f; airDb = -0.5f; break; // U48
        case 1: presFreq = 5200.0f; presDb = 1.6f; airDb = 3.8f;  break; // C12
        case 2: presFreq = 2600.0f; presDb = 1.2f; airDb = -1.6f; break; // U67
        case 3: presFreq = 6200.0f; presDb = 2.8f; airDb = 2.2f;  break; // M50
        case 4: presFreq = 4600.0f; presDb = 2.0f; airDb = 4.6f;  break; // C800G
    }

    micLow.setLowShelf (150.0f, proxDb);
    micPres.setPeak (presFreq, presDb, 1.35f);
    micAir.setHighShelf (9800.0f, airDb);

    // 3. Filtros de Consola Británica
    const float hpfFreq = apvts.getRawParameterValue ("eqHpf")->load();
    if (hpfFreq > 22.0f) eqHpf.setHighPass (hpfFreq, 0.7071f);
    else                 eqHpf.setBypass();

    eqLow.setLowShelf (110.0f, apvts.getRawParameterValue ("eqLow")->load());
    eqMid.setPeak (1400.0f, apvts.getRawParameterValue ("eqMid")->load(), 0.9f);
    eqHigh.setHighShelf (8500.0f, apvts.getRawParameterValue ("eqHigh")->load());

    // 4. Filtro paso banda vintage para cámaras
    chHpf.setHighPass (175.0f, 0.7071f);
    chLpf.setLowPass (6800.0f, 0.7071f);
}

void CapitolStudioProProcessor::processSourceAndMonitors (juce::AudioBuffer<float>& buffer)
{
    srcHpf.processBuffer (buffer);
    srcMid.processBuffer (buffer);
    srcLpf.processBuffer (buffer);
}

void CapitolStudioProProcessor::processMicLocker (juce::AudioBuffer<float>& buffer)
{
    micLow.processBuffer (buffer);
    micPres.processBuffer (buffer);
    micAir.processBuffer (buffer);
}

void CapitolStudioProProcessor::processConsoleEQ (juce::AudioBuffer<float>& buffer)
{
    eqHpf.processBuffer (buffer);
    eqLow.processBuffer (buffer);
    eqMid.processBuffer (buffer);
    eqHigh.processBuffer (buffer);
}

void CapitolStudioProProcessor::processChamberStage (juce::AudioBuffer<float>& buffer)
{
    const int chType   = static_cast<int> (apvts.getRawParameterValue ("chamberType")->load());
    const float chMix  = apvts.getRawParameterValue ("chamberMix")->load();
    const float chDec  = apvts.getRawParameterValue ("chamberDecay")->load();
    const float chPre  = apvts.getRawParameterValue ("chamberPreDelay")->load();
    const float chWid  = apvts.getRawParameterValue ("chamberWidth")->load();

    if (chMix <= 0.001f) return;

    chamberScratch.setSize (2, buffer.getNumSamples(), false, false, true);
    acousticEngine.processChamber (buffer, chamberScratch, chType, chMix, chDec, chPre, chWid);
}

void CapitolStudioProProcessor::processCompressorStage (juce::AudioBuffer<float>& buffer)
{
    const int compType = static_cast<int> (apvts.getRawParameterValue ("compType")->load());
    const float thresh = apvts.getRawParameterValue ("compThresh")->load();
    compressor.process (buffer, compType, thresh);
}

void CapitolStudioProProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    updateFilters();

    const int modeIdx     = static_cast<int> (apvts.getRawParameterValue ("mode")->load());
    const int sourceIdx   = static_cast<int> (apvts.getRawParameterValue ("source")->load());
    const int roomIdx     = static_cast<int> (apvts.getRawParameterValue ("room")->load());
    const int wallsLive   = static_cast<int> (apvts.getRawParameterValue ("walls")->load());
    const float micX      = apvts.getRawParameterValue ("micX")->load();
    const float micY      = apvts.getRawParameterValue ("micY")->load();
    const float deccaC    = apvts.getRawParameterValue ("deccaCenter")->load();
    const float deccaS    = apvts.getRawParameterValue ("deccaSides")->load();
    const float drive     = apvts.getRawParameterValue ("consoleDrive")->load();

    // 1. Fuente / Monitores (Swedien)
    processSourceAndMonitors (buffer);

    // 2. Sala 3D + Decca Tree físico
    acousticEngine.processRoomAndDecca (buffer, modeIdx, sourceIdx, roomIdx, wallsLive, micX, micY, deccaC, deccaS);

    // 3. Mic Locker + Saturación Analógica 4x Oversampled
    processMicLocker (buffer);
    analogStage.process (buffer, drive, sourceIdx == 3);

    // 4. Cadena modular reordenable (Consola EQ, Outboard Comp, Cámaras)
    const int order = static_cast<int> (apvts.getRawParameterValue ("chainOrder")->load());
    if (order == 0)
    {
        processConsoleEQ (buffer);
        processCompressorStage (buffer);
        processChamberStage (buffer);
    }
    else if (order == 1)
    {
        processCompressorStage (buffer);
        processConsoleEQ (buffer);
        processChamberStage (buffer);
    }
    else
    {
        processChamberStage (buffer);
        processCompressorStage (buffer);
        processConsoleEQ (buffer);
    }

    // 5. Ganancia de salida y medición VU RMS
    const float outGain = juce::Decibels::decibelsToGain (apvts.getRawParameterValue ("outputGain")->load());
    buffer.applyGain (outGain);

    const float rms = 0.5f * (buffer.getRMSLevel (0, 0, buffer.getNumSamples())
                            + (buffer.getNumChannels() > 1 ? buffer.getRMSLevel (1, 0, buffer.getNumSamples()) : 0.0f));
    outputVuDb.store (juce::Decibels::gainToDecibels (rms + 1e-6f));
}

juce::AudioProcessorEditor* CapitolStudioProProcessor::createEditor()
{
    return new CapitolStudioProEditor (*this);
}

void CapitolStudioProProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml (state.createXml());
    copyXmlToBinary (*xml, destData);
}

void CapitolStudioProProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xmlState (getXmlFromBinary (data, sizeInBytes));
    if (xmlState && xmlState->hasTagName (apvts.state.getType()))
        apvts.replaceState (juce::ValueTree::fromXml (*xmlState));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new CapitolStudioProProcessor();
}
