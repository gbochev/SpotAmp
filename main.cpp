/*
SpotAmp v0.1

17.01.2026 GB

backend: glfw with opengl2

for linux:
sudo apt install libglfw3-dev portaudio19-dev

compile with:
g++ main.cpp lib/audio_viz.cpp lib/cJSON.c \
    lib/imgui.cpp lib/imgui_draw.cpp lib/imgui_tables.cpp lib/imgui_widgets.cpp \
    lib/backends/imgui_impl_glfw.cpp lib/backends/imgui_impl_opengl2.cpp \
    -Ilib -lGL -lglfw -lportaudio -lssl -lcrypto -pthread -o spotamp

./go-librespot --config_dir .



for all to run:

sh spotamp.sh

*/
#include <GLFW/glfw3.h>
#include <string>
#include <cstring>
#include <regex>
#include <chrono>
#include <iostream>

// HTTP
#define CPPHTTPLIB_OPENSSL_SUPPORT //for SSL/HTTPS
#include "lib/httplib.h"

// JSON
#include "lib/cJSON.h"

// ImGui
#include "lib/imgui.h"
#include "lib/backends/imgui_impl_glfw.h"
#include "lib/backends/imgui_impl_opengl2.h"
#include "audio_viz.h"

// ============================
// Spotify state
// ============================
std::string track_name  = "N/A";
std::string artist_name = "N/A";
int volume_value = 0;
int volume_max   = 100;
bool volume_initialized = false;
bool shuffle_enabled = false;
bool shuffle_initialized = false;
int track_position_ms = 0;   // current position
int track_duration_ms = 0;   // total duration
bool seek_initialized = false;


std::string full_text;
std::string display_text;
size_t scroll_index = 0;
size_t display_width = 43;
auto scroll_last = std::chrono::steady_clock::now();
const int scroll_ms = 300;

// ============================
// Spotify API
// ============================
auto status_last_refresh = std::chrono::steady_clock::now();
const int status_refresh_interval_ms = 1000;

bool post_json(const std::string &path, const std::string &body = "{}") {
    httplib::Client cli("127.0.0.1", 3678);
    auto res = cli.Post(path.c_str(), body, "application/json");
    return res && res->status == 200;
}

void playpause() { post_json("/player/playpause"); }
void next()      { post_json("/player/next"); }
void prev()      { post_json("/player/prev"); }

void load_track(const std::string &uri, bool paused = false) {
    cJSON *root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "uri", uri.c_str());
    cJSON_AddBoolToObject(root, "paused", paused);
    char *json = cJSON_PrintUnformatted(root);
    post_json("/player/play", json);
    cJSON_free(json);
    cJSON_Delete(root);
}

void refresh_status() {
    httplib::Client cli("127.0.0.1", 3678);
    auto res = cli.Get("/status");
    if (!res || res->status != 200) return;

    cJSON *status = cJSON_Parse(res->body.c_str());
    if (!status) return;

    cJSON *track = cJSON_GetObjectItem(status, "track");
    if (track) {
        cJSON *name = cJSON_GetObjectItem(track, "name");
        if (cJSON_IsString(name)) track_name = name->valuestring;

        cJSON *artists = cJSON_GetObjectItem(track, "artist_names");
        artist_name.clear();
        if (cJSON_IsArray(artists)) {
            int n = cJSON_GetArraySize(artists);
            for (int i = 0; i < n; i++) {
                cJSON *a = cJSON_GetArrayItem(artists, i);
                if (cJSON_IsString(a)) {
                    if (!artist_name.empty()) artist_name += ", ";
                    artist_name += a->valuestring;
                }
            }
        }
    }

    cJSON *vol = cJSON_GetObjectItem(status, "volume");
    cJSON *vol_max = cJSON_GetObjectItem(status, "volume_steps");
    if (cJSON_IsNumber(vol))     volume_value = vol->valueint;
    if (cJSON_IsNumber(vol_max)) volume_max   = vol_max->valueint;

    cJSON_Delete(status);
}

void get_volume() {
    httplib::Client cli("127.0.0.1", 3678);
    auto res = cli.Get("/player/volume");
    if (!res || res->status != 200) return;

    cJSON *root = cJSON_Parse(res->body.c_str());
    if (!root) return;

    cJSON *val = cJSON_GetObjectItem(root, "value");
    cJSON *max = cJSON_GetObjectItem(root, "max");

    if (cJSON_IsNumber(val)) volume_value = val->valueint;
    if (cJSON_IsNumber(max)) volume_max   = max->valueint;

    volume_initialized = true;
    cJSON_Delete(root);
}

void set_volume(int value) {
    cJSON *root = cJSON_CreateObject();
    cJSON_AddNumberToObject(root, "volume", value);
    cJSON_AddBoolToObject(root, "relative", false);

    char *json = cJSON_PrintUnformatted(root);
    post_json("/player/volume", json);

    cJSON_free(json);
    cJSON_Delete(root);
}

