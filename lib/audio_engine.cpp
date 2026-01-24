#include "audio_engine.h"
#include "audio_fft.h"

#include <atomic>
#include <thread>
#include <vector>
#include <cstring>


#define MINIAUDIO_IMPLEMENTATION
#include "miniaudio.h"

#ifdef _WIN32
    #include <windows.h>
#else
    #include <fcntl.h>
    #include <unistd.h>
    #include <sys/stat.h>
#endif

// ============================
// Audio format (librespot)
// ============================
static constexpr int SAMPLE_RATE       = 44100;
static constexpr int CHANNELS          = 2;
static constexpr int BYTES_PER_SAMPLE  = 2; // s16
static constexpr int FRAME_BYTES       = CHANNELS * BYTES_PER_SAMPLE;

// ============================
// Pipe handle
// ============================
#ifdef _WIN32
static HANDLE pipeHandle = INVALID_HANDLE_VALUE;
static const char* PIPE_NAME = R"(\\.\pipe\spotamp_audio)";
#else
static int pipeFd = -1;
static const char* PIPE_NAME = "/tmp/spotamp_audio";
#endif

// ============================
// Miniaudio device
// ============================
static ma_device device;
static std::atomic<bool> running{false};

// ============================
// Miniaudio callback (reads pipe in real-time)
// ============================
extern AudioFFT* gAudioFFT; //FFT object defined in main.cpp


static void audio_callback(ma_device*, void* output, const void*, ma_uint32 frameCount)
{
    size_t bytesNeeded = frameCount * FRAME_BYTES;
    uint8_t* out = static_cast<uint8_t*>(output);

#ifdef _WIN32
    if (pipeHandle == INVALID_HANDLE_VALUE) {
        std::memset(out, 0, bytesNeeded);
        return;
    }

    DWORD bytesRead = 0;
    if (!ReadFile(pipeHandle, out, (DWORD)bytesNeeded, &bytesRead, NULL) || bytesRead == 0) {
        std::memset(out, 0, bytesNeeded);
        return;
    }

    if ((size_t)bytesRead < bytesNeeded) {
        std::memset(out + bytesRead, 0, bytesNeeded - bytesRead);
    }

    int framesRead = bytesRead / FRAME_BYTES;

#else
    if (pipeFd < 0) {
        std::memset(out, 0, bytesNeeded);
        return;
    }

    ssize_t bytesRead = read(pipeFd, out, bytesNeeded);
    if (bytesRead <= 0) {
        std::memset(out, 0, bytesNeeded);
        return;
    }

    if ((size_t)bytesRead < bytesNeeded) {
        std::memset(out + bytesRead, 0, bytesNeeded - bytesRead);
    }

    int framesRead = bytesRead / FRAME_BYTES;
#endif

    //push ONLY valid frames
    if (gAudioFFT && framesRead > 0) {
        // static int dbg = 0;
        // if (++dbg % 200 == 0) {
        //     int16_t* s = reinterpret_cast<int16_t*>(out);
        //     printf("audio sample L=%d R=%d\n", s[0], s[1]);
        // }
        gAudioFFT->pushAudio(reinterpret_cast<int16_t*>(out), framesRead);
    }
}

// ============================
// Pipe open thread (non-blocking init)
// ============================
static void pipe_open_thread() {
#ifdef _WIN32
    while (running && pipeHandle == INVALID_HANDLE_VALUE) {
        pipeHandle = CreateFileA(
            PIPE_NAME,
            GENERIC_READ,
            0,
            NULL,
            OPEN_EXISTING,
            0,
            NULL
        );

        if (pipeHandle == INVALID_HANDLE_VALUE) {
            Sleep(200);
        }
    }
#else
    // Ensure FIFO exists
    mkfifo(PIPE_NAME, 0666);

    while (running && pipeFd < 0) {
        pipeFd = open(PIPE_NAME, O_RDONLY);
        if (pipeFd < 0) {
            usleep(200000); // 200 ms
        }
    }
#endif
}

// ============================
// Public API
// ============================
bool audio_init() {
    running = true;

    // Start pipe open thread
    std::thread(pipe_open_thread).detach();

    // Init miniaudio
    ma_device_config cfg = ma_device_config_init(ma_device_type_playback);
    cfg.playback.format   = ma_format_s16;
    cfg.playback.channels = CHANNELS;
    cfg.sampleRate        = SAMPLE_RATE;
    cfg.dataCallback      = audio_callback;

    if (ma_device_init(NULL, &cfg, &device) != MA_SUCCESS) {
        running = false;
        return false;
    }

    if (ma_device_start(&device) != MA_SUCCESS) {
        ma_device_uninit(&device);
        running = false;
        return false;
    }

    return true;
}

void audio_shutdown() {
    running = false;

#ifdef _WIN32
    if (pipeHandle != INVALID_HANDLE_VALUE) {
        CloseHandle(pipeHandle);
        pipeHandle = INVALID_HANDLE_VALUE;
    }
#else
    if (pipeFd >= 0) {
        close(pipeFd);
        pipeFd = -1;
    }
#endif

    ma_device_uninit(&device);
}
