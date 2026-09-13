#include "lab.hpp"

#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

int main(int, char**)
{
    SDL_SetAppMetadata("Forge M3", FORGE_VERSION, "dev.forge.engine");
    forge::labs::ShadowWalkLab lab;
    return forge::app::run_lab(lab, "Forge M3");
}
