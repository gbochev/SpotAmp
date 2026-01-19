#pragma once
#include <vector>
#include <mutex>

void initAudioCapture();
void stopAudioCapture();
void drawAudioWindow(); // draws a separate GLFW window with waveform / VU
