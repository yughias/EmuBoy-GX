#ifndef _SDL_MAINLOOP_H_
#define _SDL_MAINLOOP_H_

#include <SDL2/SDL.h>
#include <SDL2/SDL_opengl.h>

#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#define frameRate(x) frameRate = x

#define LEFT SDLK_LEFT
#define RIGHT SDLK_RIGHT
#define UP SDLK_UP
#define DOWN SDLK_DOWN
#define BUTTON_LEFT SDL_BUTTON_LEFT
#define BUTTON_RIGHT SDL_BUTTON_RIGHT

typedef int keyboard;
typedef Uint32 button;

extern unsigned int displayWidth;
extern unsigned int displayHeight;
extern int width;
extern int height;
extern uint16_t* pixels;

extern float frameRate;
extern unsigned int frameCount;
extern float deltaTime;

extern int pmouseX;
extern int pmouseY;
extern int mouseX;
extern int mouseY;
extern bool isMousePressed;
extern bool isMouseReleased;
extern bool isMouseDragged;
extern button mouseButton;
extern bool isKeyPressed;
extern bool isKeyReleased;
extern keyboard keyPressed;
extern keyboard keyReleased;
extern button exitButton;

void setup();
void loop();
void size(int, int);
void setTitle(const char*);
void setWindowIcon(const char*);
double millis();
void fullScreen();
void background(int);
int color(int, int, int);
void getRGB(int, Uint8*, Uint8*, Uint8*);
void rect(int, int, int, int, int);
void drawText(SDL_Surface* surface, int x, int y, const char* string);

// Custom callbacks
extern void (*onExit)();
extern void (*onKeyPressed)(keyboard);
extern void (*onKeyReleased)(keyboard);

void noRender();
void autoRender();
void renderPixels();

// getter for program arguments
int getArgc();
const char* getArgv(int);

typedef enum {NEAREST, LINEAR, ANISOTROPIC} ScaleMode;
typedef GLuint* Shader;
Shader loadShader(const char*);
void noGlobalShader();
void setGlobalShader(Shader);
void setScaleMode(ScaleMode);
void setVoidColor(int, int, int);

typedef size_t menuId;
typedef size_t buttonId;
#ifdef _WIN32
menuId addMenuTo(menuId, const wchar_t*, bool);
buttonId addButtonTo(menuId, const wchar_t*, void (*callback)());
void checkRadioButton(buttonId); 
#else
#define addMenuTo(a, b, c) 0
#define addButtonTo(a, b, c) 0 
#define checkRadioButton(a); 
#define getAbsoluteDir(a);
#endif

#endif