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
    lastT60 = 7.5f;
    lastPluck = false;
    lastR = 0.48;
    lastPluckPos = 0.2;
    lastMu = 0.0002;
    lastK = 0.00001;
    lastP = 0.30;
    lastB = 0.0001;
    lastVelocity = 0.8f;
    dwg.setFrequency(lastFreq);
    dwg.setDamping(lastT60, lastFreq, lastMu, lastK, lastP);
    dwg.setInharmonicity(lastB);
    

}

void DWGAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{

    buffer.clear();  
    
    // Get parameters
    float freq  = apvts.getRawParameterValue("freq")->load();
    float T60  = apvts.getRawParameterValue("damping")->load();
    bool pluck = apvts.getRawParameterValue("pluck")->load();
    float R = apvts.getRawParameterValue("R")->load();
    float pluckPos = apvts.getRawParameterValue("pluckPos")->load();
    float mu = lastMu;
    float K = lastK;
    float p = apvts.getRawParameterValue("p")->load();
    float B = apvts.getRawParameterValue("B")->load();
    
    
    // Only update when parameters changed
    if (freq != lastFreq)
    {
        dwg.setFrequency(freq);
        dwg.setDamping(T60, freq, mu, K, p);
        dwg.setInharmonicity(B);
        lastFreq = freq;
    }
    
    if (T60 != lastT60 || mu != lastMu || K != lastK || p != lastP)
    {
        dwg.setDamping(T60, freq, mu, K, p);
        lastT60 = T60;
        lastMu = mu;
        lastK = K;
        lastP = p;
    }
    
    if (B != lastB)
    {
        dwg.setInharmonicity(B);
        lastB = B;
    }
    
    // Handle pluck trigger (should be one-shot, not continuous)
    if (pluck && !lastPluck)  // Rising edge detection
    {
        if (R != lastR)
        {
            lastR = R;
        }
        if (pluckPos != lastPluckPos)
        {
            lastPluckPos = pluckPos;
        }
        dwg.pluck(R, pluckPos);
    }
    lastPluck = pluck;
    
    
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
            
            float velocity = msg.getVelocity() / 127.0f;
            lastVelocity = velocity;
            // Scale pluck force by velocity
            float pluckStrength = 0.5f + 0.5f * velocity;
            dwg.setPluckStrength(pluckStrength);
            dwg.setFrequency(midiFreq);
            dwg.setDamping(lastT60, midiFreq, mu, K, p);
            dwg.setInharmonicity(lastB);
            dwg.pluck(R, pluckPos);
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
        juce::NormalisableRange<float>(41.2f, 329.6f, 0.01f, 0.5f),
        82.4f  // Default to low E2
    ));
    
    // Damping T60
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "damping",
        "DampingT60",
        juce::NormalisableRange<float>(1.0f, 10.0f, 0.1f),
        7.8f
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
        .91f
    ));
    
    // Pick position 0 ~ 1
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "pluckPos",
        "Pluck Position",
        juce::NormalisableRange<float>(0.00f, 1.00f, 0.01f),
        .20f
    ));
    
//    // Internal friction (mu) - controls high frequency decay rate
//    params.push_back(std::make_unique<juce::AudioParameterFloat>(
//        "mu",
//        "Internal Friction",
//        juce::NormalisableRange<float>(0.0001f, 0.001f, 0.0001f, 0.3f),  // 0.3 skew toward small values
//        0.0002f
//    ));
//
//    // Stiffness (K) - controls inharmonicity
//    params.push_back(std::make_unique<juce::AudioParameterFloat>(
//        "K",
//        "Stiffness",
//        juce::NormalisableRange<float>(0.000001f, 0.0001f, 0.00001f, 0.3f),  // 0.3 skew toward small values
//        0.00001f
//    ));
    
    // Global Low Pass Filtering (p) - controls the pole of the one-pole LPF
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            "p",
            "pole",
            juce::NormalisableRange<float>(0.01f, 0.99f, 0.01f),  //
            0.65f
        ));
    
    // Inharmonicity B
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "B",
        "Inharmonicity",
        juce::NormalisableRange<float>(0.0000f, 0.001f, 0.00001f, 0.3f),
        0.0000f
    ));
    return { params.begin(), params.end() };
}

//============================================================================
// This creates new instances of the plugin..
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new DWGAudioProcessor();
}
