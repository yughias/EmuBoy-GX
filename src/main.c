#include "SDL_MAINLOOP.h"
#include "gba.h"
#include "gba_recorder.h"
#include "frontend.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

float lastMousePressed = 0;

void setup(){
    size(SCREEN_WIDTH, SCREEN_HEIGHT);
    setScaleMode(NEAREST);
    setTitle(u8"エミュボーイ　GX");

    if(getArgc() != 2)
        frontend_filename[0] = 0;
    else
        strcpy(frontend_filename, getArgv(1));

    frontend_createMenu();

    char* exe_path = SDL_GetBasePath();
    char data_path[FILENAME_MAX];
    char logo_path[FILENAME_MAX];

    strncpy(data_path, exe_path, FILENAME_MAX - 1);
    strncat(data_path, "data/", FILENAME_MAX - 1);
    strncpy(logo_path, data_path, FILENAME_MAX - 1);
    strncpy(frontend_bios_path, data_path, FILENAME_MAX - 1);
    strncat(logo_path, "logo.bmp", FILENAME_MAX - 1);
    SDL_free(exe_path);

    setWindowIcon(logo_path);
    frameRate(REFRESH_RATE);
    #ifndef EMSCRIPTEN
    strncat(frontend_bios_path, "gba_bios.bin", FILENAME_MAX - 1);
    #else
    strncat(frontend_bios_path, "vba_bios.bin", FILENAME_MAX - 1);
    #endif

    SDL_AudioSpec audioSpec;
    audioSpec.freq = 44100;
    audioSpec.channels = 2;
    audioSpec.format = AUDIO_S16;
    audioSpec.samples = SAMPLE_PER_CALL;
    audioSpec.callback = audioCallback;
    audioSpec.userdata = &frontend_gba.apu.sample_buffer;
    frontend_audioDev = SDL_OpenAudioDevice(NULL, 0, &audioSpec, &audioSpec, 0);

    initGba(&frontend_gba, frontend_bios_path, frontend_filename, audioSpec);

    noRender();

    init_keypad();

    onExit = frontend_free;

    SDL_PauseAudioDevice(frontend_audioDev, 0);
}

void loop(){   
    check_controller_connection();

    if(isMouseReleased){
        float newPressed = millis();
        if(newPressed - lastMousePressed < 300)
            fullScreen();
        lastMousePressed = newPressed;
    }

    if(isKeyReleased){
        if(keyReleased == '-')
            frontend_decreaseSpeed();
        if(keyReleased == '=')
            frontend_increaseSpeed();
        if(keyReleased == SDLK_F1){
            char filename[FILENAME_MAX];
            getFilenameWithDate(filename, frontend_filename);
            frontend_triggerGbaAudioRecorder(frontend_audioDev, frontend_gba.apu.audioSpec, filename);
        }
        if(keyReleased == SDLK_F2)
            frontend_startCheatEngineSearch(&frontend_gba);
        if(keyReleased == SDLK_F3)
            frontend_continueCheatEngineSearch(&frontend_gba);
        if(keyReleased == SDLK_F4)
            frontend_writeToFoundAddressesCheatEngine(&frontend_gba);
        if(keyReleased == SDLK_F5)
            frontend_writeToSingleAddressCheatEngine(&frontend_gba);
        if(keyReleased == SDLK_F12){
            char filename[FILENAME_MAX];
            getFilenameWithDate(filename, frontend_filename);
            frontend_writeScreenShotToFile(pixels, width, height, filename);
        }

        if(keystate[SDL_SCANCODE_LCTRL]){
            if(keyReleased == SDLK_o)
                frontend_loadGame();
            if(keyReleased == SDLK_p)
                frontend_changePauseState();
            if(keyReleased == SDLK_r)
                frontend_reset();
            if(keyReleased == SDLK_s)
                frontend_writeSaveStateToFile(&frontend_gba, frontend_filename);
            if(keyReleased == SDLK_l)
                frontend_readSaveStateFromFile(&frontend_gba, frontend_filename);
        }

        frontend_gba.apu.samplePushRate = CYCLES_PER_FRAME * REFRESH_RATE * frontend_speed / frontend_gba.apu.audioSpec.freq;
        frontend_gba.ppu.frameSkip = frontend_speed >> 1;
    }

    for(int i = 0; i < frontend_speed && !frontend_pause; i++)
        emulateGba(&frontend_gba);
}