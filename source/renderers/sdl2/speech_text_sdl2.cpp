#include "speech_text_sdl2.hpp"
#include "text.hpp"
#include <SDL.h>
#include <SDL_ttf.h>
#include <log.hpp>
#include <os.hpp>

#ifdef USE_CMAKERC
#include <cmrc/cmrc.hpp>

CMRC_DECLARE(romfs);
#endif

SpeechTextObjectSDL2::SpeechTextObjectSDL2(const std::string &text, int maxWidth, const std::string &fontPath, int fontSize)
    : TextObjectSDL2(text, 0, 0), SpeechText(text, maxWidth) {
    setColor(0x00);
    setCenterAligned(false); // easier for positioning logic

    if (font && !pathFont.empty()) {
        TextObjectSDL2::fontUsageCount[pathFont]--;
        if (TextObjectSDL2::fontUsageCount[pathFont] <= 0) {
            TTF_CloseFont(TextObjectSDL2::fonts[pathFont]);
            TextObjectSDL2::fonts.erase(pathFont);
            TextObjectSDL2::fontUsageCount.erase(pathFont);
        }
        font = nullptr;
        pathFont.clear();
    }

    std::string resolvedFontPath = fontPath.empty() ? "gfx/ingame/fonts/NotoSans-Medium" : fontPath;
    resolvedFontPath = OS::getRomFSLocation() + resolvedFontPath + ".ttf";

#ifdef USE_CMAKERC
    const auto &file = cmrc::romfs::get_filesystem().open(resolvedFontPath);
    font = TTF_OpenFontRW(SDL_RWFromConstMem(file.begin(), file.size()), 1, fontSize);
#else
    font = TTF_OpenFont(resolvedFontPath.c_str(), fontSize);
#endif
    if (!font) {
        Log::logError("Failed to load speech font " + resolvedFontPath + ": " + TTF_GetError());
    }

    platformSetText(wrapText());
}

SpeechTextObjectSDL2::~SpeechTextObjectSDL2() {
    if (font) {
        TTF_CloseFont(font);
        font = nullptr;
    }
}

float SpeechTextObjectSDL2::measureTextWidth(const std::string &text) {
    if (!font) return 0.0f;

    int width, height;
    TTF_SizeUTF8(font, text.c_str(), &width, &height);
    return static_cast<float>(width);
}

void SpeechTextObjectSDL2::platformSetText(const std::string &text) {
    TextObjectSDL2::setText(text);
}

void SpeechTextObjectSDL2::setText(std::string txt) {
    SpeechText::setText(txt);
}
