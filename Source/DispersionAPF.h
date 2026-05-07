/*
  ==============================================================================

    DispersionAPF.h
    Created: 6 May 2026 5:17:10pm
    Author:  chenzuyu

  ==============================================================================
*/

#pragma once
#include <JuceHeader.h>  // ← add this
#include <cmath>
#include <array>
#include <algorithm>  // for std::clamp

//  One stage of a 2nd-order all-pass filter for dispersion simulation.
//
//  Transfer function: H(z) = (a2 + a1*z^-1 + z^-2) / (1 + a1*z^-1 + a2*z^-2)
//  Magnitude is exactly 1 at all frequencies (pure phase filter).
//  Phase delay increases with frequency, simulating string stiffness.

class DispersionAPF
{
public:
    // Set coefficients directly
    void setCoefficients(float a1_, float a2_)
    {
        a1 = a1_;
        a2 = a2_;
    }

    // Set coefficients from inharmonicity coefficient B and a tuning frequency ratio
    // B = inharmonicity coefficient (typically 1e-5 to 1e-3 for guitar)
    // Call this whenever B or sample rate changes
    void setFromInharmonicity(float B, float sampleRate, float freq)
    {
        // Clamp B to reasonable range
       float B_clamped = std::max(0.000001f, std::min(0.01f, B));
        
        // a2 controls how much phase is accumulated at high frequencies
        // Larger |a2| = more dispersion. Keep negative to push transition above fundamental.
        a2 = -std::exp(-2.0f * juce::MathConstants<float>::pi * std::sqrt(B_clamped + 1e-10f));
        a2 = std::clamp(a2, -0.99f, -0.01f);  // Keep away from -1
        
        // a1 tunes the centre of the phase transition
        // Setting cos term to freq-dependent value concentrates dispersion above fundamental
        // Set transition centre well above fundamental — target ~1/4 Nyquist
        // This leaves the fundamental largely unaffected and bends upper partials
        float wc = juce::MathConstants<float>::pi * 0.5f;  // ← fixed, not freq-dependent
        
        float denom = 1.0f + a2;
        a1 = (std::abs(denom) < 1e-6f) ? 0.0f
                                            : std::clamp(-2.0f * a2 * std::cos(wc) / denom, -1.9f, 1.9f);
    }

    float process(float x)
    {
        // Direct Form II Transposed
        float y = a2 * x + w1;
        w1 = a1 * x - a1 * y + w2;
        w2 = x - a2 * y;
        return y;
    }

    void reset()
    {
        w1 = 0.0f;
        w2 = 0.0f;
    }

    // The formula for one 2nd-order APF stage at DC (ω=0) is:
    // τ(0) = -(a1 + 2·a2) / (1 + a1 + a2)   [in samples]
    float phaseDelayAtDC() const
    {
        float denom = 1.0f + a1 + a2;
        if (std::abs(denom) < 1e-6f) return 0.0f;
        return -(a1 + 2.0f * a2) / denom;
    }
    
private:
    float a1 = 0.0f;
    float a2 = 0.0f;
    float w1 = 0.0f;  // state 1
    float w2 = 0.0f;  // state 2
};


