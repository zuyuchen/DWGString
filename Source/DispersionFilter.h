/*
  ==============================================================================
    DispersionFilter.h
    Cascade of NUM_STAGES first-order all-pass filters for string dispersion.
  ==============================================================================
*/

#pragma once
#include "DispersionAPF.h"
#include <array>
#include <cmath>

//  Cascade of NUM_STAGES identical 1st-order all-pass stages.
//
//  Why cascade instead of one stage with a bigger coefficient?
//  A single stage has phase delay that drops from (1−c)/(1+c) at DC to 1 at Nyquist.
//  This is a gentle, sub-linear curve. Real string inharmonicity follows B·n² (quadratic).
//  Cascading M stages with a proportionally smaller c per stage approximates the quadratic curve more closely across more harmonics, at the cost of M multiply-adds per sample.
//
//  Staging math:
//      Total phase delay spread ≈ M · [−2c/(1+c)]   (exact for small |c|)
//  To keep the same total spread as M increases, divide c_single by M:
//      c_per_stage = c_single / M
//
//  NUM_STAGES = 2 is the recommended default for guitar.
//
//  Tuning compensation:
//      Adding this filter to the DWG loop adds phaseDelayAtDC() extra samples of delay
//      at the fundamental. The delay line length must be reduced by this amount to keep
//      the string in tune. Call phaseDelayAt(omega_fundamental) for exact compensation.

class DispersionFilter
{
public:
    static constexpr int NUM_STAGES = 2;

    //  Main entry point. Call whenever B, sampleRate, or freq changes.
    //  B    = inharmonicity coefficient (guitar: ~1e-5 to 1e-3)
    //  freq = fundamental frequency of this string [Hz]
    void setFromInharmonicity(float B, float sampleRate, float freq)
    {
        float c = computeCoefficientPerStage(B, sampleRate, freq);
        for (auto& stage : stages)
            stage.setCoefficient(c);
    }

    //  Manual override — set the per-stage coefficient directly.
    //  Useful for A/B testing or when B is not available.
    void setCoefficientPerStage(float c)
    {
        for (auto& stage : stages)
            stage.setCoefficient(c);
    }

    float process(float x)
    {
        for (auto& stage : stages)
            x = stage.process(x);
        return x;
    }

    void reset()
    {
        for (auto& stage : stages)
            stage.reset();
    }

    //  Total phase delay at DC [samples] — use this to shorten the DWG delay line.
    float phaseDelayAtDC() const
    {
        float total = 0.0f;
        for (const auto& stage : stages)
            total += stage.phaseDelayAtDC();
        return total;
    }

    //  Total phase delay at a given radian frequency ω ∈ [0, π].
    //  More accurate than phaseDelayAtDC() for tuning at the fundamental.
    //  Pass  ω = 2π·f0/sampleRate  for the string fundamental.
    float phaseDelayAt(float omega) const
    {
        float total = 0.0f;
        for (const auto& stage : stages)
            total += stage.phaseDelayAt(omega);
        return total;
    }

private:
    std::array<DispersionAPF, NUM_STAGES> stages;

    //  B → per-stage coefficient.
    //
    //  c_single:  target coefficient if only one stage were used.
    //             Linear in B_norm gives uniform perceptual sensitivity across [0, B_max].
    //             At B = 0.001 → c_single = −0.5, total spread ≈ 2 samples (2-stage cascade).
    //
    //  c_per_stage = c_single / NUM_STAGES
    //             Keeps total phase delay spread constant regardless of stage count.
    //             Exact for |c| ≪ 1; slight underestimate for larger |c| (acceptable for guitar).
    static float computeCoefficientPerStage(float B, float /*sampleRate*/, float /*freq*/)
    {
        if (B < 1e-6f) return 0.0f;

        constexpr float B_max    = 0.001f;   // reference: stiff steel string
        constexpr float c_max    = -0.5f;    // single-stage target at B_max
        // audible range starts around |c_single| > 0.02, so B ~ 4e-5

        float B_norm     = std::clamp(B / B_max, 0.0f, 1.0f);
        float c_single   = c_max * B_norm;                           // linear, not sqrt
        float c_per_stage = c_single / static_cast<float>(NUM_STAGES);

        return std::clamp(c_per_stage, -0.99f, 0.0f);
    }
};
