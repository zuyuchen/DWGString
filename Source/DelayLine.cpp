/*
  ==============================================================================

    DelayLine.cpp
    Created: 23 Apr 2026 2:34:40pm
    Author:  chenzuyu

  ==============================================================================
*/

#include <vector>
#include <cmath>

class DelayLine
{
public:
    // Initialize the delay line size with integer delay
    void prepare (int delaySamples)
    {
        size = delaySamples;
        buffer.assign (size, 0.0f);
        
        // set the read pos and write pos at the boundary
        writePtr = 0;
        readPtr = 0;
    }

    float read() const
    {
        return buffer[readPtr];
    }
    
    void write(float x)
    {
        buffer[writePtr] = x;
    }
    
    void advance()
    {
        writePtr = (writePtr + 1) % size;
        readPtr = (readPtr + 1) % size;
    }
    
    void clear()
    {
        std::fill(buffer.begin(), buffer.end(), 0.0f);
        writePtr = 0;
        readPtr = 0;
    }
    
    int getSize() const { return size; }
    
private:
    std::vector<float> buffer; //DelayLine buffer
    int size = 0;
    int writePtr = 0;
    int readPtr = 0;
};
