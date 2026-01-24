#include "audio_fft.h"

#include <thread>
#include <cmath>
#include <algorithm>
#include <complex>
#include <chrono>

#include "pocketfft_hdronly.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif


AudioFFT::AudioFFT(int fftSize_)
    : fftSize(fftSize_),
      writeIndex(0),
      running(false),
      hasData(false),
      peak(1e-3f)
{
    audioBuffer.resize(fftSize, 0.0f);
    fftInput.resize(fftSize, 0.0f);
    fftMagnitude.resize(fftSize / 2 + 1, 0.0f);

    // Number of vertical lines in ImGui plot
    displayVector.resize(200, 0.0f);
    waveform.resize(512, 0.0f);
}

AudioFFT::~AudioFFT() {
    stop();
}

void AudioFFT::start() {
    running = true;
    fftThread = std::thread(&AudioFFT::threadFunc, this);
}

void AudioFFT::stop() {
    running = false;
    if (fftThread.joinable())
        fftThread.join();
}

// Push stereo interleaved s16 samples
void AudioFFT::pushAudio(const int16_t* samples, int frameCount) {
    for (int i = 0; i < frameCount; i++) {
        float mono =
            (samples[i * 2] + samples[i * 2 + 1]) * (1.0f / 32768.0f) * 0.5f;

        audioBuffer[writeIndex] = mono;
        writeIndex = (writeIndex + 1) % fftSize;

        if (writeIndex == 0)
            hasData = true;
    }

    // DEBUG waveform capture
    for (int i = 0; i < frameCount && i < waveform.size(); i++) {
        waveform[i] = (samples[i*2] + samples[i*2+1]) * (1.0f / 65536.0f);
    }
}

void AudioFFT::threadFunc() {
    // static constexpr int SAMPLE_RATE       = 44100;
    // using namespace pocketfft;

    // const float smoothing = 0.80f;
    // const int displayBins = (int)displayVector.size();

    // shape_t shape{ (size_t)fftSize };
    // stride_t stride{ 1 };
    // shape_t axes{ 0 };

    // std::vector<std::complex<float>> fftOut(fftSize / 2 + 1);

    // // ---- Precompute log-frequency bin mapping (ONCE) ----
    // const float minFreq = 20.0f;
    // const float maxFreq = SAMPLE_RATE * 0.5f;

    // std::vector<int> logBinStart(displayBins);
    // std::vector<int> logBinEnd(displayBins);

    // for (int i = 0; i < displayBins; i++) {
    //     float t0 = i / (float)displayBins;
    //     float t1 = (i + 1) / (float)displayBins;

    //     float f0 = minFreq * std::pow(maxFreq / minFreq, t0);
    //     float f1 = minFreq * std::pow(maxFreq / minFreq, t1);

    //     int b0 = (int)(f0 * fftSize / SAMPLE_RATE);
    //     int b1 = (int)(f1 * fftSize / SAMPLE_RATE);

    //     b0 = std::clamp(b0, 1, fftSize / 2);
    //     b1 = std::clamp(b1, b0 + 1, fftSize / 2 + 1);

    //     logBinStart[i] = b0;
    //     logBinEnd[i]   = b1;
    // }

    // constexpr float dbMin = -100.0f;
    // constexpr float dbMax = -20.0f;
    // constexpr float eps   = 1e-12f;

    // std::vector<float> prevDisplay(displayBins, 0.0f);

    // while (running) {
    //     if (!hasData) {
    //         std::this_thread::sleep_for(std::chrono::milliseconds(10));
    //         continue;
    //     }

    //     // ---- Copy circular buffer (oldest -> newest) ----
    //     int start = writeIndex.load();
    //     for (int i = 0; i < fftSize; i++)
    //         fftInput[i] = audioBuffer[(start + i) % fftSize];

    //     // ---- Hann window ----
    //     for (int i = 0; i < fftSize; i++) {
    //         float w = 0.5f * (1.0f - std::cos(2.0f * M_PI * i / (fftSize - 1)));
    //         fftInput[i] *= w;
    //     }

    //     // ---- Remove DC offset ----
    //     float mean = 0.0f;
    //     for (float v : fftInput)
    //         mean += v;
    //     mean /= fftSize;

    //     for (float& v : fftInput)
    //         v -= mean;

    //     // ---- FFT ----
    //     r2c(shape, stride, stride, axes,
    //         FORWARD, fftInput.data(), fftOut.data(), 1.0f);

    //     fftOut[0] = 0.0f; // kill DC explicitly

    //     // ---- Power → dB ----
    //     for (size_t i = 1; i < fftOut.size(); i++) {
    //         float power = std::norm(fftOut[i]);           // REAL FIX
    //         float db = 10.0f * std::log10(power + eps);   // power → dB

    //         db = std::clamp(db, dbMin, dbMax);

    //         fftMagnitude[i] =
    //             fftMagnitude[i] * smoothing + db * (1.0f - smoothing);
    //     }

    //     // ---- Log-frequency downsampling with bin averaging ----
    //     for (int i = 0; i < displayBins; i++) {
    //         float sum = 0.0f;
    //         int count = 0;

    //         for (int b = logBinStart[i]; b < logBinEnd[i]; b++) {
    //             sum += fftMagnitude[b];
    //             count++;
    //         }

    //         float db = (count > 0) ? sum / count : dbMin;

    //         float norm = (db - dbMin) / (dbMax - dbMin);
    //         norm = std::clamp(norm, 0.0f, 1.0f);

    //         displayVector[i] =
    //             prevDisplay[i] * smoothing + norm * (1.0f - smoothing);
    //     }

    //     prevDisplay = displayVector;

    //     std::this_thread::sleep_for(std::chrono::milliseconds(16));
    // }
}
