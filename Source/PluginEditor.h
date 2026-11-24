#pragma once
#include "PluginProcessor.h"

// Estilo visual hardware vintage / Universal Audio
class UADVintageLookAndFeel : public juce::LookAndFeel_V4
{
public:
    UADVintageLookAndFeel();

    void drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height,
                           float sliderPos, float rotaryStartAngle, float rotaryEndAngle,
                           juce::Slider& slider) override;

    void drawComboBox (juce::Graphics& g, int width, int height, bool isButtonDown,
                       int buttonX, int buttonY, int buttonW, int buttonH,
                       juce::ComboBox& box) override;

    void drawButtonBackground (juce::Graphics& g, juce::Button& button,
                               const juce::Colour& backgroundColour,
                               bool isHighlighted, bool isDown) override;
};

// Vúmetro analógico de aguja retroiluminado (OUT / GR)
class AnalogVuMeter : public juce::Component, private juce::Timer
{
public:
    explicit AnalogVuMeter (CapitolStudioProProcessor& proc);
    void paint (juce::Graphics& g) override;
    void setShowGainReduction (bool showGr) { showGR = showGr; }
    bool isShowingGainReduction() const { return showGR; }

private:
    void timerCallback() override;

    CapitolStudioProProcessor& processor;
    bool showGR = false;
    float smoothedNeedle = 0.0f;
};

// Plano interactivo 2D de Studio A / B con movimiento libre X/Y + Decca Tree
class Studio2DRoomPad : public juce::Component, private juce::Timer
{
public:
    explicit Studio2DRoomPad (juce::AudioProcessorValueTreeState& apvtsRef);
    void paint (juce::Graphics& g) override;
    void mouseDown (const juce::MouseEvent& e) override;
    void mouseDrag (const juce::MouseEvent& e) override;

private:
    void timerCallback() override { repaint(); }
    void updateMicFromMouse (const juce::MouseEvent& e);

    juce::AudioProcessorValueTreeState& apvts;
};

class CapitolStudioProEditor : public juce::AudioProcessorEditor
{
public:
    explicit CapitolStudioProEditor (CapitolStudioProProcessor&);
    ~CapitolStudioProEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void setupKnob (juce::Slider& slider, juce::Label& label, const juce::String& text);
    void setupCombo (juce::ComboBox& box, const juce::StringArray& items, juce::Label& label, const juce::String& text);
    void openIRFileChooser();

    CapitolStudioProProcessor& audioProcessor;
    UADVintageLookAndFeel uadLAF;
    AnalogVuMeter vuMeter;
    Studio2DRoomPad roomPad;

    juce::TextButton vuModeBtn { "VU: OUT" };
    juce::TextButton loadIRBtn { "LOAD IR (.WAV)" };
    juce::Label irNameLbl;
    std::unique_ptr<juce::FileChooser> fileChooser;

    // Selectores
    juce::ComboBox modeBox, sourceBox, monitorBox, roomBox, wallsBox, micBox, chamberBox, compBox, chainBox;
    juce::Label modeLbl, sourceLbl, monitorLbl, roomLbl, wallsLbl, micLbl, chamberLbl, compLbl, chainLbl;

    // Perillas
    juce::Slider driveSlider, micYSlider, deccaCenterSlider, deccaSidesSlider;
    juce::Slider chMixSlider, chDecaySlider, chPreSlider, chWidthSlider;
    juce::Slider eqHpfSlider, eqLowSlider, eqMidSlider, eqHighSlider, compThreshSlider, outGainSlider;

    juce::Label driveLbl, micYLbl, deccaCenterLbl, deccaSidesLbl;
    juce::Label chMixLbl, chDecayLbl, chPreLbl, chWidthLbl;
    juce::Label eqHpfLbl, eqLowLbl, eqMidLbl, eqHighLbl, compThreshLbl, outGainLbl;

    using ComboAtt  = juce::AudioProcessorValueTreeState::ComboBoxAttachment;
    using SliderAtt = juce::AudioProcessorValueTreeState::SliderAttachment;

    std::unique_ptr<ComboAtt> modeAtt, sourceAtt, monitorAtt, roomAtt, wallsAtt, micAtt, chamberAtt, compAtt, chainAtt;
    std::unique_ptr<SliderAtt> driveAtt, micYAtt, deccaCenterAtt, deccaSidesAtt;
    std::unique_ptr<SliderAtt> chMixAtt, chDecayAtt, chPreAtt, chWidthAtt;
    std::unique_ptr<SliderAtt> eqHpfAtt, eqLowAtt, eqMidAtt, eqHighAtt, compThreshAtt, outGainAtt;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CapitolStudioProEditor)
};
