extern "C" {
#include <SDL.h>
// Some SDL builds (yawut port) may not provide SDL_RWclose symbol; provide
// a small compatibility wrapper mapping to SDL_FreeRW.
void SDL_RWclose(SDL_RWops *context)
{
    SDL_FreeRW(context);
}
}
