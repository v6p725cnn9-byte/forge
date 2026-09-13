#include "lab.hpp"

#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

int main(int, char**)
{
    SDL_SetAppMetadata("Forge Survival", FORGE_VERSION, "dev.forge.engine");
    return forge::app::run_game("Forge");
}
