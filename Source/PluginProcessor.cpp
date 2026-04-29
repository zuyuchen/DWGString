/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin processor.

  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"

//=============================================================================s f d s
DWGAudioProcessor::DWGAudioProcessor()
#ifndef JucePlugin_PreferredChannelConfigurations
     : AudioProcessor (BusesProperties()
                     #if ! JucePlugin_IsMidiEffect
                      #if ! JucePlugin_IsSynth
                       .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                      #endif
                       .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
                     #endif
                        apvts(*this, nullptr, "PARAMS", createParameterLayout()) // initialize APVTS
                       
#endif
{
}

DWGAudioProcessor::~DWGAudioProcessor()
{
}

//==============================================================================
const juce::String DWGAudioProcessor::getName() const
{
    return JucePlugin_Name;
}

bool DWGAudioProcessor::acceptsMidi() const
{
   #if JucePlugin_WantsMidiInput
    return true;
   #else
    return false;
   #endif
}

bool DWGAudioProcessor::producesMidi() const
{
   #if JucePlugin_ProducesMidiOutput
    return true;
   #else
    return false;
   #endif
}

bool DWGAudioProcessor::isMidiEffect() const
{
   #if JucePlugin_IsMidiEffect
    return true;
   #else
    return false;
   #endif
}

double DWGAudioProcessor::getTailLengthSeconds() const
{
    return 0.0;
}

int DWGAudioProcessor::getNumPrograms()
{
    return 1;   // NB: some hosts don't cope very well if you tell them there are 0 programs,
                // so this should be at least 1, even if you're not really implementing programs.
}

int DWGAudioProcessor::getCurrentProgram()
{
    return 0;
}

void DWGAudioProcessor::setCurrentProgram (int index)
{
}

const juce::String DWGAudioProcessor::getProgramName (int index)
{
    return {};
}

void DWGAudioProcessor::changeProgramName (int index, const juce::String& newName)
{
}


void DWGAudioProcessor::releaseResources()
{
    // When playback stops, you can use this as an opportunity to free up any
    // spare memory, etc.
}

#ifndef JucePlugin_PreferredChannelConfigurations
bool DWGAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
  #if JucePlugin_IsMidiEffect
    juce::ignoreUnused (layouts);
    return true;
  #else
    // This is the place where you check if the layout is supported.
    // In this template code we only support mono or stereo.
    // Some plugin hosts, such as certain GarageBand versions, will only
    // load plugins that support stereo bus layouts.
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono()
     && layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

    // This checks if the input layout matches the output layout
   #if ! JucePlugin_IsSynth
    if (layouts.getMainOutputChannelSet() != layouts.getMainInputChannelSet())
        return false;
   #endif

    return true;
  #endif
}
#endif


//==============================================================================
void DWGAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    dwg.prepare(sampleRate);
    
    // Initialize default values
    lastFreq = 440.0f;
    lastT60 = 3.0f;
    lastPluck = false;
    lastR = 0.8;
    lastPickPos = 0.5;
    
    dwg.setFrequency(lastFreq);
    dwg.setDamping(lastT60, lastFreq);

}

void DWGAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{

    buffer.clear();  
    
    // Get parameters
    float freq  = apvts.getRawParameterValue("freq")->load();
    float T60  = apvts.getRawParameterValue("damping")->load();
    bool pluck = apvts.getRawParameterValue("pluck")->load();
    float R = apvts.getRawParameterValue("R")->load();
    float pickPos = apvts.getRawParameterValue("pickPos")->load();
    
    // Only update when parameters changed
    if (freq != lastFreq)
    {
        dwg.setFrequency(freq);
        dwg.setDamping(T60, freq);
        lastFreq = freq;
    }
    
    if (T60 != lastT60)
    {
        dwg.setDamping(T60, freq);
        lastT60 = T60;
    }
    
    // Handle pluck trigger (should be one-shot, not continuous)
    if (pluck && !lastPluck)  // Rising edge detection
    {
        if (R != lastR)
        {
            lastR = R;
        }
        if (pickPos != lastPickPos)
        {
            lastPickPos = pickPos;
        }
        
        dwg.pluck(R);
    }
    lastPluck = pluck;
    
    //
    
    // 先处理所有 MIDI 事件
    for (const auto metadata : midiMessages)
    {
        DBG("MIDI received");
        DBG("num MIDI events = " + juce::String(midiMessages.getNumEvents()));
        auto msg = metadata.getMessage();
        
        if (msg.isNoteOn())
        {
            DBG("num MIDI events = " + juce::String(midiMessages.getNumEvents()));
            DBG("MIDI received");
            
            int midiNote = msg.getNoteNumber();
            float midiFreq = 440.0f * std::pow(2.0f, (midiNote - 69) / 12.0f);
            
            dwg.setFrequency(midiFreq);
            dwg.setDamping(lastT60, midiFreq);
            dwg.pluck(R);
        }
        // Note Off 可以暂时不处理，让弦自然衰减
    }
    
    
    // 然后正常处理audio
    int numSamples = buffer.getNumSamples();
    int numCh = buffer.getNumChannels();
        
    // 逐样本处理
    for (int i = 0; i < numSamples; ++i)
    {
        float out = dwg.process();
        
//        // TEMP DEBUG: force a tone to confirm audio path works
//         out = 0.1f * std::sin(2.0f * M_PI * 440.0f * debugPhase / 44100.0f);
//         debugPhase++;
//        
        for (int ch = 0; ch < numCh; ++ch)
        {
            buffer.setSample(ch, i, out);
        }
       
    }
}

//==============================================================================
bool DWGAudioProcessor::hasEditor() const
{
    return true; // (change this to false if you choose to not supply an editor)
}

juce::AudioProcessorEditor* DWGAudioProcessor::createEditor()
{
    return new DWGAudioProcessorEditor (*this);
}

//==============================================================================
void DWGAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    // You should use this method to store your parameters in the memory block.
    // You could do that either as raw data, or use the XML or ValueTree classes
    // as intermediaries to make it easy to save and load complex data.
}

void DWGAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    // You should use this method to restore your parameters from this memory block,
    // whose contents will have been created by the getStateInformation() call.
}

//============================================================================

juce::AudioProcessorValueTreeState::ParameterLayout DWGAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;
    
    // Frequency
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
       "freq",                      // parameter ID (must NEVER change later)
       "Frequency",                 // UI name
       juce::NormalisableRange<float>(20.0f, 20000.0f, 0.01f, 0.5f),
       440.0f
    ));
    
    // Damping (g)
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "damping",
        "DampingT60",
        juce::NormalisableRange<float>(0.00f, 7.00f, 0.01f),
        3.00f
    ));

    // Pluck triger
    params.push_back(std::make_unique<juce::AudioParameterBool>(
        "pluck",
        "Pluck",
        false
    ));
    
    // Input dynamics filter (soften the attack)
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "R",
        "Soften Attack",
        juce::NormalisableRange<float>(0.00f, 1.00f, 0.01f),
        .80f
    ));
    
    // Pick position 0 ~ 1
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "pickPos",
        "Pick Position",
        juce::NormalisableRange<float>(0.00f, 1.00f, 0.01f),
        .50f
    ));
    
    return { params.begin(), params.end() };
}

//============================================================================
// This creates new instances of the plugin..
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new DWGAudioProcessor();
}
