#include "audio_viz.h"
#include <portaudio.h>
#include <GLFW/glfw3.h>
#include <iostream>

static std::vector<float> audioBuffer(1024, 0.0f);
static std::mutex audioMutex;
static PaStream* stream = nullptr;
static GLFWwindow* vizWindow = nullptr;


int getDefaultMonitorDevice() {
    int numDevices = Pa_GetDeviceCount();
    if (numDevices < 0) {
        std::cerr << "ERROR: Pa_CountDevices returned " << numDevices << std::endl;
        return -1;
    }

    for (int i = 0; i < numDevices; ++i) {
        const PaDeviceInfo* info = Pa_GetDeviceInfo(i);
        if (!info) continue;

        std::string name(info->name);

        // Make sure it has input channels
        if (info->maxInputChannels <= 0) continue;

        // Look for monitor devices
        if (name.find(".monitor") != std::string::npos) {
            std::cout << "Using monitor device: " << name << " (index " << i << ")\n";
            return i;
        }
    }

    std::cerr << "No monitor device found. Using default input.\n";
    return Pa_GetDefaultInputDevice();
}

// ----------------- PortAudio callback -----------------
static int paCallback(const void* input, void* output,
                      unsigned long frameCount,
                      const PaStreamCallbackTimeInfo* timeInfo,
                      PaStreamCallbackFlags statusFlags,
                      void* userData) {
    const float* in = (const float*)input;
    if (in) {
        std::lock_guard<std::mutex> lock(audioMutex);
        for (unsigned long i = 0; i < frameCount && i < audioBuffer.size(); ++i)
            audioBuffer[i] = in[i];
    }
    return paContinue;
}

// ----------------- Init / Stop -----------------
void initAudioCapture() {
    Pa_Initialize();

    PaStreamParameters inputParams;
    inputParams.device = getDefaultMonitorDevice(); // auto pick monitor
    inputParams.channelCount = 2;                  // stereo
    inputParams.sampleFormat = paFloat32;
    inputParams.suggestedLatency =
        Pa_GetDeviceInfo(inputParams.device)->defaultLowInputLatency;
    inputParams.hostApiSpecificStreamInfo = nullptr;

    PaError err = Pa_OpenStream(&stream,
                                &inputParams,    // input
                                nullptr,         // no output
                                48000,           // sample rate (match monitor)
                                256,             // frames per buffer
                                paClipOff,
                                paCallback,
                                nullptr);

    if (err != paNoError) {
        std::cerr << "Failed to open stream: " << Pa_GetErrorText(err) << "\n";
    } else {
        Pa_StartStream(stream);
    }

    // Create a new GLFW window for visualization
    vizWindow = glfwCreateWindow(600, 200, "SpotAmp Visualizer", nullptr, nullptr);
    if (!vizWindow) {
        std::cerr << "Failed to create visualizer window\n";
        return;
    }
    glfwMakeContextCurrent(vizWindow);
    glOrtho(-1, 1, -1, 1, -1, 1);
}

void stopAudioCapture() {
    if (stream) {
        Pa_StopStream(stream);
        Pa_CloseStream(stream);
        Pa_Terminate();
        stream = nullptr;
    }
    if (vizWindow) {
        glfwDestroyWindow(vizWindow);
        vizWindow = nullptr;
    }
}

// ----------------- Draw visual -----------------
void drawAudioWindow() {
    if (!vizWindow || glfwWindowShouldClose(vizWindow)) return;

    glfwMakeContextCurrent(vizWindow);
    glClear(GL_COLOR_BUFFER_BIT);

    glColor3f(0.0f, 1.0f, 0.0f);
    glBegin(GL_LINE_STRIP);
    {
        std::lock_guard<std::mutex> lock(audioMutex);
        int N = audioBuffer.size();
        for (int i = 0; i < N; ++i) {
            float x = (float)i / (N - 1) * 2.0f - 1.0f;
            float y = audioBuffer[i];
            glVertex2f(x, y);
        }
    }
    glEnd();

    glfwSwapBuffers(vizWindow);
    glfwPollEvents();
}