void get_shuffle() {
    httplib::Client cli("127.0.0.1", 3678);
    auto res = cli.Get("/status");
    if (!res || res->status != 200) return;

    cJSON *root = cJSON_Parse(res->body.c_str());
    if (!root) return;

    cJSON *shuffle = cJSON_GetObjectItem(root, "shuffle_context");
    if (cJSON_IsBool(shuffle)) {
        shuffle_enabled = cJSON_IsTrue(shuffle);
        shuffle_initialized = true;
    }

    cJSON_Delete(root);
}

void set_shuffle(bool enable) {
    cJSON *root = cJSON_CreateObject();
    cJSON_AddBoolToObject(root, "shuffle_context", enable);

    char *json = cJSON_PrintUnformatted(root);
    post_json("/player/shuffle_context", json);

    cJSON_free(json);
    cJSON_Delete(root);

    shuffle_enabled = enable; // keep state in sync
}

void get_seek() {
    httplib::Client cli("127.0.0.1", 3678);
    auto res = cli.Get("/status");
    if (!res || res->status != 200) return;

    cJSON *root = cJSON_Parse(res->body.c_str());
    if (!root) return;

    cJSON *track = cJSON_GetObjectItem(root, "track");
    if (track) {
        cJSON *pos = cJSON_GetObjectItem(track, "position");  // current ms
        cJSON *dur = cJSON_GetObjectItem(track, "duration");  // total ms
        if (cJSON_IsNumber(pos)) track_position_ms = pos->valueint;
        if (cJSON_IsNumber(dur)) track_duration_ms = dur->valueint;
        seek_initialized = true;
    }

    cJSON_Delete(root);
}

void set_seek(int pos_ms) {
    cJSON *root = cJSON_CreateObject();
    cJSON_AddNumberToObject(root, "position", pos_ms);
    cJSON_AddBoolToObject(root, "relative", false); // absolute position

    char *json = cJSON_PrintUnformatted(root);
    post_json("/player/seek", json);

    cJSON_free(json);
    cJSON_Delete(root);

    track_position_ms = pos_ms; // keep state in sync
}



// ============================
// Scrolling text
// ============================
void update_scroll() {
    if (full_text.empty()) return;
    scroll_index = (scroll_index + 1) % full_text.size();
    display_text.clear();
    for (size_t i = 0; i < display_width; i++) {
        display_text += full_text[(scroll_index + i) % full_text.size()];
    }
}

// ============================
// URI handler (from URL spotify share)
// ============================

// Converts a Spotify share URL into spotify:track:... or spotify:playlist:...
// If input is already a URI, it returns it unchanged.
std::string handle_paste(const std::string &input) {
    std::string uri = input;

    // Regex for open.spotify.com links
    // Examples:
    // https://open.spotify.com/track/7FKUc1mFraEOl4k6sdcwfb?si=a3555da3e9444ef1
    // https://open.spotify.com/playlist/0iAeUtwINlqfjwAyQ4ykur?si=jrwFsnk8RzCNZb4-xI19uw
    std::regex rgx(R"(https?://open\.spotify\.com/(track|playlist|album)/([a-zA-Z0-9]+))");
    std::smatch match;

    if (std::regex_search(input, match, rgx)) {
        if (match.size() >= 3) {
            std::string type = match[1];
            std::string id   = match[2];
            uri = "spotify:" + type + ":" + id;
        }
    }

    // Otherwise, return the input as-is
    return uri;
}

