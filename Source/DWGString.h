/*
  ==============================================================================
    DWGString.h
    Digital Waveguide string model using custom DSP components.
  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>   // for juce::MathConstants, DBG, etc.
#include "DelayLine.cpp"
#include "FracDelay.cpp"
#include "DispersionFilter.h"

// ============================================================================
//  DWGString — bidirectional digital waveguide string model.
//
//  Two-rail topology:
//    delayR/L : integer sample delay (custom circular buffer)
//    fracDelayR/L : fractional tuning via 1st-order all-pass
//    dispersionR/L : inharmonicity via 2-stage all-pass cascade
//    onePoleLP : frequency-dependent loss at each reflection
//
//  Round-trip delay = 2 × (intDelay + fracDelay + dispDelay + lpDelay) = fs/freq
// ============================================================================
class DWGString
{
public:
    // Prepare with sample rate — must be called before anything else
    void prepare(double sampleRate);

    // Set fundamental frequency — triggers updateDelay()
    void setFrequency(float frequency);

    // Set damping: T60 controls overall decay, p is the one-pole LP coefficient
    void setDamping(float T60_, float frequency, float p_);

    // Set inharmonicity coefficient B (guitar range: ~1e-5 to 1e-3)
    void setInharmonicity(float B_);

    // Set excitation amplitude
    void setPluckStrength(float strength);

    // Initialise the delay lines with a shaped noise burst at pluckPos ∈ [0,1]
    // R = input LP pole (0 = hard attack, ~0.95 = soft/muted)
    void pluck(float R, float pluckPos);

    // Process one output sample
    float process();

private:
    void  updateDelay();
    float noiseInput(float amplitude);
    float twoPtAvgDecay(float x, float& z1);
    float onePoleLP(float x, float& z1, float pole);

    // -------------------------------------------------------------------------
    // State & parameters
    // -------------------------------------------------------------------------
    double fs    = 44100.0;
    float  freq  = 440.0f;
    float  g     = 0.995f;   // per-sample loss coefficient
    float  T60   = 2.0f;
    float  p     = 0.40f;    // one-pole LP pole
    float  B     = 0.0f;     // inharmonicity coefficient
    float  pluckStrength = 0.5f;

    // -------------------------------------------------------------------------
    // Delay line sizing — computed by updateDelay()
    // -------------------------------------------------------------------------
    int   intDelay        = 16;
    float fractionalDelay = 0.5f;

    // -------------------------------------------------------------------------
    // DSP components — custom classes, no JUCE DSP module required
    // -------------------------------------------------------------------------
    DelayLine       delayR,       delayL;       // integer delay rails
    FracDelay       fracDelayR,   fracDelayL;   // fractional pitch tuning
    DispersionFilter dispersionR, dispersionL;  // inharmonicity

    // -------------------------------------------------------------------------
    // Filter states
    // -------------------------------------------------------------------------
    float z1R    = 0.0f;   // right-rail LP state
    float z1L    = 0.0f;   // left-rail LP state
    float zInput = 0.0f;   // pluck-input LP state
};
