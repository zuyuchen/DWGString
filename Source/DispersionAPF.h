/*
  ==============================================================================
    DispersionAPF.h
    Single stage of a 1st-order all-pass filter for dispersion simulation.
  ==============================================================================
*/

#pragma once
#include <cmath>
#include <algorithm>

//  One stage of a 1st-order all-pass:
//
//      H(z) = (c + z⁻¹) / (1 + c·z⁻¹)
//
//  With c < 0:  pole at z = −c ∈ (0,1)  (positive real axis)
//               phase delay decreases monotonically from DC → Nyquist
//               → upper harmonics complete their loop sooner
//               → they resonate sharp → positive inharmonicity (B > 0 behaviour)
//
//  Phase delay:
//      τ_p(0)  = (1−c)/(1+c)   samples   ← maximum, at DC
//      τ_p(π)  = 1              sample    ← minimum, at Nyquist (always 1, independent of c)
//
//  This class is a pure DSP primitive. It holds no knowledge of B or sample rate;
//  the coefficient c must be set by DispersionFilter after staging math is done.

class DispersionAPF
{
public:
    //  Set the all-pass coefficient directly.
    //  c must be in (−1, 0] for stability and correct dispersion direction.
    void setCoefficient(float c_)
    {
        c = std::clamp(c_, -0.99f, 0.0f);
    }

    float getCoefficient() const { return c; }

    //  Transposed Direct Form II — one multiply-add in the forward path,
    //  no feedback loop in the state update → cleaner numerical behaviour.
    //
    //  y     = c·x + state
    //  state = x − c·y          (equivalent to storing x − c·(c·x + state_prev))
    float process(float x)
    {
        float y = c * x + state;
        state   = x - c * y;
        return y;
    }

    void reset() { state = 0.0f; }

    //  Phase delay at DC [samples].
    //  Because φ(0) = 0 for all-pass, the DC phase delay equals the DC group delay:
    //      τ_p(0) = lim_{ω→0} −φ(ω)/ω  =  τ_g(0)  =  (1−c)/(1+c)
    float phaseDelayAtDC() const
    {
        float denom = 1.0f + c;
        if (std::abs(denom) < 1e-6f) return 0.0f;
        return (1.0f - c) / denom;
    }

    //  Phase delay at an arbitrary frequency ω ∈ [0, π] [samples].
    //  Useful for tuning-compensation at the fundamental frequency.
    //
    //  H(e^{jω}) = (c + e^{−jω}) / (1 + c·e^{−jω})
    //  φ(ω)      = atan2(−sin ω, c + cos ω) − atan2(−c·sin ω, 1 + c·cos ω)
    //  τ_p(ω)    = −φ(ω) / ω
    float phaseDelayAt(float omega) const
    {
        if (omega < 1e-6f) return phaseDelayAtDC();
        float phi = std::atan2(-std::sin(omega),  c + std::cos(omega))
                  - std::atan2(-c * std::sin(omega), 1.0f + c * std::cos(omega));
        return -phi / omega;
    }

private:
    float c     = 0.0f;
    float state = 0.0f;
};
