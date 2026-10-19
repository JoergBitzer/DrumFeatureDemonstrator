
#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "PluginSettings.h"

//==============================================================================
#if WITH_MIDIKEYBOARD   
DrumFeatureDemonstratorAudioProcessorEditor::DrumFeatureDemonstratorAudioProcessorEditor (DrumFeatureDemonstratorAudioProcessor& p)
    : AudioProcessorEditor (&p), m_processorRef (p), m_presetGUI(p.m_presets),
    	m_keyboard(m_processorRef.m_keyboardState, MidiKeyboardComponent::Orientation::horizontalKeyboard), 
        m_wheels(p.m_wheelState), m_editor(p,*p.m_parameterVTS)
#else
DrumFeatureDemonstratorAudioProcessorEditor::DrumFeatureDemonstratorAudioProcessorEditor (DrumFeatureDemonstratorAudioProcessor& p)
    : AudioProcessorEditor (&p), m_processorRef (p), m_presetGUI(p.m_presets), m_editor(p,*p.m_parameterVTS)
#endif
{
    float scaleFactor = m_processorRef.getScaleFactor();
    setResizeLimits (g_minGuiSize_x,static_cast<int>(g_minGuiSize_x*g_guiratio) , g_maxGuiSize_x, static_cast<int>(g_maxGuiSize_x*g_guiratio));
    setResizable(true,true);
    getConstrainer()->setFixedAspectRatio(1./g_guiratio);
    setSize (static_cast<int>(scaleFactor*g_minGuiSize_x), static_cast<int>(scaleFactor*g_minGuiSize_x*g_guiratio));

	addAndMakeVisible(m_presetGUI);
#if WITH_MIDIKEYBOARD      
	m_keyboard.setOctaveForMiddleC(g_octaveOfMiddleC); // note 60 is shown as C4, like the base note and the step notes
	addAndMakeVisible(m_keyboard);
    addAndMakeVisible(m_wheels);    
#endif

    // from here your algo editor ---------
    addAndMakeVisible(m_editor);
    addKeyListener(&m_editor);       // cursor keys: select the step
    setWantsKeyboardFocus(true);

    for (auto* parameter : m_processorRef.getParameters())
        parameter->addListener(this);
    m_processorRef.m_parameterVTS->state.addListener(this);

#if WITH_DAYNIGHT
    bool presetFolderWasCreated;
    m_userSettings = jade::createUserSettings(m_processorRef.m_presets.getUserPresetsFolder(presetFolderWasCreated));
    const auto theme = m_userSettings->getBoolValue("dayTheme", false) ? jade::DayNightLookAndFeel::Theme::Day
                                                                        : jade::DayNightLookAndFeel::Theme::Night;
    m_lookAndFeel.setTheme(theme);
    m_themeButton.setCurrentTheme(theme);
    m_themeButton.onClick = [this]() { toggleTheme(); };
    addAndMakeVisible(m_themeButton);
    // last, after all children are added: then all of them get the new colours
    setLookAndFeel(&m_lookAndFeel);
#endif
}

DrumFeatureDemonstratorAudioProcessorEditor::~DrumFeatureDemonstratorAudioProcessorEditor()
{
    removeKeyListener(&m_editor);
#if WITH_DAYNIGHT
    setLookAndFeel(nullptr);
#endif
    m_processorRef.m_parameterVTS->state.removeListener(this);
    for (auto* parameter : m_processorRef.getParameters())
        parameter->removeListener(this);
}

void DrumFeatureDemonstratorAudioProcessorEditor::valueTreePropertyChanged(juce::ValueTree& tree, const juce::Identifier& property)
{
    juce::ignoreUnused(tree);
    if (property != juce::Identifier("PatternData"))
        return;
    juce::MessageManager::callAsync([safeThis = juce::Component::SafePointer<DrumFeatureDemonstratorAudioProcessorEditor>(this)]
    {
        if (safeThis != nullptr)
            safeThis->m_presetGUI.setSomethingChanged();
    });
}

void DrumFeatureDemonstratorAudioProcessorEditor::parameterGestureChanged(int, bool gestureIsStarting)
{
    if (!gestureIsStarting)
        return;
    juce::MessageManager::callAsync([safeThis = juce::Component::SafePointer<DrumFeatureDemonstratorAudioProcessorEditor>(this)]
    {
        if (safeThis != nullptr)
            safeThis->m_presetGUI.setSomethingChanged();
    });
}

