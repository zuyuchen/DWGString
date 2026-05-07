/*
  ==============================================================================

    DWGString.cpp
    Created: 23 Apr 2026 5:15:30pm
    Author:  chenzuyu

  ==============================================================================
*/
#include <JuceHeader.h>  // ← add this
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

void DWGString::setDamping(float T60, float frequency, float mu_, float K_)
{

    float gLoop = pow(10.0f, -3.0f / (T60 * frequency)); // g now is proportional to 10^(-1/f), lower in pitch(longer loops) decay faster per loop but overall it decays 10^-3 of orgional after T60s for all frequencies.
    g = sqrt(gLoop);
    
    // set mu and K
    mu = mu_;
    K = K_;
    // Frequency-dependent loss pole: p = exp(-2π * mu / (K * frequency))
    // This gives realistic frequency-dependent damping, ensuring p stays between 0 and 1 and gives natural high-frequency rolloff!
    float exponent = -2.0f * juce::MathConstants<float>::pi * mu / (K * frequency + 0.00001f);
    p = std::clamp(exp(exponent), 0.01f, 0.99f);
        
    // Update fractional delay
    updateDelay(); // pole changes lead to change of phase delay
}

void DWGString::setInharmonicity(float B_)
{
    B = B_;
    dispersionR.setFromInharmonicity(B, fs, freq);
    dispersionL.setFromInharmonicity(B, fs, freq);
    
    // NOTE: updateDelay() should also account for dispersion phase delay
    // for now this is approximate — tuning compensation comes next
    updateDelay();
}

void DWGString::setPluckStrength(float strength)
{
    pluckStrength = strength;
}

// ===========================
// Private member Function
// updates the delay size when sample rate or frequency changes
// ===========================
void DWGString::updateDelay()
{
    float totalDelay = (float)(fs / freq);
        
    // Phase delay of one-pole LP at DC = p / (1 - p)
    float lpDelay = p / (1.0f - p);
    
    // Phase delay of 4-stage dispersion APF at DC (once per loop)
    // For H(z) = (a2 + a1*z^-1 + z^-2)/(1 + a1*z^-1 + a2*z^-2)
    // Phase delay at DC = -(a1 + 2*a2) / (1 + a1 + a2)  per stage
    float dispDelay = dispersionR.phaseDelayAtDC();  // total for all stages
    
    // Total non-delay-line phase in the loop
    float filterDelay = lpDelay + dispDelay;
    
    // Each delay line phase
    float halfDelay = 0.5f * (totalDelay - filterDelay);
    intDelay = std::max(1, (int)halfDelay);
    fractionalDelay = std::clamp(halfDelay - intDelay, 0.0f, 0.999f);
    
    fracDelayR.setDelay(fractionalDelay);
    fracDelayL.setDelay(fractionalDelay);
    
    delayR.prepare(intDelay);
    delayL.prepare(intDelay);

    
}

// initialize the delay line with white noise
void DWGString::pluck(float R, float pluckPos)
{
  
    // Clear the delay lines and reset the states of the filter
    delayR.clear();
    delayL.clear();
    z1R = 0.0f;
    z1L = 0.0f;
    zInput = 0.0f;
    dispersionR.reset();
    dispersionL.reset();
    
    int peakIdx = std::max(1, (int)std::round(pluckPos * intDelay)); // pick position index as the peak point of the triangle plucking wave
    
    // M = pluck position as fraciton of total delay size
    int M = peakIdx;
    std::vector<float> combBuffer(M, 0.0f); // input buffer (feedfoward comb filter buffer)
    int combPtr = 0;
    
    // Write the input signal in
    for (int i = 0; i < intDelay; ++i)
    {
        // White Noise Input
        float displacement = noiseInput(pluckStrength);
        
//        Triangle Displacement Input
//       float displacement;
//       if (i <= peakIdx)
//           displacement = pluckStrength * (float)i / (float)peakIdx; // rising slope
//       else 
//           displacement = pluckStrength * (float)(intDelay - i) / (float)(intDelay - peakIdx); // falling slope
        // Add small noise to the pluck for realism
        float noise = noiseInput(0.1);
        displacement += noise;
        
        // First apply comb (pluck position notches)
        float delayed = combBuffer[combPtr];
        float combed = displacement - delayed;
        combBuffer[combPtr] = displacement;
        combPtr = (combPtr + 1) % peakIdx;

        // Then soften with LPF
        float softened = onePoleLP(combed, this->zInput, R);
        
        // Write same filtered noise to both delay lines (mono excitation)
        delayR.write(softened);
        delayL.write(softened);
        
        delayR.advance();
        delayL.advance();
        
    }

}

float DWGString::noiseInput(float amplitude)
{
    // Generate white noise
    float noise = amplitude * (2.0f * rand() / RAND_MAX - 1.0f);
    return noise;
}


float DWGString::twoPtAvgDecay(float x, float& z1)
{
    float output = 0.5 * (x + z1);
    
    // update the states;
    z1 = x;
    
    return output;
}

float DWGString::onePoleLP(float x, float& z1, float p)
{
    float b0 = 1.0f - p;
    float y = b0 * x + p * z1;  // y[n] = (1-p)*x[n] + p*y[n-1]
    z1 = y;
    return y;
}

//delayLine.read()
//  → fracDelay      (fractional pitch tuning)
//  → dispersionAPF  (inharmonicity)
//  → onePoleLP      (frequency-dependent loss)
//  → reflect (-g)
//  → delayLine.write()

float DWGString::process()
{
    // read the integer-delay line values at the boundary
    float yPlus = delayR.read();
    float yMinus = delayL.read();
    
    // apply fractional delay ONCE in loop
    yPlus = fracDelayR.process(yPlus);
    yMinus = fracDelayL.process(yMinus);
    
//    // Dispersion - goes after fractional delay, before loss
    yPlus = dispersionR.process(yPlus);
    yMinus = dispersionL.process(yMinus);
    
    // apply two point average (one zero low pass) filter as the frequency-dependent loss
//    float yPlusLP = twoPtAvgDecay(yPlus, this->z1R);
//    float yMinusLP = twoPtAvgDecay(yMinus, this->z1L);
    
    // apply one-one pole loss pass filter as the frequency-dependent loss
    float yPlusLP = onePoleLP(yPlus, this->z1R, p);
    float yMinusLP = onePoleLP(yMinus, this->z1L, p);
    
    // calculate the reflected value
    float reflectedL = -g * yPlusLP;
    float reflectedR = -g * yMinusLP;
    
    // write back to the opposite delay line
    delayL.write(reflectedL);
    delayR.write(reflectedR);
    
    // step forward
    delayR.advance();
    delayL.advance();
    
    // combine the traveling waves for the output
    float output = yPlus + yMinus;
    
    // Debug: Check if output is silent
    static int debugCounter = 0;
    if (debugCounter++ % 44100 == 0) // Once per second at 44.1kHz
    {
        DBG("String output level: " + juce::String(output));
    }
    
    return output;
   
}
