#include <SDL3/SDL.h>
#include <iostream>
int main() { if (SDL_Init(SDL_INIT_VIDEO) != 0) { std::cout << "SDL Error: " << SDL_GetError() << std::endl; return 1; } std::cout << "SDL OK" << std::endl; SDL_Quit(); return 0; }