#if WITH_DAYNIGHT
void DrumFeatureDemonstratorAudioProcessorEditor::toggleTheme()
{
    const bool day = m_lookAndFeel.getTheme() != jade::DayNightLookAndFeel::Theme::Day;
    const auto theme = day ? jade::DayNightLookAndFeel::Theme::Day : jade::DayNightLookAndFeel::Theme::Night;
    m_lookAndFeel.setTheme(theme);
    m_themeButton.setCurrentTheme(theme);
    m_userSettings->setValue("dayTheme", day);
    sendLookAndFeelChange(); // all components take the new colours
    repaint();
}
#endif

//==============================================================================
void DrumFeatureDemonstratorAudioProcessorEditor::paint (juce::Graphics& g)
{
    // (Our component is opaque, so we must completely fill the background with a solid colour)
    g.fillAll (getLookAndFeel().findColour (juce::ResizableWindow::backgroundColourId));

    g.setColour (getLookAndFeel().findColour (juce::Label::textColourId));
    g.setFont (15.0f);
    juce::String text2display = "Hello World! V " + juce::String(PLUGIN_VERSION_MAJOR) + "." + juce::String(PLUGIN_VERSION_MINOR) + "." + juce::String(PLUGIN_VERSION_PATCH);
    g.drawFittedText (text2display, getLocalBounds(), juce::Justification::centred, 1);
}
void DrumFeatureDemonstratorAudioProcessorEditor::resized()
{
    int height = getHeight();
    // necessary to change fontsize of comboboxes and PopUpmenus
    // 0.5 is a good compromisecould be slightly higher or lower
    // m_jadeLAF.setFontSize(0.5*height*g_minPresetHandlerHeight/g_minGuiSize_y);
    // top presethandler
#if WITH_PRESETHANDLERGUI    
    const int presetHeight = height*g_minPresetHandlerHeight/g_minGuiSize_y;
  #if WITH_DAYNIGHT
    // day/night button at the right end of the preset bar
    m_themeButton.setBounds(getWidth() - presetHeight, 2, presetHeight - 4, presetHeight - 4);
    m_presetGUI.setBounds(0, 0, getWidth() - presetHeight, presetHeight);
  #else
    m_presetGUI.setBounds(0, 0, getWidth(), presetHeight);
  #endif
#endif
    // bottom a small midkeyboard
#if WITH_MIDIKEYBOARD    
    m_wheels.setBounds(0, static_cast<int> (height*(1-g_midikeyboardratio)),  static_cast<int> (g_wheelstokeyboardratio*getWidth()),  static_cast<int> (height*g_midikeyboardratio));
    m_keyboard.setBounds(static_cast<int> (g_wheelstokeyboardratio*getWidth()), static_cast<int> (height*(1-g_midikeyboardratio)), static_cast<int> ((1.0-g_wheelstokeyboardratio)*getWidth()),static_cast<int> ( height*g_midikeyboardratio));
#endif
    // This is generally where you'll want to lay out the positions of any
    // subcomponents in your editor..

    int width = getWidth();
	float scaleFactor = float(width)/g_minGuiSize_x;
    m_processorRef.setScaleFactor(scaleFactor);
    // use setBounds with scaleFactor
#if WITH_MIDIKEYBOARD    
    #if WITH_PRESETHANDLERGUI    
    m_editor.setBounds(0, static_cast<int> (height*g_minPresetHandlerHeight/g_minGuiSize_y + 1), 
                        getWidth(), static_cast<int> (height*(1-g_midikeyboardratio) - (height*g_minPresetHandlerHeight/g_minGuiSize_y + 1) ));
    #else
    m_editor.setBounds(0, 0, getWidth(), static_cast<int> (height*(1-g_midikeyboardratio) ));

    #endif                        
#else
    #if WITH_PRESETHANDLERGUI    
    m_editor.setBounds(0, static_cast<int> (height*g_minPresetHandlerHeight/g_minGuiSize_y + 1), 
                        getWidth(), static_cast<int> (height - (height*g_minPresetHandlerHeight/g_minGuiSize_y + 1) ));
    #else

    m_editor.setBounds(0, 0, getWidth(), height);

    #endif                        
#endif

}
