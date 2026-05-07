/*
  ==============================================================================

    DWGString.h
    Created: 23 Apr 2026 5:15:30pm
    Author:  chenzuyu

  ==============================================================================
*/

#pragma once
#include "DelayLine.cpp"
#include "FracDelay.cpp"
#include "DispersionFilter.h"

class DWGString
{
public:
    void prepare(double sampleRate);
    void setFrequency(float frequency);
    void setDamping(float T60, float frequency, float mu_, float K_); // T60 controlled global damping coefficient
    void setInharmonicity(float B_);
    void setPluckStrength(float strength);
    void pluck(float R, float pluckPos);    // pluck initialization
    
    float noiseInput(float amplitude); // emulate a random displacement profile
    float triangleInput(float amplitude);  // emulate the displacement shape of pluck
    void squareStrike(float velocity); // emulate the velocity wave of strike
    
    float twoPtAvgDecay(float sample, float& z1);   // implement a two-point average loww pass filtering as the frequency dependent decay
    float onePoleLP(float x, float& z1, float p);
 
    float process();
    
    
    
private:
    void updateDelay();
    
    DelayLine delayR, delayL;  // two delay lines (left and right)
    int intDelay = 1; // integer delay
    
    FracDelay fracDelayR, fracDelayL;  // two fractional delay filters (apf)
    float fractionalDelay = 0.1;  // fractional delay values ( 0 ~ 1 samples)
   
    double fs = 44100.0;  // sample rate
    float freq = 440.0f; // fuundamental frequency (pitch)
    float pluckStrength = 1; // pluck amplitude
    float g = 0.999f; // damping coefficient
    float T60 = 2.0; // T60 decay time in sec
    
    float zInput = 0.0f; // Input dynamics filter state
    
    // Frequency-dependent loss filter prams
    float z1R = 0.0f;
    float z1L = 0.0f; // inital state
    
    float p = 0.9; // pole = 1 - 2pi*mu / K
    float mu = 0.001; // Internal friction (mu) - controls high frequency decay rate
    float K = 0.0001; // Stiffness (K) - controls inharmonicity
    
    // Dispersion Filter
    DispersionFilter dispersionR, dispersionL;
    float B = 0.0001f;  // inharmonicity coefficient, exposed as parameter
     
};
