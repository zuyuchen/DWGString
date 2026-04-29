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

class DWGString
{
public:
    void prepare(double sampleRate);
    void setFrequency(float frequency);
    void setDamping(float T60, float frequency); // T60 controlled global damping coefficient

    void pluck(float R, float pluckPos);    // pluck initialization
    float noiseInput(float amplitude); // emulate a random displacement profile
    float triangleInput(float amplitude);  // emulate the displacement shape of pluck
    void squareStrike(float velocity); // emulate the velocity wave of strike
    
    float twoPtAvgDecay(float sample, float& z1);   // implement a two-point average loww pass filtering as the frequency dependent decay
    float process();
    
    
    
private:
    void updateDelay();
    
    DelayLine delayR, delayL;  // two delay lines (left and right)
    int intDelay = 1; // integer delay
    
    FracDelay fracDelayR, fracDelayL;  // two fractional delay filters (apf)
    float fractionalDelay = 0.1;  // fractional delay values ( 0 ~ 1 samples)
   
    double fs = 44100.0;  // sample rate
    float freq = 440.0f; // fuundamental frequency (pitch)
    float amplitude = 1; // pluck amplitude
    float g = 0.999f; // damping coefficient
    float T60 = 2.0; // T60 decay time in sec
    
    float zInput = 0.0f; // Input dynamics filter state
    
    float z1R = 0.0f;
    float z1L = 0.0f; // inital state for the two point average filter
};
