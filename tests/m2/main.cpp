#include "lab.hpp"

#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

int main(int, char**)
{
    SDL_SetAppMetadata("Forge M2", FORGE_VERSION, "dev.forge.engine");
    forge::labs::PbrHelmetLab lab;
    return forge::app::run_lab(lab, "Forge M2");
}
