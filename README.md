# SpotAmp
Winamp like player for Spotify

### Description
SpotAmp is a small Winamp inspired Spotify client that uses imgui as interface library, GLFW as graphics backend and go-librespot as Spotify client library.

### How to use
#### Linux
Download the release folder, then start the sh script with:
```
sh spotamp.sh
```
and wait for the window to open. Once it shows up, open Spotify client and select from the output the new device (to reroute the audio). One this is done you should be able to see in the sh terminal that you are logged it. You need to do this only once, it will then reuse your credentials but at first you will need Spotify client installed.
\
\
![Main window](https://github.com/gbochev/SpotAmp/blob/main/Screenshot_2026-01-24_13-40-25.png)
\
\
Once it connects you can paste into the URI text field a link from Spotify (via the share button for a song, album or playlist - copy it and paste it into the field). Then you need to press enter there or click outside the box. It will extract just the needed id. Then you must click the load button and it should start playing.\
The S checkbox is for shuffle. Only works after a playlist is loaded. If you load track and click it will hang. This is a bug. If there is something wrong usually you can always stop it with Ctrl+C on the sh terminal window (this will kill both SpotAmp and the go-librespot instance).

#### Windows
Not compiled so far and not tested. Current release is only for linux. Written in a multiplatform way, so it should compile with the default tools and should work without any changes in the code.

### How to compile
#### Linux
Make sure that you have glfw (for ubuntu: ``` sudo apt install libglfw3-dev ```) and build tools. Compile the main file with:

```
g++ main.cpp lib/audio_engine.cpp lib/audio_fft.cpp lib/cJSON.c lib/imgui.cpp lib/imgui_draw.cpp lib/imgui_tables.cpp lib/imgui_widgets.cpp lib/backends/imgui_impl_glfw.cpp lib/backends/imgui_impl_opengl2.cpp -Ilib -lGL -lglfw -lssl -lcrypto -pthread -lpthread -lm -o spotamp
```
And then start it the usual way with:
```
sh spotamp.sh
```
### Used libraries
[go-librespot](https://github.com/devgianlu/go-librespot/tree/master)
[imgui](https://github.com/ocornut/imgui)
[httplib](https://github.com/yhirose/cpp-httplib)
[cJSON](https://github.com/DaveGamble/cJSON)
[miniaudio](https://github.com/mackron/miniaudio)
[pocketfft](https://github.com/mreineck/pocketfft)


