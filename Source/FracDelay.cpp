/*
  ==============================================================================

    FracDelay.cpp
    Created: 24 Apr 2026 4:27:12pm
    Author:  chenzuyu

    Fractional Delay is implemented by the all pass filter H(z) = (C + z^-1) / (1 + C*z^-1)
    for its phase delay linearly approximates Pc = (1 - C) / (1 + C) with Pc as the fractional delay.
 
  ==============================================================================
*/

class FracDelay
{
public:
    
    // set the filter coefficient
    void setDelay(float frac)
    {
        a = (1 - frac) / (1 + frac);
    }
    
    float process(float x)
    {
        float y = a * x + s; // state variable "s" stores the previous state
        s = x - a * y; // update the state varible for the current state
        return y;
    }
    
private:
    float a = 0.0f;
    float s = 0.0f; // old state
};
