#pragma once

#include <vector>
#include <atomic>
#include <thread>
#include <cstdint>

class AudioFFT {
public:
    explicit AudioFFT(int fftSize = 1024);
    ~AudioFFT();

    void start();
    void stop();

    // frameCount = number of stereo frames
    void pushAudio(const int16_t* samples, int frameCount);

    std::vector<float>& getDisplayVector() { return displayVector; }
    std::vector<float> waveform;
    std::vector<float> fftMagnitude;


private:
    void threadFunc();

    int fftSize;

    // Circular audio buffer
    std::vector<float> audioBuffer;
    std::vector<float> fftInput;
    

    // What ImGui reads
    std::vector<float> displayVector;
    
    // Threading / state
    std::atomic<int> writeIndex;
    std::atomic<bool> running;
    std::atomic<bool> hasData;
    float peak;

    std::thread fftThread;
};
