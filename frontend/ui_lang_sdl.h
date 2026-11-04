#pragma once
#include <SDL.h>
#include "ui_lang.h"
#ifdef __SWITCH__
#include "switch_platform.h"
#endif

namespace UiLang
{
inline void DetectSystem()
{
#ifdef __SWITCH__
    if (const char* lang = SwitchPlatform::SystemLanguage())
    {
        SetSystem(lang);
        return;
    }
#endif
    if (SDL_Locale* locales = SDL_GetPreferredLocales())
    {
        SetSystem(locales[0].language ? locales[0].language : "en");
        SDL_free(locales);
    }
}
}
