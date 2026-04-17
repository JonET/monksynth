#pragma once

namespace MonkSynth {
namespace i18n {

enum class StringId : int {
    SetupNeedsTheme,
    SetupImportFromClassic1,
    SetupImportFromClassic2,
    SetupDownloadFrom,
    SetupThenClick1,
    SetupThenClick2,
    SetupImportButton,

    InfoCreatedBy,
    InfoLicenseHeader,
    InfoSourceCodeHeader,
    InfoTagline1,
    InfoTagline2,
    InfoClose,

    ContributeHeader,
    ContributeShare,
    ContributeLookingFor,
    ContributeOpenFolder,

    MenuThemePrefix,
    MenuLoadTheme,
    MenuImportClassic,
    MenuOpenFolder,
    MenuLanguage,
    MenuLanguageAuto,
    MenuPitchBend,
    MenuPitchBendClassic,
    MenuPitchBendPitch,
    MenuPitchBendBoth,
    MenuPitchBendBothInverted,
    MenuAdvanced,
    FileSelectThemeJson,
    FileSelectDll,
    FileExtJson,
    FileExtDll,

    ErrCannotOpen,
    ErrUnexpectedSize,
    ErrReadFailed,
    ErrChecksumMismatch,
    ErrCreateDir,
    MsgImportedCount,
    ErrExtractFailed,

    Count // sentinel, keep last
};

enum class Language {
    English,
    Japanese,
    Korean,
};

// Detects UI language from the OS. Returns English for unknown locales.
Language detectSystemLanguage();

// Sets the active language. Call after reading the override from config.json,
// or pass detectSystemLanguage() for auto.
void setLanguage(Language lang);

Language currentLanguage();

// Returns the localized string for |id|. Falls back to English if the current
// language's table has an empty entry. Never returns null.
const char *str(StringId id);

// Returns the UI font name for the current language. Picks CJK-capable fonts
// for Japanese/Korean; defaults to the system sans-serif elsewhere.
const char *uiFont();

// Parses a POSIX/Windows locale string (e.g. "ja_JP.UTF-8", "ja-JP", "ja")
// to a Language. Matches on the first two characters, case-insensitively.
Language parseLocale(const char *locale);

} // namespace i18n
} // namespace MonkSynth
