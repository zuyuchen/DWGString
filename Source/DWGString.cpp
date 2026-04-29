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

void DWGString::setDamping(float T60, float frequency)
{

    float gLoop = pow(10.0f, -3.0f / (T60 * frequency)); // g now is proportional to 10^(-1/f), lower in pitch(longer loops) decay faster per loop but overall it decays 10^-3 of orgional after T60s for all frequencies.
    g = sqrt(gLoop);
//    g = 0.9999f;
}

// ===========================
// Private member Function
// updates the delay size when sample rate or frequency changes
// ===========================
void DWGString::updateDelay()
{
    float totalDelay = (float)(fs / freq);
    
    // Reserve 0.5 samples for the two-point average, then split remaining equally
    float halfDelay = (totalDelay - 0.5f) / 2.0f;
    intDelay = std::max(1, (int)halfDelay);
    fractionalDelay = std::clamp(halfDelay - intDelay, 0.0f, 0.999f);
    
    // set the filter coefficients for the FracDelay filter (allpass filter)
    fracDelayR.setDelay(fractionalDelay);
    fracDelayL.setDelay(fractionalDelay);
    
    // set the integer delay size for the delay lines
    delayR.prepare(intDelay);
    delayL.prepare(intDelay);
    
}

// initialize the delay line with white noise
void DWGString::pluck(float R)
{
  
    // Clear the delay lines and reset the states of the filter
    delayR.clear();
    delayL.clear();
    z1R = 0.0f;
    z1L = 0.0f;
    zInput = 0.0f;
    
    // M = pluck position as fraciton of total delay size
    float totalDelay = (float)(fs / freq);
    int M = std::max(1, (int)std::round(pluckPos * totalDelay));
    std::vector<float> combBuffer(M, 0.0f); // input buffer (feedfoward comb filter buffer)
    int combPtr = 0;
    
    // Write the input signal in
    for (int i = 0; i < intDelay; ++i)
    {
        // Generate white noise
        float noise = amplitude * (2.0f * rand() / RAND_MAX - 1.0f); // [-1, 1] randome numbers scaled by amplitude
        
        // Apply input dynamic filter (one-pole LPF to soften high frequencies)
        noise = (1 - R) * noise + R * zInput;
        zInput = noise;
        
        // Apply pick position filter (feedforward comb, "notch", filter)
        float delayed = combBuffer[combPtr];
        float combed = noise + delayed; // y[n] = x[n] + x[n - M]
        combBuffer[combPtr] = noise;
        combPtr = (combPtr + 1) % M;
        
        // Write same filtered noise to both delay lines (mono excitation)
        delayR.write(combed);
        delayL.write(combed);
        
        delayR.advance();
        delayL.advance();
        
        
    }

}

float DWGString::twoPtAvgDecay(float x, float& z1)
{
    float output = 0.5 * (x + z1);
    
    // update the states;
    z1 = x;
    
    return output;
}

float DWGString::process()
{
    // read the integer-delay line values at the boundary
    float yPlus = delayR.read();
    float yMinus = delayL.read();
    
    // apply fractional delay ONCE in loop
    yPlus = fracDelayR.process(yPlus);
    yMinus = fracDelayL.process(yMinus);
    
    // apply two point average (one zero low pass) filter as the frequency-dependent loss
    float yPlusLP = twoPtAvgDecay(yPlus, this->z1R);
    float yMinusLP = twoPtAvgDecay(yMinus, this->z1L);
    
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
    return yPlus + yMinus;
   
}