// ============================
// Main
// ============================
int main() {
    if (!glfwInit()) return 1;

    GLFWwindow *window = glfwCreateWindow(440, 110, "SpotAmp", nullptr, nullptr);
    glfwSetWindowSizeLimits(window, 440, 110, 440, 110);
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    ImGui::StyleColorsDark();
    
    ImFont* defaultFont = io.Fonts->AddFontDefault();
    ImFont* pixelFont = io.Fonts->AddFontFromFileTTF("fonts/BetterVCR 25.09.ttf", 12.0f);
    ImFont* songTitleFont = io.Fonts->AddFontFromFileTTF("fonts/UbuntuMono-Regular.ttf", 18.0f);

    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL2_Init();

    refresh_status();
    get_volume();
    get_shuffle();
    get_seek();

    // initAudioCapture();

    std::string song_uri = "spotify:track:6mfOyqROx7tnXkL9pNAp75";
    static char buffer[256] = {};

    while (!glfwWindowShouldClose(window)) {
        // glfwMakeContextCurrent(window); //only when using more tha 1 window
        glfwPollEvents();

        ImGui_ImplOpenGL2_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        ImGuiIO &io = ImGui::GetIO();
        ImGui::SetNextWindowPos(ImVec2(0, 0));
        ImGui::SetNextWindowSize(io.DisplaySize);

        ImGui::Begin("SpotAmp", nullptr,
            ImGuiWindowFlags_NoTitleBar |
            ImGuiWindowFlags_NoResize |
            ImGuiWindowFlags_NoMove);

        //////////////////////////////////
        ImGui::PushFont(songTitleFont);
        ImGui::Text("%s", display_text.c_str());
        ImGui::PopFont();
        //////////////////////////////////
        if (seek_initialized && track_duration_ms > 0) {
            int prev_pos = track_position_ms;

            // Slider with range 0 → track duration
            ImGui::SetNextItemWidth(385.0f); // pixels
            ImGui::SliderInt(
                "Seek",
                &track_position_ms,
                0,
                track_duration_ms,
                "",
                ImGuiSliderFlags_AlwaysClamp
            );

            // Only send new seek if user changed the slider
            if (track_position_ms != prev_pos) {
                set_seek(track_position_ms);
            }

            // Optional: show current / total in seconds
            ImGui::SetNextItemWidth(50.0f);
            ImGui::Text("%02d:%02d / %02d:%02d",
                track_position_ms / 60000,
                (track_position_ms / 1000) % 60,
                track_duration_ms / 60000,
                (track_duration_ms / 1000) % 60
            );
        } else {
            ImGui::SetNextItemWidth(385.0f); // pixels
            ImGui::SliderInt(
                "Seek",
                &track_position_ms,
                0,
                0,
                "",
                ImGuiSliderFlags_AlwaysClamp
            );
            ImGui::SetNextItemWidth(50.0f);
            ImGui::Text("00:00 / 00:00");
        }


        //////////////////////////////////
        ImGui::PushFont(pixelFont);
        ImGui::SameLine();
        if (ImGui::Button(" ▶/⏸ ")) playpause();
        ImGui::SameLine();
        if (ImGui::Button(" ⏮ ")) prev();
        ImGui::SameLine();
        if (ImGui::Button(" ⏭ ")) next();
        ImGui::PopFont();

        // ImGui::SameLine();
        // if (ImGui::Button(" I>/|| ")) playpause();
        // ImGui::SameLine();
        // if (ImGui::Button(" << ")) prev();
        // ImGui::SameLine();
        // if (ImGui::Button(" >> ")) next();

        //shuffle
        ImGui::SameLine();
        if (shuffle_initialized) {
            bool prev = shuffle_enabled;
            if (ImGui::Checkbox("S", &shuffle_enabled)) {
                set_shuffle(shuffle_enabled);
            }
        }

        // Volume
        ImGui::SameLine();
        if (volume_initialized) {
            int prev_volume = volume_value;
            ImGui::SetNextItemWidth(130.0f); // pixels
            ImGui::SliderInt(
                "Vol",
                &volume_value,
                0,
                volume_max,
                "%d"
            );

            if (ImGui::IsItemHovered()) {
                ImGuiIO &io = ImGui::GetIO();
                if (io.MouseWheel != 0.0f) {
                    // change 1 step per wheel tick
                    volume_value += static_cast<int>(io.MouseWheel * 1); 
                    
                    // clamp to valid range
                    if (volume_value < 0) volume_value = 0;
                    if (volume_value > volume_max) volume_value = volume_max;
                }
            }

            // Send to API only if user changed it
            if (volume_value != prev_volume) {
                set_volume(volume_value);
            }
        } else {
            ImGui::Text("Volume: syncing...");
        }

        ///////////////////////////////////

        if (ImGui::Button("Load")) load_track(song_uri);
        ImGui::SameLine();

        if (buffer[0] == 0) {
            strncpy(buffer, song_uri.c_str(), sizeof(buffer));
        }

        ImGui::SetNextItemWidth(341.0f); // pixels
        if (ImGui::InputText("URI", buffer, sizeof(buffer))) {
            song_uri = handle_paste(buffer);
            // optionally, update buffer so the field shows canonical URI
            std::strncpy(buffer, song_uri.c_str(), sizeof(buffer));
            buffer[sizeof(buffer)-1] = '\0';
        }

        ImGui::End();

        ImGui::Render();
        int w, h;
        glfwGetFramebufferSize(window, &w, &h);
        glViewport(0, 0, w, h);
        glClearColor(0.1f, 0.1f, 0.1f, 1);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL2_RenderDrawData(ImGui::GetDrawData());
        glfwSwapBuffers(window);

        auto now = std::chrono::steady_clock::now();

        if (std::chrono::duration_cast<std::chrono::milliseconds>(now - status_last_refresh).count() > status_refresh_interval_ms) {
            refresh_status();
            get_seek();
            full_text = track_name + " by " + artist_name + "    ";
            status_last_refresh = now;
            if (track_name != "N/A") {
                std::string title = "SpotAmp - " + track_name + " by " + artist_name;
                glfwSetWindowTitle(window, title.c_str());
            }
        }

        if (std::chrono::duration_cast<std::chrono::milliseconds>(now - scroll_last).count() > scroll_ms) {
            update_scroll();
            scroll_last = now;
        }


        // drawAudioWindow();
    }

    ImGui_ImplOpenGL2_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    glfwDestroyWindow(window);
    // stopAudioCapture();
    glfwTerminate();
    return 0;
}
