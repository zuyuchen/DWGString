/*
  ==============================================================================

    DispersionFilter.h
    Created: 6 May 2026 5:20:50pm
    Author:  chenzuyu

  ==============================================================================
*/

// DispersionFilter.h  (4-stage cascade)
#pragma once
#include "DispersionAPF.h"
#include <array>

class DispersionFilter
{
public:
    static constexpr int NUM_STAGES = 2;

    void setFromInharmonicity(float B, float sampleRate, float freq)
    {
        for (auto& stage : stages)
            stage.setFromInharmonicity(B, sampleRate, freq);
    }

    void setCoefficients(float a1, float a2)
    {
        for (auto& stage : stages)
            stage.setCoefficients(a1, a2);
    }

    float process(float x)
    {
        float y = x;
        for (auto& stage : stages)
            y = stage.process(y);
        return y;
    }

    void reset()
    {
        for (auto& stage : stages)
            stage.reset();
    }

    // Returns total phase delay at a given frequency (radians), for tuning compensation
    // Approximation: each 2nd-order APF contributes ~2*a2/(1+a2) samples at low freq
    float phaseDelayAtDC() const
    {
        // For H(z) = (a2 + a1*z^-1 + z^-2)/(1 + a1*z^-1 + a2*z^-2)
        // Phase delay at DC = (a1 + 2*a2) / (1 + a1 + a2)  [in samples, per stage...
        // actually this needs the coefficients stored - simplified below]
        // This is a placeholder; implement properly once coefficients are stable
        
        float total = 0.0f;
        for (const auto& stage : stages)
            total += stage.phaseDelayAtDC();
        return total;
        
    }

private:
    std::array<DispersionAPF, NUM_STAGES> stages;
};
