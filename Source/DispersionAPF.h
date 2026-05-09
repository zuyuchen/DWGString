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
        // Hard bypass when B is negligible
        if (B < 1e-6f) { a1 = 0.0f; a2 = 0.0f; return; }

        // --- a2: monotonically maps B → dispersion amount ---
        // For guitar B ∈ [0, 0.001], target a2 ∈ [0, -0.5]
        // sqrt scaling gives a reasonable perceptual curve
        float B_norm = std::sqrt(B / 0.001f);              // 0..1 for B in [0, 0.001]
        a2 = std::clamp(-B_norm * 0.5f, -0.85f, 0.0f);    // more B → more negative a2

        // --- a1: set transition centre above the fundamental ---
        // Target: 4× the fundamental, clamped to 10%–40% of Nyquist
        float wc_hz = std::clamp(freq * 4.0f,
                                 sampleRate * 0.05f,
                                 sampleRate * 0.40f);
        float wc = 2.0f * juce::MathConstants<float>::pi * wc_hz / sampleRate;

        float denom = 1.0f + a2;
        float a1_raw = (std::abs(denom) < 1e-6f) ? 0.0f
                         : -2.0f * a2 * std::cos(wc) / denom;

        // ← clamp to actual stability triangle, not an arbitrary constant
        float max_a1 = 0.95f * (1.0f + a2);   // 5% margin inside the boundary
        a1 = std::clamp(a1_raw, -max_a1, max_a1);
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


