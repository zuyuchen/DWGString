/*
  ==============================================================================
    DWGString.cpp
  ==============================================================================
*/
#include <JuceHeader.h>
#include "DWGString.h"
#include <cstdlib>
#include <algorithm>

void DWGString::prepare(double sampleRate)
{
    fs = sampleRate;
    updateDelay();
}

void DWGString::setFrequency(float frequency)
{
    freq = frequency;
    updateDelay();
}

void DWGString::setDamping(float T60_, float frequency, float p_)
{
    // g per sample: signal decays to 10^-3 of original after T60 seconds.
    // gLoop = 10^(-3 / (T60 * freq))  — one loop = one period = freq loops/sec
    float gLoop = std::pow(10.0f, -3.0f / (T60_ * frequency));
    g   = std::sqrt(gLoop);   // split equally across both rails
    T60 = T60_;
    p   = p_;
    updateDelay();
}

void DWGString::setInharmonicity(float B_)
{
    B = B_;
    dispersionR.setFromInharmonicity(B, (float)fs, freq);
    dispersionL.setFromInharmonicity(B, (float)fs, freq);
    updateDelay();   // dispersion phase delay affects tuning
}

void DWGString::setPluckStrength(float strength)
{
    pluckStrength = strength;
}

// ============================================================================
//  updateDelay — the only place that touches intDelay / fractionalDelay.
//
//  Topology: two rails (R and L), each carrying half the round-trip delay.
//  Filters appear once per rail, so their delay is counted once per rail too.
//
//  Round trip = 2 × railDelay + 2 × filterDelayPerRail = fs / freq
//  ⟹  railDelay = fs / (2 × freq) − filterDelayPerRail
// ============================================================================
void DWGString::updateDelay()
{
    const float totalDelay = (float)(fs / freq);    // full round-trip samples

    // Phase delay of the one-pole LP at the fundamental (DC approx is fine for guitar)
    const float lpDelay = p / (1.0f - p);

    // Phase delay of the dispersion filter at the fundamental frequency
    const float omega0   = juce::MathConstants<float>::twoPi * freq / (float)fs;
    const float dispDelay = dispersionR.phaseDelayAt(omega0);   // per rail

    const float filterDelayPerRail = lpDelay + dispDelay;

    // Each rail carries half the round-trip, minus its own filter contribution
    float railDelay = totalDelay / 2.0f - filterDelayPerRail;
    railDelay = std::max(railDelay, 1.05f);    // hard floor: at least 1 int + small frac

    intDelay        = (int)railDelay;
    fractionalDelay = std::clamp(railDelay - (float)intDelay, 0.05f, 0.95f);

    fracDelayR.setDelay(fractionalDelay);
    fracDelayL.setDelay(fractionalDelay);
    delayR.prepare(intDelay);
    delayL.prepare(intDelay);
}

// ============================================================================
//  pluck — fill both delay lines with a shaped noise burst.
//
//  Signal chain:
//    white noise  →  feedforward comb (pluck position notches)
//                 →  one-pole LP (soften attack)
//                 →  both delay lines
// ============================================================================
void DWGString::pluck(float R, float pluckPos)
{
    delayR.clear();
    delayL.clear();
    z1R    = 0.0f;
    z1L    = 0.0f;
    zInput = 0.0f;
    dispersionR.reset();
    dispersionL.reset();

    int peakIdx = std::max(1, (int)std::round(pluckPos * intDelay));

    std::vector<float> combBuffer(peakIdx, 0.0f);
    int combPtr = 0;

    for (int i = 0; i < intDelay; ++i)
    {
        float displacement = noiseInput(pluckStrength);

        // Feedforward comb — creates spectral notches at multiples of 1/pluckPos
        float delayed = combBuffer[combPtr];
        float combed  = displacement - delayed;
        combBuffer[combPtr] = displacement;
        combPtr = (combPtr + 1) % peakIdx;

        // Soften attack
        float softened = onePoleLP(combed, zInput, R);

        delayR.write(softened);
        delayL.write(softened);
        delayR.advance();
        delayL.advance();
    }
}

// ============================================================================
//  process — one sample of the DWG loop.
//
//  Per rail:
//    delayLine.read()  →  fracDelay  →  dispersion  →  onePoleLP  →  reflect (-g)  →  delayLine.write()
//
//  Output = sum of both traveling waves at the bridge (read point).
// ============================================================================
float DWGString::process()
{
    float yPlus  = delayR.read();
    float yMinus = delayL.read();

    yPlus  = fracDelayR.process(yPlus);
    yMinus = fracDelayL.process(yMinus);

    yPlus  = dispersionR.process(yPlus);
    yMinus = dispersionL.process(yMinus);

    float yPlusLP  = onePoleLP(yPlus,  z1R, p);
    float yMinusLP = onePoleLP(yMinus, z1L, p);

    // Rigid boundary reflection: negate and scale by loss coefficient
    delayL.write(-g * yPlusLP);
    delayR.write(-g * yMinusLP);

    delayR.advance();
    delayL.advance();

    return yPlus + yMinus;
}

// ============================================================================
//  Utilities
// ============================================================================
float DWGString::noiseInput(float amplitude)
{
    return amplitude * (2.0f * rand() / (float)RAND_MAX - 1.0f);
}

float DWGString::twoPtAvgDecay(float x, float& z1)
{
    float y = 0.5f * (x + z1);
    z1 = x;
    return y;
}

float DWGString::onePoleLP(float x, float& z1, float pole)
{
    float y = (1.0f - pole) * x + pole * z1;
    z1 = y;
    return y;
}
