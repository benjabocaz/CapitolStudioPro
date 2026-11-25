#include "PluginEditor.h"

//==============================================================================
// UADVintageLookAndFeel
//==============================================================================
UADVintageLookAndFeel::UADVintageLookAndFeel()
{
    setColour (juce::ComboBox::backgroundColourId, juce::Colour (0xff15171b));
    setColour (juce::ComboBox::textColourId, juce::Colour (0xffede3cd));
    setColour (juce::ComboBox::outlineColourId, juce::Colour (0xff544c3d));
    setColour (juce::PopupMenu::backgroundColourId, juce::Colour (0xff181a1f));
    setColour (juce::PopupMenu::textColourId, juce::Colour (0xffede3cd));
    setColour (juce::PopupMenu::highlightedBackgroundColourId, juce::Colour (0xffd4a24c));
    setColour (juce::PopupMenu::highlightedTextColourId, juce::Colour (0xff111215));
    setColour (juce::TextButton::textColourOffId, juce::Colour (0xffede3cd));
    setColour (juce::TextButton::textColourOnId, juce::Colour (0xff111215));
}

void UADVintageLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height,
                                              float sliderPos, float rotaryStartAngle, float rotaryEndAngle,
                                              juce::Slider&)
{
    auto bounds = juce::Rectangle<float> (static_cast<float> (x), static_cast<float> (y),
                                          static_cast<float> (width), static_cast<float> (height)).reduced (7.0f);
    const float radius = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f;
    const float cx = bounds.getCentreX();
    const float cy = bounds.getCentreY();
    const float angle = rotaryStartAngle + sliderPos * (rotaryEndAngle - rotaryStartAngle);

    // Arco de escala y nivel en ámbar vintage
    juce::Path bgArc;
    bgArc.addCentredArc (cx, cy, radius + 3.5f, radius + 3.5f, 0.0f, rotaryStartAngle, rotaryEndAngle, true);
    g.setColour (juce::Colour (0xff2c3037));
    g.strokePath (bgArc, juce::PathStrokeType (2.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    juce::Path valArc;
    valArc.addCentredArc (cx, cy, radius + 3.5f, radius + 3.5f, 0.0f, rotaryStartAngle, angle, true);
    g.setColour (juce::Colour (0xffdda74f));
    g.strokePath (valArc, juce::PathStrokeType (2.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    // Sombra realista
    g.setColour (juce::Colours::black.withAlpha (0.6f));
    g.fillEllipse (cx - radius + 1.5f, cy - radius + 3.5f, radius * 2.0f, radius * 2.0f);

    // Cuerpo de baquelita + tapa metálica cepillada
    juce::ColourGradient body (juce::Colour (0xff3c4049), cx - radius * 0.5f, cy - radius * 0.5f,
                               juce::Colour (0xff121417), cx + radius * 0.6f, cy + radius * 0.6f, true);
    g.setGradientFill (body);
    g.fillEllipse (cx - radius, cy - radius, radius * 2.0f, radius * 2.0f);

    juce::ColourGradient cap (juce::Colour (0xff4d515c), cx - radius * 0.3f, cy - radius * 0.4f,
                              juce::Colour (0xff23262c), cx + radius * 0.4f, cy + radius * 0.4f, true);
    g.setGradientFill (cap);
    g.fillEllipse (cx - radius * 0.72f, cy - radius * 0.72f, radius * 1.44f, radius * 1.44f);

    g.setColour (juce::Colour (0xff7a7468).withAlpha (0.5f));
    g.drawEllipse (cx - radius * 0.72f, cy - radius * 0.72f, radius * 1.44f, radius * 1.44f, 1.0f);

    // Puntero crema/blanco
    juce::Path pointer;
    pointer.addRoundedRectangle (-1.5f, -radius * 0.88f, 3.0f, radius * 0.52f, 1.5f);
    pointer.applyTransform (juce::AffineTransform::rotation (angle).translated (cx, cy));
    g.setColour (juce::Colour (0xfff6f0e2));
    g.fillPath (pointer);
}

void UADVintageLookAndFeel::drawComboBox (juce::Graphics& g, int width, int height, bool,
                                          int, int, int, int, juce::ComboBox&)
{
    auto area = juce::Rectangle<float> (0.0f, 0.0f, static_cast<float> (width), static_cast<float> (height)).reduced (0.5f);
    g.setColour (juce::Colour (0xff14161a));
    g.fillRoundedRectangle (area, 4.0f);
    g.setColour (juce::Colour (0xff595040));
    g.drawRoundedRectangle (area, 4.0f, 1.0f);

    juce::Path arrow;
    const float ax = static_cast<float> (width) - 15.0f;
    const float ay = static_cast<float> (height) * 0.5f - 2.0f;
    arrow.addTriangle (ax, ay, ax + 8.0f, ay, ax + 4.0f, ay + 5.0f);
    g.setColour (juce::Colour (0xffdda74f));
    g.fillPath (arrow);
}

void UADVintageLookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& button,
                                                  const juce::Colour&, bool isHighlighted, bool isDown)
{
    auto area = button.getLocalBounds().toFloat().reduced (0.5f);
    g.setColour (isDown ? juce::Colour (0xffd4a24c) : (isHighlighted ? juce::Colour (0xff3a3529) : juce::Colour (0xff23262d)));
    g.fillRoundedRectangle (area, 4.0f);
    g.setColour (juce::Colour (0xffc99a49));
    g.drawRoundedRectangle (area, 4.0f, 1.1f);
}

//==============================================================================
// AnalogVuMeter (Vúmetro Retroiluminado)
//==============================================================================
AnalogVuMeter::AnalogVuMeter (CapitolStudioProProcessor& proc) : processor (proc)
{
    startTimerHz (30);
}

void AnalogVuMeter::timerCallback()
{
    float target = 0.0f;
    if (showGR)
    {
        const float gr = std::clamp (processor.getCompGrDb(), 0.0f, 20.0f);
        target = 1.0f - (gr / 20.0f);
    }
    else
    {
        const float db = std::clamp (processor.getOutputVuDb(), -36.0f, 3.0f);
        target = juce::jmap (db, -36.0f, 3.0f, 0.0f, 1.0f);
    }

    smoothedNeedle = smoothedNeedle * 0.72f + target * 0.28f;
    repaint();
}

void AnalogVuMeter::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat().reduced (1.0f);

    // Marco metálico biselado
    g.setColour (juce::Colour (0xff0e1013));
    g.fillRoundedRectangle (r, 5.0f);
    g.setColour (juce::Colour (0xff5c5342));
    g.drawRoundedRectangle (r, 5.0f, 1.4f);

    // Pantalla ámbar cálida retroiluminada
    auto glass = r.reduced (3.5f);
    juce::ColourGradient lamp (juce::Colour (0xfff6d58e), glass.getCentreX(), glass.getBottom(),
                               juce::Colour (0xffb88332), glass.getCentreX(), glass.getY(), true);
    g.setGradientFill (lamp);
    g.fillRoundedRectangle (glass, 3.0f);

    // Escala del vúmetro
    const float pivotX = glass.getCentreX();
    const float pivotY = glass.getBottom() + 14.0f;
    const float rad = glass.getHeight() + 4.0f;

    juce::Path scaleArc;
    scaleArc.addCentredArc (pivotX, pivotY, rad * 0.85f, rad * 0.85f, 0.0f, -0.75f, 0.75f, true);
    g.setColour (juce::Colour (0xff2b1f0d));
    g.strokePath (scaleArc, juce::PathStrokeType (1.4f));

    // Zona roja (+0 a +3 dB)
    juce::Path redArc;
    redArc.addCentredArc (pivotX, pivotY, rad * 0.85f, rad * 0.85f, 0.0f, 0.35f, 0.75f, true);
    g.setColour (juce::Colour (0xffb52318));
    g.strokePath (redArc, juce::PathStrokeType (2.4f));

    g.setColour (juce::Colour (0xff2b1f0d));
    g.setFont (juce::Font (juce::FontOptions (9.5f).withStyle ("Bold")));
    g.drawText (showGR ? "GR  dB" : "VU  OUT", glass.reduced (6.0f, 3.0f), juce::Justification::bottomLeft);
    g.drawText ("-20   -10   -5   0  +3", glass.reduced (8.0f, 4.0f), juce::Justification::centredTop);

    // Aguja analógica
    const float needleAngle = juce::jmap (smoothedNeedle, 0.0f, 1.0f, -0.75f, 0.75f);
    juce::Path needle;
    needle.startNewSubPath (pivotX, pivotY);
    needle.lineTo (pivotX + std::sin (needleAngle) * (rad * 0.92f),
                   pivotY - std::cos (needleAngle) * (rad * 0.92f));
    g.setColour (juce::Colour (0xff18120a));
    g.strokePath (needle, juce::PathStrokeType (1.8f));
}

//==============================================================================
// Studio2DRoomPad (Arrastrable en X y Y con Decca Tree + Monitores Swedien)
//==============================================================================
Studio2DRoomPad::Studio2DRoomPad (juce::AudioProcessorValueTreeState& apvtsRef) : apvts (apvtsRef)
{
    startTimerHz (24);
}

void Studio2DRoomPad::paint (juce::Graphics& g)
{
    auto area = getLocalBounds().toFloat().reduced (2.0f);

    const int roomIdx   = static_cast<int> (apvts.getRawParameterValue ("room")->load());
    const int wallsLive = static_cast<int> (apvts.getRawParameterValue ("walls")->load());
    const int sourceIdx = static_cast<int> (apvts.getRawParameterValue ("source")->load());
    const float micX    = apvts.getRawParameterValue ("micX")->load();
    const float micY    = apvts.getRawParameterValue ("micY")->load();
    const float sides   = apvts.getRawParameterValue ("deccaSides")->load();

    // Piso arquitectónico de estudio
    juce::ColourGradient floorGrad (juce::Colour (0xff1a1d23), area.getTopLeft(),
                                    juce::Colour (0xff0f1115), area.getBottomRight(), false);
    g.setGradientFill (floorGrad);
    g.fillRoundedRectangle (area, 6.0f);

    // Cuadrícula acústica
    g.setColour (juce::Colour (0xff2b303a).withAlpha (0.5f));
    for (float x = area.getX() + 26.0f; x < area.getRight(); x += 26.0f)
        g.drawVerticalLine (static_cast<int> (x), area.getY() + 4.0f, area.getBottom() - 4.0f);
    for (float y = area.getY() + 26.0f; y < area.getBottom(); y += 26.0f)
        g.drawHorizontalLine (static_cast<int> (y), area.getX() + 4.0f, area.getRight() - 4.0f);

    // Indicador visual de paredes
    g.setColour (wallsLive ? juce::Colour (0xffc99a49) : juce::Colour (0xff48515e));
    g.drawRoundedRectangle (area, 6.0f, wallsLive ? 2.2f : 1.5f);

    // Etiqueta de sala
    g.setColour (juce::Colour (0xffdda74f).withAlpha (0.9f));
    g.setFont (juce::Font (juce::FontOptions (10.5f).withStyle ("Bold")));
    juce::String title = (roomIdx == 0 ? "STUDIO A (16x12m)" : "STUDIO B (10x8m)");
    title += (wallsLive ? "  •  LIVE WALLS" : "  •  TIGHT WALLS");
    g.drawText (title, area.reduced (10.0f).removeFromTop (15.0f), juce::Justification::centredLeft);

    // Fuente sonora en la parte superior
    const float srcX = area.getCentreX();
    const float srcY = area.getY() + 42.0f;

    if (sourceIdx == 3) // Par de Monitores (Swedien)
    {
        g.setColour (juce::Colour (0xffdda74f).withAlpha (0.22f));
        g.fillEllipse (srcX - 46.0f, srcY - 14.0f, 92.0f, 30.0f);
        g.setColour (juce::Colour (0xffede3cd));
        g.drawRoundedRectangle (srcX - 38.0f, srcY - 10.0f, 22.0f, 18.0f, 2.5f, 1.3f);
        g.drawRoundedRectangle (srcX + 16.0f, srcY - 10.0f, 22.0f, 18.0f, 2.5f, 1.3f);
        g.setFont (juce::Font (juce::FontOptions (9.0f).withStyle ("Bold")));
        g.drawText ("MON L/R", static_cast<int> (srcX - 36), static_cast<int> (srcY + 10), 72, 12, juce::Justification::centred);
    }
    else
    {
        g.setColour (juce::Colour (0xffdda74f).withAlpha (0.22f));
        g.fillEllipse (srcX - 28.0f, srcY - 14.0f, 56.0f, 28.0f);
        g.setColour (juce::Colour (0xffede3cd));
        g.drawRoundedRectangle (srcX - 26.0f, srcY - 10.0f, 52.0f, 20.0f, 3.0f, 1.3f);
        juce::String lbl = (sourceIdx == 0 ? "VOCALS" : (sourceIdx == 1 ? "DRUMS" : "ENSEMBLE"));
        g.setFont (juce::Font (juce::FontOptions (9.0f).withStyle ("Bold")));
        g.drawText (lbl, static_cast<int> (srcX - 26), static_cast<int> (srcY - 10), 52, 20, juce::Justification::centred);
    }

    // Posición 2D del micrófono + Triángulo Decca Tree
    const float mX = juce::jmap (micX, -1.0f, 1.0f, area.getX() + 48.0f, area.getRight() - 48.0f);
    const float mY = juce::jmap (micY,  0.0f, 1.0f, area.getY() + 74.0f, area.getBottom() - 26.0f);
    const float spread = juce::jmap (sides, 0.0f, 1.5f, 10.0f, 44.0f);

    // Rayos acústicos desde la fuente
    g.setColour (juce::Colour (0xffdda74f).withAlpha (0.28f));
    g.drawLine (srcX, srcY + 12.0f, mX - spread, mY + 6.0f, 1.1f);
    g.drawLine (srcX, srcY + 12.0f, mX + spread, mY + 6.0f, 1.1f);
    g.drawLine (srcX, srcY + 12.0f, mX, mY - 8.0f, 1.1f);

    // Barra del Decca Tree
    g.setColour (juce::Colour (0xff8c8270));
    g.drawLine (mX - spread, mY + 6.0f, mX + spread, mY + 6.0f, 1.5f);
    g.drawLine (mX, mY - 8.0f, mX, mY + 6.0f, 1.5f);

    // Cápsulas L y R
    g.setColour (juce::Colour (0xffb5a485));
    g.fillEllipse (mX - spread - 4.0f, mY + 2.0f, 8.0f, 8.0f);
    g.fillEllipse (mX + spread - 4.0f, mY + 2.0f, 8.0f, 8.0f);

    // Cápsula Central (arrastrable)
    g.setColour (juce::Colour (0xffdda74f));
    g.fillEllipse (mX - 8.5f, mY - 16.5f, 17.0f, 17.0f);
    g.setColour (juce::Colour (0xff15171b));
    g.fillEllipse (mX - 3.5f, mY - 11.5f, 7.0f, 7.0f);

    g.setColour (juce::Colour (0xffede3cd).withAlpha (0.6f));
    g.setFont (juce::Font (juce::FontOptions (9.5f)));
    g.drawText ("DRAG MIC IN 2D (DISTANCE & POSITION)", area.reduced (6.0f).removeFromBottom (13.0f), juce::Justification::centred);
}

void Studio2DRoomPad::mouseDown (const juce::MouseEvent& e) { updateMicFromMouse (e); }
void Studio2DRoomPad::mouseDrag (const juce::MouseEvent& e) { updateMicFromMouse (e); }

void Studio2DRoomPad::updateMicFromMouse (const juce::MouseEvent& e)
{
    auto area = getLocalBounds().toFloat().reduced (2.0f);
    const float normX = juce::jlimit (0.0f, 1.0f, juce::jmap (static_cast<float> (e.x), area.getX() + 48.0f, area.getRight() - 48.0f, 0.0f, 1.0f));
    const float normY = juce::jlimit (0.0f, 1.0f, juce::jmap (static_cast<float> (e.y), area.getY() + 74.0f, area.getBottom() - 26.0f, 0.0f, 1.0f));

    if (auto* pX = apvts.getParameter ("micX")) pX->setValueNotifyingHost (normX);
    if (auto* pY = apvts.getParameter ("micY")) pY->setValueNotifyingHost (normY);
}

//==============================================================================
// CapitolStudioProEditor
//==============================================================================
CapitolStudioProEditor::CapitolStudioProEditor (CapitolStudioProProcessor& p)
    : AudioProcessorEditor (&p), audioProcessor (p), vuMeter (p), roomPad (p.apvts)
{
    setLookAndFeel (&uadLAF);
    setSize (1040, 640);

    addAndMakeVisible (vuMeter);
    addAndMakeVisible (roomPad);

    // Botón VU (OUT / GR)
    vuModeBtn.onClick = [this]
    {
        const bool next = ! vuMeter.isShowingGainReduction();
        vuMeter.setShowGainReduction (next);
        vuModeBtn.setButtonText (next ? "VU: GR" : "VU: OUT");
    };
    addAndMakeVisible (vuModeBtn);

    // Botón Cargador de IR Externo (.wav)
    loadIRBtn.onClick = [this] { openIRFileChooser(); };
    addAndMakeVisible (loadIRBtn);

    irNameLbl.setText (audioProcessor.getLoadedIRName(), juce::dontSendNotification);
    irNameLbl.setJustificationType (juce::Justification::centredLeft);
    irNameLbl.setColour (juce::Label::textColourId, juce::Colour (0xffb8ae99));
    irNameLbl.setFont (juce::Font (juce::FontOptions (10.5f)));
    addAndMakeVisible (irNameLbl);

    // Selectores
    setupCombo (modeBox,    { "Reverb", "Re-Mic" }, modeLbl, "MODE");
    setupCombo (sourceBox,  { "Vocals", "Drums", "Ensemble", "Monitors (Swedien)" }, sourceLbl, "SOURCE");
    setupCombo (monitorBox, { "Altec Main", "NS-10 Nearfield", "JBL PA Wedge" }, monitorLbl, "MONITOR SPEAKER");
    setupCombo (roomBox,    { "Studio A (Large)", "Studio B (Tight)" }, roomLbl, "STUDIO ROOM");
    setupCombo (wallsBox,   { "Tight / Controlled", "Live / Reflective" }, wallsLbl, "ROOM WALLS");
    setupCombo (micBox,     { "U48 (Tube)", "C12 (Air)", "U67 (Warm)", "M50 (Decca)", "C800G (Modern)" }, micLbl, "MIC LOCKER");
    setupCombo (chamberBox, { "Chamber 1 (Bright)", "Chamber 3 (Classic)", "Chamber 5 (Deep)", "Chamber 8 (Dense)", "Custom WAV IR" }, chamberLbl, "ECHO CHAMBER");
    setupCombo (compBox,    { "Bypass", "1176LN (FET)", "LA-2A (Opto)", "Mastering (Tube)" }, compLbl, "OUTBOARD COMP");
    setupCombo (chainBox,   { "EQ -> Comp -> Chamber", "Comp -> EQ -> Chamber", "Chamber -> Comp -> EQ" }, chainLbl, "SIGNAL ROUTING");

    // Perillas
    setupKnob (driveSlider,       driveLbl,       "TUBE DRIVE");
    setupKnob (micYSlider,        micYLbl,        "DISTANCE");
    setupKnob (deccaCenterSlider, deccaCenterLbl, "DECCA CTR");
    setupKnob (deccaSidesSlider,  deccaSidesLbl,  "DECCA SIDES");
    setupKnob (chMixSlider,       chMixLbl,       "SEND");
    setupKnob (chDecaySlider,     chDecayLbl,     "DECAY");
    setupKnob (chPreSlider,       chPreLbl,       "PRE-DELAY");
    setupKnob (chWidthSlider,     chWidthLbl,     "WIDTH");
    setupKnob (eqHpfSlider,       eqHpfLbl,       "HPF");
    setupKnob (eqLowSlider,       eqLowLbl,       "LOW");
    setupKnob (eqMidSlider,       eqMidLbl,       "MID");
    setupKnob (eqHighSlider,      eqHighLbl,      "HIGH");
    setupKnob (compThreshSlider,  compThreshLbl,  "THRESHOLD");
    setupKnob (outGainSlider,     outGainLbl,     "OUTPUT");

    // Attachments
    auto& v = audioProcessor.apvts;
    modeAtt        = std::make_unique<ComboAtt>  (v, "mode", modeBox);
    sourceAtt      = std::make_unique<ComboAtt>  (v, "source", sourceBox);
    monitorAtt     = std::make_unique<ComboAtt>  (v, "monitorType", monitorBox);
    roomAtt        = std::make_unique<ComboAtt>  (v, "room", roomBox);
    wallsAtt       = std::make_unique<ComboAtt>  (v, "walls", wallsBox);
    micAtt         = std::make_unique<ComboAtt>  (v, "mic", micBox);
    chamberAtt     = std::make_unique<ComboAtt>  (v, "chamberType", chamberBox);
    compAtt        = std::make_unique<ComboAtt>  (v, "compType", compBox);
    chainAtt       = std::make_unique<ComboAtt>  (v, "chainOrder", chainBox);

    driveAtt       = std::make_unique<SliderAtt> (v, "consoleDrive", driveSlider);
    micYAtt        = std::make_unique<SliderAtt> (v, "micY", micYSlider);
    deccaCenterAtt = std::make_unique<SliderAtt> (v, "deccaCenter", deccaCenterSlider);
    deccaSidesAtt  = std::make_unique<SliderAtt> (v, "deccaSides", deccaSidesSlider);
    chMixAtt       = std::make_unique<SliderAtt> (v, "chamberMix", chMixSlider);
    chDecayAtt     = std::make_unique<SliderAtt> (v, "chamberDecay", chDecaySlider);
    chPreAtt       = std::make_unique<SliderAtt> (v, "chamberPreDelay", chPreSlider);
    chWidthAtt     = std::make_unique<SliderAtt> (v, "chamberWidth", chWidthSlider);
    eqHpfAtt       = std::make_unique<SliderAtt> (v, "eqHpf", eqHpfSlider);
    eqLowAtt       = std::make_unique<SliderAtt> (v, "eqLow", eqLowSlider);
    eqMidAtt       = std::make_unique<SliderAtt> (v, "eqMid", eqMidSlider);
    eqHighAtt      = std::make_unique<SliderAtt> (v, "eqHigh", eqHighSlider);
    compThreshAtt  = std::make_unique<SliderAtt> (v, "compThresh", compThreshSlider);
    outGainAtt     = std::make_unique<SliderAtt> (v, "outputGain", outGainSlider);
}

CapitolStudioProEditor::~CapitolStudioProEditor()
{
    setLookAndFeel (nullptr);
}

void CapitolStudioProEditor::openIRFileChooser()
{
    fileChooser = std::make_unique<juce::FileChooser> ("Select Studio or Chamber IR (.wav)", juce::File{}, "*.wav;*.aiff;*.aif");
    auto flags = juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles;

    fileChooser->launchAsync (flags, [this] (const juce::FileChooser& fc)
    {
        auto file = fc.getResult();
        if (file.existsAsFile())
        {
            audioProcessor.loadCustomIR (file);
            irNameLbl.setText (audioProcessor.getLoadedIRName(), juce::dontSendNotification);
            chamberBox.setSelectedItemIndex (4, juce::sendNotification);
        }
    });
}

void CapitolStudioProEditor::setupKnob (juce::Slider& slider, juce::Label& label, const juce::String& text)
{
    slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 58, 15);
    slider.setColour (juce::Slider::textBoxTextColourId, juce::Colour (0xffd6ccb6));
    slider.setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    addAndMakeVisible (slider);

    label.setText (text, juce::dontSendNotification);
    label.setJustificationType (juce::Justification::centred);
    label.setColour (juce::Label::textColourId, juce::Colour (0xffdda74f));
    label.setFont (juce::Font (juce::FontOptions (10.0f).withStyle ("Bold")));
    addAndMakeVisible (label);
}

void CapitolStudioProEditor::setupCombo (juce::ComboBox& box, const juce::StringArray& items, juce::Label& label, const juce::String& text)
{
    box.addItemList (items, 1);
    addAndMakeVisible (box);

    label.setText (text, juce::dontSendNotification);
    label.setJustificationType (juce::Justification::centredLeft);
    label.setColour (juce::Label::textColourId, juce::Colour (0xffb8ae99));
    label.setFont (juce::Font (juce::FontOptions (9.5f).withStyle ("Bold")));
    addAndMakeVisible (label);
}

void CapitolStudioProEditor::paint (juce::Graphics& g)
{
    auto bounds = getLocalBounds();

    // 1. Laterales de madera de nogal (estilo consolas UAD)
    auto drawWood = [&] (juce::Rectangle<int> r)
    {
        juce::ColourGradient wood (juce::Colour (0xff56341d), static_cast<float> (r.getX()), 0.0f,
                                   juce::Colour (0xff2a1509), static_cast<float> (r.getRight()), 0.0f, false);
        g.setGradientFill (wood);
        g.fillRect (r);
        g.setColour (juce::Colours::black.withAlpha (0.28f));
        for (int x = r.getX() + 4; x < r.getRight(); x += 5)
            g.drawVerticalLine (x, static_cast<float> (r.getY()), static_cast<float> (r.getBottom()));
    };
    drawWood (bounds.removeFromLeft (24));
    drawWood (bounds.removeFromRight (24));

    // 2. Chasis metálico oscuro cepillado
    juce::ColourGradient chassis (juce::Colour (0xff272b33), 0.0f, 0.0f,
                                  juce::Colour (0xff15181d), 0.0f, static_cast<float> (getHeight()), false);
    g.setGradientFill (chassis);
    g.fillRect (bounds);

    // 3. Barra superior de consola
    auto header = bounds.removeFromTop (72);
    g.setColour (juce::Colour (0xff111317));
    g.fillRect (header);
    g.setColour (juce::Colour (0xffdda74f).withAlpha (0.45f));
    g.drawHorizontalLine (header.getBottom() - 1, static_cast<float> (header.getX()), static_cast<float> (header.getRight()));

    g.setColour (juce::Colour (0xffede3cd));
    g.setFont (juce::Font (juce::FontOptions (21.0f).withStyle ("Bold")));
    g.drawText ("CAPITOL STUDIO PRO", header.reduced (16, 10).removeFromTop (28), juce::Justification::centredLeft);

    g.setColour (juce::Colour (0xffdda74f));
    g.setFont (juce::Font (juce::FontOptions (10.5f).withStyle ("Bold")));
    g.drawText ("4X OVERSAMPLED  •  DECCA TREE  •  8-CH HADAMARD CHAMBERS",
                header.reduced (16, 0).withTrimmedTop (40), juce::Justification::centredLeft);

    // 4. Módulos tipo Rack con tornillos
    auto drawPanel = [&] (juce::Rectangle<int> r, const juce::String& title)
    {
        g.setColour (juce::Colour (0xff1c1f25));
        g.fillRoundedRectangle (r.toFloat(), 6.0f);
        g.setColour (juce::Colour (0xff3a3e47));
        g.drawRoundedRectangle (r.toFloat(), 6.0f, 1.1f);

        // Tornillos metálicos en esquinas
        g.setColour (juce::Colour (0xff545963));
        const float rad = 3.0f;
        g.fillEllipse (static_cast<float> (r.getX() + 6), static_cast<float> (r.getY() + 6), rad * 2, rad * 2);
        g.fillEllipse (static_cast<float> (r.getRight() - 12), static_cast<float> (r.getY() + 6), rad * 2, rad * 2);

        g.setColour (juce::Colour (0xffdda74f));
        g.setFont (juce::Font (juce::FontOptions (11.0f).withStyle ("Bold")));
        g.drawText (title, r.reduced (18, 5).removeFromTop (16), juce::Justification::centredLeft);
    };

    drawPanel ({ 36,  84, 270, 330 }, "SOURCE, MONITORS & MIC LOCKER");
    drawPanel ({ 318, 84, 380, 330 }, "DYNAMIC 2D ROOM & DECCA TREE");
    drawPanel ({ 710, 84, 294, 330 }, "UNDERGROUND CHAMBERS & IR");
    drawPanel ({ 36, 426, 968, 196 }, "BRITISH CONSOLE EQ & OUTBOARD RACK");
}

void CapitolStudioProEditor::resized()
{
    // Header: Modo, Ruteo y Vúmetro analógico
    modeLbl.setBounds  (415, 14, 90, 14);  modeBox.setBounds  (415, 30, 105, 26);
    chainLbl.setBounds (530, 14, 150, 14); chainBox.setBounds (530, 30, 185, 26);
    vuMeter.setBounds  (730, 8,  188, 56);
    vuModeBtn.setBounds (926, 22, 74, 28);

    // Panel Izquierdo: Source, Monitors, Room, Walls, Mic Locker + Tube Drive
    sourceLbl.setBounds  (50, 108, 160, 13); sourceBox.setBounds  (50, 122, 242, 23);
    monitorLbl.setBounds (50, 150, 160, 13); monitorBox.setBounds (50, 164, 242, 23);
    roomLbl.setBounds    (50, 192, 160, 13); roomBox.setBounds    (50, 206, 242, 23);
    wallsLbl.setBounds   (50, 234, 160, 13); wallsBox.setBounds   (50, 248, 242, 23);
    micLbl.setBounds     (50, 276, 160, 13); micBox.setBounds     (50, 290, 242, 23);

    driveLbl.setBounds    (120, 322, 100, 14);
    driveSlider.setBounds (132, 336, 76, 72);

    // Panel Central: Plano 2D + Distance & Decca Center/Sides
    roomPad.setBounds (330, 108, 356, 212);

    micYLbl.setBounds        (344, 326, 86, 14); micYSlider.setBounds        (349, 340, 76, 68);
    deccaCenterLbl.setBounds (465, 326, 86, 14); deccaCenterSlider.setBounds (470, 340, 76, 68);
    deccaSidesLbl.setBounds  (586, 326, 86, 14); deccaSidesSlider.setBounds  (591, 340, 76, 68);

    // Panel Derecho: Cámaras 1/3/5/8 + Custom IR + 4 Perillas
    chamberLbl.setBounds (724, 108, 160, 13); chamberBox.setBounds (724, 122, 266, 24);
    loadIRBtn.setBounds  (724, 152, 114, 24); irNameLbl.setBounds  (844, 152, 146, 24);

    chMixLbl.setBounds   (746, 188, 92, 14); chMixSlider.setBounds   (750, 202, 84, 86);
    chDecayLbl.setBounds (874, 188, 92, 14); chDecaySlider.setBounds (878, 202, 84, 86);
    chPreLbl.setBounds   (746, 300, 92, 14); chPreSlider.setBounds   (750, 314, 84, 86);
    chWidthLbl.setBounds (874, 300, 92, 14); chWidthSlider.setBounds (878, 314, 84, 86);

    // Panel Inferior: British Console EQ + Outboard Comp + Output
    eqHpfLbl.setBounds  (54,  466, 88, 14); eqHpfSlider.setBounds  (56,  482, 86, 102);
    eqLowLbl.setBounds  (160, 466, 88, 14); eqLowSlider.setBounds  (162, 482, 86, 102);
    eqMidLbl.setBounds  (266, 466, 88, 14); eqMidSlider.setBounds  (268, 482, 86, 102);
    eqHighLbl.setBounds (372, 466, 88, 14); eqHighSlider.setBounds (374, 482, 86, 102);

    compLbl.setBounds       (512, 492, 150, 14); compBox.setBounds       (512, 510, 178, 28);
    compThreshLbl.setBounds (716, 466, 94, 14);  compThreshSlider.setBounds (720, 482, 86, 102);

    outGainLbl.setBounds    (876, 466, 94, 14);  outGainSlider.setBounds (880, 482, 86, 102);
}
