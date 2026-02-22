#include <SDL3/SDL.h>
#include <iostream>

int main() {
    std::cout << "Testing SDL initialization..." << std::endl;

    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        std::cerr << "SDL_Init failed: " << SDL_GetError() << std::endl;
        return -1;
    }

    std::cout << "SDL initialized successfully!" << std::endl;
    std::cout << "SDL version: " << SDL_GetVersion() << std::endl;

    SDL_Window* window = SDL_CreateWindow("Test Window", 800, 600, 0);
    if (!window) {
        std::cerr << "Window creation failed: " << SDL_GetError() << std::endl;
        SDL_Quit();
        return -1;
    }

    std::cout << "Window created successfully!" << std::endl;

    SDL_Delay(2000); // Show window for 2 seconds

    SDL_DestroyWindow(window);
    SDL_Quit();

    std::cout << "Test completed successfully!" << std::endl;
    return 0;
}