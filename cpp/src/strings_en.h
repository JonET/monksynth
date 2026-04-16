#pragma once

#include "i18n.h"

namespace MonkSynth {
namespace i18n {

// English is the source of truth. When user-facing text changes, update this
// table first; other languages fall back to English for any empty entry.
// Keep the order matching the StringId enum in i18n.h.
constexpr const char *kStringsEn[] = {
    // SetupNeedsSkin
    "MonkSynth needs a skin to display its GUI.",
    // SetupImportFromClassic1
    "You can import the classic look from the",
    // SetupImportFromClassic2
    "original Delay Lama v1.1 plugin (Windows).",
    // SetupDownloadFrom
    "Download it for free from:",
    // SetupThenClick1
    "Then click below and select the",
    // SetupThenClick2
    "\"Delay Lama.dll\" file.",
    // SetupImportButton
    "Import Classic Skin...",

    // InfoCreatedBy
    "Created by Jonathan Taylor",
    // InfoLicenseHeader
    "License",
    // InfoSourceCodeHeader
    "Source Code",
    // InfoTagline1
    "A vocal synthesizer inspired by Delay Lama.",
    // InfoTagline2
    "Open source and free forever.",
    // InfoClose
    "Close",

    // ContributeHeader
    "Create your own theme!",
    // ContributeShare
    "Share it with the community",
    // ContributeLookingFor
    "We're looking for fresh default themes",
    // ContributeOpenFolder
    "Open themes folder",

    // MenuThemePrefix
    "Theme: ",
    // MenuLoadTheme
    "Load Theme...",
    // MenuImportClassic
    "Import Classic Skin from DLL...",
    // MenuOpenFolder
    "Open Themes Folder",
    // MenuLanguage
    "Language",
    // MenuLanguageAuto
    "Auto",
    // MenuPitchBend
    "Pitch Bend",
    // MenuPitchBendClassic
    "Classic (Vowel)",
    // MenuPitchBendPitch
    "Pitch",
    // MenuPitchBendBoth
    "Both (Pitch + Vowel)",
    // MenuPitchBendBothInverted
    "Both (Pitch + Inverted Vowel)",
    // FileSelectThemeJson
    "Select theme.json in Theme Folder",
    // FileSelectDll
    "Select Delay Lama DLL",
    // FileExtJson
    "JSON Files",
    // FileExtDll
    "DLL Files",

    // ErrCannotOpen
    "Could not open the selected file.",
    // ErrUnexpectedSize
    "This doesn't appear to be the original Delay Lama DLL "
    "(unexpected file size).",
    // ErrReadFailed
    "Failed to read the file.",
    // ErrChecksumMismatch
    "This doesn't appear to be the original Delay Lama DLL "
    "(checksum mismatch).",
    // ErrCreateDir
    "Could not create theme directory: ",
    // MsgImportedCount
    "Imported %d/%d assets.",
    // ErrExtractFailed
    "Failed to extract resources from the DLL.",
};

static_assert(sizeof(kStringsEn) / sizeof(kStringsEn[0]) ==
                  static_cast<int>(StringId::Count),
              "kStringsEn must have one entry per StringId");

} // namespace i18n
} // namespace MonkSynth
