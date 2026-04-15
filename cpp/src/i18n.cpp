#include "i18n.h"
#include "strings_en.h"
#include "strings_ja.h"
#include "strings_ko.h"

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#elif defined(__APPLE__)
#include <CoreFoundation/CoreFoundation.h>
#else
#include <cstdlib>
#endif

namespace MonkSynth {
namespace i18n {

namespace {

Language g_language = Language::English;

const char *const *tableForLanguage(Language lang) {
    switch (lang) {
        case Language::Japanese: return kStringsJa;
        case Language::Korean: return kStringsKo;
        case Language::English:
        default: return kStringsEn;
    }
}

} // namespace

Language parseLocale(const char *locale) {
    if (!locale || !locale[0] || !locale[1])
        return Language::English;
    char a = locale[0];
    char b = locale[1];
    if (a >= 'A' && a <= 'Z')
        a = static_cast<char>(a - 'A' + 'a');
    if (b >= 'A' && b <= 'Z')
        b = static_cast<char>(b - 'A' + 'a');
    if (a == 'j' && b == 'a')
        return Language::Japanese;
    if (a == 'k' && b == 'o')
        return Language::Korean;
    return Language::English;
}

Language detectSystemLanguage() {
#ifdef _WIN32
    wchar_t buf[LOCALE_NAME_MAX_LENGTH] = {};
    if (GetUserDefaultLocaleName(buf, LOCALE_NAME_MAX_LENGTH) > 0) {
        char narrow[8] = {};
        for (int i = 0; i < 7 && buf[i]; ++i)
            narrow[i] = static_cast<char>(buf[i]);
        return parseLocale(narrow);
    }
    return Language::English;
#elif defined(__APPLE__)
    CFLocaleRef locale = CFLocaleCopyCurrent();
    if (!locale)
        return Language::English;
    auto langCode =
        static_cast<CFStringRef>(CFLocaleGetValue(locale, kCFLocaleLanguageCode));
    char buf[8] = {};
    Language result = Language::English;
    if (langCode && CFStringGetCString(langCode, buf, sizeof(buf), kCFStringEncodingUTF8))
        result = parseLocale(buf);
    CFRelease(locale);
    return result;
#else
    const char *lang = std::getenv("LC_ALL");
    if (!lang || !*lang)
        lang = std::getenv("LC_MESSAGES");
    if (!lang || !*lang)
        lang = std::getenv("LANG");
    return parseLocale(lang);
#endif
}

void setLanguage(Language lang) { g_language = lang; }

Language currentLanguage() { return g_language; }

const char *str(StringId id) {
    int idx = static_cast<int>(id);
    if (idx < 0 || idx >= static_cast<int>(StringId::Count))
        return "";
    const char *const *table = tableForLanguage(g_language);
    const char *s = table[idx];
    if (!s || !*s)
        s = kStringsEn[idx];
    return s ? s : "";
}

const char *uiFont() {
    switch (g_language) {
        case Language::Japanese:
#ifdef _WIN32
            return "Yu Gothic UI";
#elif defined(__APPLE__)
            return "Hiragino Sans";
#else
            return "Noto Sans CJK JP";
#endif
        case Language::Korean:
#ifdef _WIN32
            return "Malgun Gothic";
#elif defined(__APPLE__)
            return "Apple SD Gothic Neo";
#else
            return "Noto Sans CJK KR";
#endif
        case Language::English:
        default:
#ifdef __linux__
            return "DejaVu Sans";
#elif defined(__APPLE__)
            return "Helvetica";
#else
            return "Arial";
#endif
    }
}

} // namespace i18n
} // namespace MonkSynth
