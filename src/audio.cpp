/*
 * Copyright (C) 2025 Andreas Kromke, andreas.kromke@gmail.com
 *
 * This program is free software; you can redistribute it or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 3
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

/*
*
* Manages audio output via [https://wiki.libsdl.org/SDL2_mixer]
*
*/

#include <SDL2/SDL.h>
#include <SDL2/SDL_mixer.h>
#include "audio.h"


#if 0
// without mixer
struct AudioData
{
    Uint8* position;
    Uint32 length;
};

void audioCallback(void* userData, Uint8* stream, int streamLength)
{
    AudioData* audio = (AudioData*)userData;

    if (audio->length == 0)
    {
        return;
    }

    Uint32 length = (Uint32)streamLength;

    length = (length > audio->length ? audio->length : length);

    SDL_memcpy(stream, audio->position, length);

    audio->position += length;
    audio->length -= length;
}

int main(int argc, char** argv)
{
    SDL_Init(SDL_INIT_AUDIO);

    SDL_AudioSpec wavSpec;
    Uint8* wavStart;
    Uint32 wavLength;
    char* filePath = "ADD YOUR FILE PATH HERE";

    if (SDL_LoadWAV(filePath, &wavSpec, &wavStart, &wavLength) == NULL)
    {
        cerr << "Error: file could not be loaded as an audio file." << endl;
    }

    AudioData audio;
    audio.position = wavStart;
    audio.length = wavLength;

    wavSpec.callback = audioCallback;
    wavSpec.userdata = &audio;

    SDL_AudioDeviceID audioDevice;
    audioDevice = SDL_OpenAudioDevice(NULL, 0, &wavSpec, NULL, SDL_AUDIO_ALLOW_ANY_CHANGE);

    if (audioDevice == 0)
    {
        cerr << "Error: " << SDL_GetError() << endl;
        return 1;
    }

    SDL_PauseAudioDevice(audioDevice, 0);

    while (audio.length > 0)
    {
        SDL_Delay(100);
    }

    SDL_CloseAudioDevice(audioDevice);
    SDL_FreeWAV(wavStart);
    SDL_Quit();

    return 0;
}
#endif

/*

read from memory:

extern DECLSPEC SDL_RWops *SDLCALL SDL_RWFromMem(void *mem, int size);
extern DECLSPEC Mix_Music * SDLCALL Mix_LoadMUS_RW(SDL_RWops *src, int freesrc);

use "xxd -i" to convert binary file to C struct

*/

static Mix_Music *music_click = nullptr;
static Mix_Music *music_pling = nullptr;


int CAudio::init(const char *click, const char *pling)
{
    static const int init_flags = MIX_INIT_MP3;

    if (SDL_Init(SDL_INIT_AUDIO) < 0)
    {
        printf("SDL_Init(SDL_INIT_AUDIO) -> %s\n", Mix_GetError());
        return -1;
    }

    if (init_flags != Mix_Init(init_flags))
    {
        printf("Mix_Init() -> %s\n", Mix_GetError());
        return -2;
    }

    static const int frequency = 22050;
    static const Uint16 format = AUDIO_S16SYS;
    static const int channels = 2;
    static const int chunksize = 640;
    if (Mix_OpenAudio(frequency, format, channels, chunksize))
    {
        printf("Mix_OpenAudio() -> %s\n", Mix_GetError());
        return -4;
    }

    music_click = Mix_LoadMUS(click);
    if (music_click == nullptr)
    {
        printf("Mix_LoadMUS() -> %s\n", Mix_GetError());
        return -5;
    }

    music_pling = Mix_LoadMUS(pling);
    if (music_pling == nullptr)
    {
        printf("Mix_LoadMUS() -> %s\n", Mix_GetError());
        return -5;
    }

    return 0;
}

void CAudio::exit()
{
    while (Mix_PlayingMusic())
    {
    }

    if (music_click != nullptr)
    {
        Mix_FreeMusic(music_click);
        music_click = nullptr;
    }

    if (music_pling != nullptr)
    {
        Mix_FreeMusic(music_pling);
        music_pling = nullptr;
    }

    SDL_Quit();
}

void CAudio::play_click()
{
    if ((music_click != nullptr) && Mix_PlayMusic(music_click, 0))    // 0: play once and stop
    {
        printf("Mix_PlayMusic() -> %s\n", Mix_GetError());
    }
}
void CAudio::play_pling()
{
    if ((music_pling != nullptr) && Mix_PlayMusic(music_pling, 0))    // 0: play once and stop
    {
        printf("Mix_PlayMusic() -> %s\n", Mix_GetError());
    }
}
