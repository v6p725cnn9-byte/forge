#include "lab.hpp"

#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

int main(int, char**)
{
    SDL_SetAppMetadata("Forge M4", FORGE_VERSION, "dev.forge.engine");
    forge::labs::StreamCityLab lab;
    return forge::app::run_lab(lab, "Forge M4");
}
