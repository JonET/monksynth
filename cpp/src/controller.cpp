#include "controller.h"
#include "pluginterfaces/base/ibstream.h"
#include "dll_extractor.h"
#include "i18n.h"
#include "info_button.h"
#include "info_view.h"
#include "monk_view.h"
#include "overlay_view.h"
#include "open_url.h"
#include "plugin_cids.h"
#include "setup_view.h"
#include "theme_info_view.h"
#include "xy_pad.h"

#include "public.sdk/source/vst/vstparameters.h"
#include "vstgui/lib/cbitmap.h"
#include "vstgui/lib/cfileselector.h"
#include "vstgui/lib/cframe.h"
#include "vstgui/lib/controls/coptionmenu.h"
#include "vstgui/lib/cvstguitimer.h"
#include "vstgui/lib/platform/platformfactory.h"
#include "vstgui/uidescription/uidescription.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <optional>
#include <stdexcept>

using namespace Steinberg;
using namespace Steinberg::Vst;
using namespace VSTGUI;
namespace fs = std::filesystem;

namespace MonkSynth {

Controller::DllKey Controller::DllKey::of(const fs::path &p) {
    std::error_code ec;
    DllKey k;
    k.path = p;
    k.size = fs::file_size(p, ec);
    if (ec)
        k.size = 0;
    k.mtime = fs::last_write_time(p, ec);
    if (ec)
        k.mtime = {};
    return k;
}

// A file that already failed to import is skipped until it changes, so the
// Import button falls through to the file picker and the folder watch
// doesn't retry it every second.
std::optional<fs::path> Controller::findFolderDll() const {
    std::error_code ec;
    for (const fs::path &dir : {ThemeManager::getThemesDir(), ThemeManager::getConfigDir()}) {
        fs::directory_iterator it(dir, ec);
        if (ec)
            continue;
        for (const auto &entry : it) {
            if (!entry.is_regular_file(ec) || ec)
                continue;
            std::string ext = entry.path().extension().string();
            std::transform(ext.begin(), ext.end(), ext.begin(),
                           [](unsigned char c) { return std::tolower(c); });
            if (ext != ".dll")
                continue;
            if (failedDll_ && *failedDll_ == DllKey::of(entry.path()))
                continue;
            return entry.path();
        }
    }
    return std::nullopt;
}

// VSTGUI file selectors return UTF-8 strings.  On Windows, the fs::path(const
// char*) constructor interprets narrow strings using the active ANSI code page,
// which mangles paths containing characters outside that code page (e.g.
// Japanese characters on a non-Japanese locale).  std::filesystem::u8path()
// handles this correctly on all platforms.
static std::filesystem::path pathFromUTF8(const char *utf8) {
    if (!utf8)
        return {};
    return std::filesystem::u8path(utf8);
}

// Resolve a persisted language preference ("auto" | "en" | "ja" | "ko" | "")
// into an active i18n language and install it globally.
static void applyLanguagePreference(const std::string &pref) {
    if (pref == "en")
        i18n::setLanguage(i18n::Language::English);
    else if (pref == "ja")
        i18n::setLanguage(i18n::Language::Japanese);
    else if (pref == "ko")
        i18n::setLanguage(i18n::Language::Korean);
    else
        i18n::setLanguage(i18n::detectSystemLanguage());
}

IPlugView *PLUGIN_API Controller::createView(const char *name) {
    if (FIDStringsEqual(name, ViewType::kEditor)) {
        return new ThemedVST3Editor(this, "view", "editor.uidesc", &themeManager_);
    }
    return nullptr;
}

CView *Controller::createCustomView(UTF8StringPtr name, const UIAttributes & /*attributes*/,
                                    const IUIDescription *description, VST3Editor * /*editor*/) {
    if (UTF8StringView(name) == "MonkAnimation") {
        auto *view = new MonkView(CRect(0, 0, 311, 311));
        CBitmap *bmp = description->getBitmap("monk_strip");
        if (bmp) {
            view->setMonkBitmap(bmp);
        }
        view->setVowelValue(static_cast<float>(getParamNormalized(kVowel)));
        // kNoteActive is only pushed on edges, so sync on create in case a
        // note was already held when the editor opened.
        view->setNoteActive(getParamNormalized(kNoteActive) > 0.5);
        monkView_ = view;
        return view;
    }
    if (UTF8StringView(name) == "XYPad") {
        return new XYPadView(CRect(0, 0, 100, 100), nullptr, this);
    }
    if (UTF8StringView(name) == "InfoButton") {
        auto *btn = new InfoButton(CRect(0, 0, 25, 25));
        btn->setClickCallback([this]() {
            if (currentEditor_)
                showInfoOverlay(currentEditor_);
        });
        infoButton_ = btn;
        return btn;
    }
    return nullptr;
}

void Controller::didOpen(VST3Editor *editor) {
    currentEditor_ = editor;
    // Re-apply language preference in case the user changed their OS locale
    // since initialize() was called (e.g. relaunched the host in a different
    // language).
    applyLanguagePreference(themeManager_.languagePref());
    // Theme bitmaps are applied in ThemedVST3Editor::open() before views are
    // created, so CAnimKnob sees the real bitmap dimensions at init time.
    // We only need to show the setup overlay when no theme is available.
    if (!themeManager_.hasTheme()) {
        showSetupOverlay(editor);
    }

    // Info button callback is set in createCustomView using currentEditor_
}

void Controller::importClassicFromDll(ThemedVST3Editor *editor, SetupView *setup,
                                      const fs::path &dllPath) {
    if (editor != currentEditor_)
        return;
    // A file panel or deferred call can outlive the setup screen it was
    // started from (close and reopen the editor while the panel is up).
    // Callers hold the view, so it is valid memory; just don't draw on it.
    if (setup && !setup->isAttached())
        setup = nullptr;
    try {
        auto result = extractClassicTheme(dllPath, ThemeManager::getConfigDir());
        if (result.success) {
            stopSetupDllWatch(); // the setup screen is about to be replaced
            selectTheme(result.themeDir, false);
        } else {
            failedDll_ = DllKey::of(dllPath);
            if (setup)
                setup->setStatusText(result.error);
        }
    } catch (const std::exception &e) {
        failedDll_ = DllKey::of(dllPath);
        if (setup)
            setup->setStatusText(std::string("Error: ") + e.what());
    } catch (...) {
        failedDll_ = DllKey::of(dllPath);
        if (setup)
            setup->setStatusText("An unexpected error occurred.");
    }
}

void Controller::showSetupOverlay(VST3Editor *editor) {
    auto *frame = editor->getFrame();
    if (!frame)
        return;

    auto *setup = new SetupView(CRect(0, 0, 360, 510));
    auto *themedEditor = static_cast<ThemedVST3Editor *>(editor);

    // Offer themes shipped inside the bundle as a one-click alternative.
    std::vector<ThemeManager::InstalledTheme> bundled;
    for (auto &t : ThemeManager::listInstalledThemes())
        if (t.bundled)
            bundled.push_back(t);
    setup->setBuiltInThemes(std::move(bundled));
    setup->setBuiltInThemeCallback(
        [this](const fs::path &themeDir) { selectTheme(themeDir, true); });

    // Called from inside the OS drop callback: defer the extraction (a few
    // MB of synchronous work) so the drag animation can finish first. The
    // callback lives in the view, so it holds only a raw pointer to it; the
    // deferred closure retains the view.
    setup->setDllDropCallback([this, setup, themedEditor](const fs::path &dllPath) {
        deferUI([this, setup = SharedPointer<SetupView>(setup), themedEditor, dllPath]() {
            importClassicFromDll(themedEditor, setup, dllPath);
        });
    });

    setup->setImportCallback([this, frame, setup, themedEditor]() {
        if (auto dll = findFolderDll()) {
            importClassicFromDll(themedEditor, setup, *dll);
            return;
        }

        auto *selector = CNewFileSelector::create(frame, CNewFileSelector::kSelectFile);
        if (!selector)
            return;

        selector->setTitle(i18n::str(i18n::StringId::FileSelectDll));
        selector->addFileExtension(
            CFileExtension(i18n::str(i18n::StringId::FileExtDll), "dll"));

        // The panel is asynchronous on macOS: hold the view so the status
        // text target is at least valid memory if the editor was closed and
        // reopened meanwhile (importClassicFromDll checks isAttached).
        selector->run([this, setup = SharedPointer<SetupView>(setup),
                       themedEditor](CNewFileSelector *sel) {
            if (sel->getNumSelectedFiles() == 0)
                return;
            importClassicFromDll(themedEditor, setup, pathFromUTF8(sel->getSelectedFile(0)));
        });
        selector->forget();
    });

    frame->addView(setup);

    // Import a DLL from the folder without another click: immediately if
    // one is already there (the user followed the copy-it-there instruction
    // before reopening the plugin), or as soon as one appears while the
    // setup screen is up. A successful import stops the watch (the screen
    // is replaced). A failed one shows its error and the file is skipped
    // until it changes, so the watch keeps running for a corrected copy.
    // The closure retains the view: a late run after the frame is gone at
    // most sets text on a detached view.
    auto folderImport = [this, setup = SharedPointer<SetupView>(setup), themedEditor]() {
        if (!setupDllWatch_)
            return; // setup screen gone, or an import already succeeded
        if (auto dll = findFolderDll())
            importClassicFromDll(themedEditor, setup, *dll);
    };
    // A file the watch sees may still be mid-copy: import only once it has
    // the same size and mtime on two consecutive ticks. The import runs
    // deferred rather than inside the timer callback.
    setupDllWatch_ = VSTGUI::makeOwned<VSTGUI::CVSTGUITimer>(
        [this, folderImport, seen = std::optional<DllKey>{}](VSTGUI::CVSTGUITimer *) mutable {
            auto dll = findFolderDll();
            if (!dll) {
                seen.reset();
                return;
            }
            auto key = DllKey::of(*dll);
            if (seen && *seen == key) {
                seen.reset();
                deferUI(folderImport);
            } else {
                seen = key;
            }
        },
        1000);
    if (findFolderDll())
        deferUI(folderImport);
}

void Controller::presentOverlay(VST3Editor *editor, OverlayView *view) {
    // Refuse if the editor is gone or an overlay is already up (right-click
    // passes through overlays to the context menu, so About Theme... could
    // otherwise stack). Identity is checked before the editor is touched.
    auto *frame = (editor && editor == currentEditor_) ? editor->getFrame() : nullptr;
    if (!frame || (overlay_ && frame->isChild(overlay_))) {
        view->forget();
        return;
    }
    overlay_ = view;
    view->setCloseCallback([this, frame, view]() {
        // Runs from the overlay's own click handler; removing it there would
        // free the view mid-dispatch. The deferral is cancelled if the
        // editor closes first, and recreateUI may already have removed it.
        deferUI([this, frame, view]() {
            // overlay_ is cleared by rebuildEditorForTheme, so this also
            // covers a rebuild in the window that reused the view's address.
            if (overlay_ != view)
                return;
            overlay_ = nullptr;
            if (frame->isChild(view))
                frame->removeView(view);
        });
    });
    frame->addView(view);
}

void Controller::showInfoOverlay(VST3Editor *editor) {
    presentOverlay(editor, new InfoView(CRect(0, 0, 360, 510)));
}

void Controller::showThemeInfoOverlay(VST3Editor *editor) {
    presentOverlay(editor, new ThemeInfoView(CRect(0, 0, 360, 510), themeManager_.getThemeInfo()));
}

void Controller::stopSetupDllWatch() {
    if (setupDllWatch_) {
        setupDllWatch_->stop();
        setupDllWatch_ = nullptr;
    }
}

void Controller::willClose(VST3Editor * /*editor*/) {
    cancelDeferredUI();
    stopSetupDllWatch();
    finishPitchBendSpring();
    monkView_ = nullptr;
    infoButton_ = nullptr;
    currentEditor_ = nullptr;
    overlay_ = nullptr;
}

void Controller::applyTheme(VST3Editor *editor) {
    if (!themeManager_.hasTheme())
        return;

    auto *desc = editor->getUIDescription();
    if (!desc)
        return;

    for (auto &[name, filename] : ThemeManager::bitmapFileMap()) {
        auto path = themeManager_.resolveThemeBitmap(name);
        if (!path)
            continue;

        auto platformBmp =
            getPlatformFactory().createBitmapFromPath(path->generic_u8string().c_str());
        if (!platformBmp)
            continue;

        CBitmap *bmp = desc->getBitmap(name.c_str());
        if (bmp)
            bmp->setPlatformBitmap(platformBmp);
    }
}

void Controller::selectTheme(const std::filesystem::path &themeDir, bool bundled) {
    // The folder may have been deleted since the menu was built.
    std::error_code ec;
    if (!fs::is_directory(themeDir, ec))
        return;
    themeManager_.setThemePath(themeDir, bundled);
    deferUI([this]() { rebuildEditorForTheme(); });
}

void Controller::rebuildEditorForTheme() {
    auto *editor = static_cast<ThemedVST3Editor *>(currentEditor_);
    if (!editor)
        return;
    overlay_ = nullptr; // recreateUI destroys it
    stopSetupDllWatch(); // and the setup screen with it
    auto *desc = editor->getUIDescription();
    if (desc)
        desc->freePlatformResources();
    applyTheme(editor);
    editor->recreateUI();
}

void Controller::deferUI(std::function<void()> fn) {
    auto timer = VSTGUI::makeOwned<VSTGUI::CVSTGUITimer>(
        [this, fn = std::move(fn)](VSTGUI::CVSTGUITimer *t) {
            t->stop();
            // CVSTGUITimer::fire holds a guard on the timer, so dropping our
            // reference from inside the callback is safe.
            deferredUI_.erase(std::remove_if(deferredUI_.begin(), deferredUI_.end(),
                                             [t](const auto &p) { return p.get() == t; }),
                              deferredUI_.end());
            fn();
        },
        10);
    deferredUI_.push_back(timer);
}

void Controller::cancelDeferredUI() {
    for (auto &t : deferredUI_)
        t->stop();
    deferredUI_.clear();
}

tresult PLUGIN_API Controller::terminate() {
    cancelDeferredUI();
    stopSetupDllWatch();
    pitchBendSpringTimer_ = nullptr;
    pbSpringState_ = PbSpring::Idle;
    return EditController::terminate();
}

COptionMenu *Controller::createContextMenu(const CPoint & /*pos*/, VST3Editor *editor) {
    auto *menu = new COptionMenu();
    auto *themedEditor = static_cast<ThemedVST3Editor *>(editor);

    // ---- Theme switcher submenu ----
    // Labelled "Theme: <current>" so the active theme is visible without
    // opening it (kChecked alone isn't rendered reliably by all hosts).
    // Lists every theme folder under the user themes dir; if the active
    // theme was loaded from somewhere else it's appended so it still shows
    // as checked.
    std::string themeLabel =
        std::string(i18n::str(i18n::StringId::MenuThemePrefix)) + themeManager_.getThemeName();
    auto installed = ThemeManager::listInstalledThemes();
    auto isActive = [this](const fs::path &p) {
        std::error_code ec;
        return themeManager_.hasTheme() && fs::equivalent(p, themeManager_.themePath(), ec);
    };
    bool activeListed = std::any_of(installed.begin(), installed.end(),
                                    [&](const auto &t) { return isActive(t.path); });
    if (themeManager_.hasTheme() && !activeListed)
        installed.push_back({themeManager_.getThemeName(), themeManager_.themePath(), false});

    if (installed.empty()) {
        // Nothing to switch between; fall back to a disabled header.
        CCommandMenuItem::Desc headerDesc{UTF8String(themeLabel)};
        headerDesc.flags = CMenuItem::kDisabled;
        menu->addEntry(new CCommandMenuItem(std::move(headerDesc)));
    } else {
        auto *themeMenu = new COptionMenu();
        for (const auto &t : installed) {
            // A user copy of a bundled theme has the same display name, so
            // mark the read-only one to tell them apart.
            std::string label = t.name;
            if (t.bundled)
                label += i18n::str(i18n::StringId::MenuBuiltInSuffix);
            CCommandMenuItem::Desc desc{UTF8String(label)};
            // The active theme is shown checked and disabled: re-selecting
            // it would only rebuild the UI for nothing.
            if (isActive(t.path))
                desc.flags |= CMenuItem::kChecked | CMenuItem::kDisabled;
            auto *item = new CCommandMenuItem(std::move(desc));
            fs::path themeDir = t.path;
            bool bundled = t.bundled;
            item->setActions([this, themeDir, bundled](CCommandMenuItem *) {
                // selectTheme defers the rebuild: doing it while the menu's
                // tracking loop is still running tears down the menu's parent.
                selectTheme(themeDir, bundled);
            });
            themeMenu->addEntry(item);
        }
        // addEntry takes its own reference to the submenu; drop ours.
        menu->addEntry(themeMenu, UTF8String(themeLabel));
        themeMenu->forget();
    }

    // "About Theme..." — credits and link from the active theme's theme.json.
    if (themeManager_.hasTheme()) {
        auto *aboutItem = new CCommandMenuItem(
            CCommandMenuItem::Desc(i18n::str(i18n::StringId::MenuAboutTheme)));
        aboutItem->setActions([this](CCommandMenuItem *) {
            deferUI([this]() {
                if (currentEditor_)
                    showThemeInfoOverlay(currentEditor_);
            });
        });
        menu->addEntry(aboutItem);
    }

    menu->addSeparator();

    // "Load Theme..." item.
    auto *loadItem =
        new CCommandMenuItem(CCommandMenuItem::Desc(i18n::str(i18n::StringId::MenuLoadTheme)));
    loadItem->setActions([this](CCommandMenuItem *) {
        // Defer file dialog opening until after the context menu's tracking
        // loop has ended.  On macOS, opening NSOpenPanel while the NSMenu is
        // still active crashes the host (e.g. Ableton Live).
        deferUI([this]() {
            auto *frame = currentEditor_ ? currentEditor_->getFrame() : nullptr;
            if (!frame)
                return;

            // Use kSelectFile (select theme.json) instead of kSelectDirectory.
            // macOS NSOpenPanel directory selection doesn't work reliably as a
            // sheet in plugin hosts.
            auto *selector = CNewFileSelector::create(frame, CNewFileSelector::kSelectFile);
            if (!selector)
                return;

            selector->setTitle(i18n::str(i18n::StringId::FileSelectThemeJson));
            selector->addFileExtension(
                CFileExtension(i18n::str(i18n::StringId::FileExtJson), "json"));
            if (themeManager_.hasTheme())
                selector->setInitialDirectory(
                    UTF8String(themeManager_.themePath().generic_u8string()));

            selector->run([this](CNewFileSelector *sel) {
                try {
                    if (sel->getNumSelectedFiles() > 0) {
                        auto file = pathFromUTF8(sel->getSelectedFile(0));
                        selectTheme(file.parent_path(), false);
                    }
                } catch (...) {
                }
            });
            selector->forget();
        });
    });
    menu->addEntry(loadItem);

    // "Open Themes Folder" item. Reveals the user config themes directory
    // so people can drop a new theme folder in or grab an existing one to
    // share.
    auto *openFolderItem =
        new CCommandMenuItem(CCommandMenuItem::Desc(i18n::str(i18n::StringId::MenuOpenFolder)));
    openFolderItem->setActions([](CCommandMenuItem *) {
        openFolder(ThemeManager::getThemesDir());
    });
    menu->addEntry(openFolderItem);

    // "Import Classic Theme from DLL..." item.
    auto *importItem =
        new CCommandMenuItem(CCommandMenuItem::Desc(i18n::str(i18n::StringId::MenuImportClassic)));
    importItem->setActions([this](CCommandMenuItem *) {
        deferUI([this]() {
            auto *editor = static_cast<ThemedVST3Editor *>(currentEditor_);
            if (!editor)
                return;
            if (auto dll = findFolderDll()) {
                importClassicFromDll(editor, nullptr, *dll);
                return;
            }

            auto *frame = editor->getFrame();
            if (!frame)
                return;

            auto *selector = CNewFileSelector::create(frame, CNewFileSelector::kSelectFile);
            if (!selector)
                return;

            selector->setTitle(i18n::str(i18n::StringId::FileSelectDll));
            selector->addFileExtension(
                CFileExtension(i18n::str(i18n::StringId::FileExtDll), "dll"));

            selector->run([this](CNewFileSelector *sel) {
                if (sel->getNumSelectedFiles() == 0)
                    return;
                auto *editor = static_cast<ThemedVST3Editor *>(currentEditor_);
                if (editor)
                    importClassicFromDll(editor, nullptr, pathFromUTF8(sel->getSelectedFile(0)));
            });
            selector->forget();
        });
    });
    menu->addEntry(importItem);

    // ---- Language submenu ----
    menu->addSeparator();

    auto *langMenu = new COptionMenu();
    const std::string currentPref =
        themeManager_.languagePref().empty() ? "auto" : themeManager_.languagePref();

    struct LangOption {
        const char *label;
        const char *pref;
    };
    // "English", "日本語", "한국어" are displayed in their own scripts so a
    // user who accidentally flipped to a script they can't read still has an
    // obvious escape hatch. They intentionally are NOT routed through i18n.
    const LangOption opts[] = {
        {i18n::str(i18n::StringId::MenuLanguageAuto), "auto"},
        {"English", "en"},
        {"\xE6\x97\xA5\xE6\x9C\xAC\xE8\xAA\x9E", "ja"},
        {"\xED\x95\x9C\xEA\xB5\xAD\xEC\x96\xB4", "ko"},
    };

    for (const auto &opt : opts) {
        CCommandMenuItem::Desc desc(opt.label);
        if (currentPref == opt.pref)
            desc.flags |= CMenuItem::kChecked;
        auto *langItem = new CCommandMenuItem(std::move(desc));
        std::string pref = opt.pref;
        langItem->setActions([this, themedEditor, pref](CCommandMenuItem *) {
            themeManager_.setLanguagePref(pref);
            applyLanguagePreference(pref);
            auto *frame = themedEditor->getFrame();
            if (frame)
                frame->invalid();
        });
        langMenu->addEntry(langItem);
    }
    menu->addEntry(langMenu, i18n::str(i18n::StringId::MenuLanguage));
    langMenu->forget();

    // ---- Pitch Bend routing submenu ----
    auto *pbMenu = new COptionMenu();
    const auto currentMode = pitchBendModeFromNormalized(
        static_cast<float>(getParamNormalized(kPitchBendRouting)));

    struct PbOption {
        i18n::StringId labelId;
        PitchBendMode mode;
    };
    const PbOption pbOpts[] = {
        {i18n::StringId::MenuPitchBendClassic, PitchBendMode::Classic},
        {i18n::StringId::MenuPitchBendPitch, PitchBendMode::Pitch},
        {i18n::StringId::MenuPitchBendBoth, PitchBendMode::Both},
        {i18n::StringId::MenuPitchBendBothInverted, PitchBendMode::BothInverted},
    };

    for (const auto &opt : pbOpts) {
        CCommandMenuItem::Desc desc(i18n::str(opt.labelId));
        if (currentMode == opt.mode)
            desc.flags |= CMenuItem::kChecked;
        auto *pbItem = new CCommandMenuItem(std::move(desc));
        PitchBendMode target = opt.mode;
        pbItem->setActions([this, target](CCommandMenuItem *) {
            // Push the value through beginEdit/performEdit/endEdit so the
            // host records the change and forwards it to the processor;
            // setParamNormalized separately updates our own state and fires
            // the restartComponent(kMidiCCAssignmentChanged) notification.
            ParamValue v = pitchBendModeToNormalized(target);
            beginEdit(kPitchBendRouting);
            performEdit(kPitchBendRouting, v);
            endEdit(kPitchBendRouting);
            setParamNormalized(kPitchBendRouting, v);
        });
        pbMenu->addEntry(pbItem);
    }
    // Surface the active mode in the parent menu label so users see the
    // current state without opening the submenu. The kChecked flag on the
    // submenu items alone is not reliably rendered by all hosts.
    i18n::StringId currentLabel = i18n::StringId::MenuPitchBendClassic;
    switch (currentMode) {
        case PitchBendMode::Classic:
            currentLabel = i18n::StringId::MenuPitchBendClassic;
            break;
        case PitchBendMode::Both:
            currentLabel = i18n::StringId::MenuPitchBendBoth;
            break;
        case PitchBendMode::BothInverted:
            currentLabel = i18n::StringId::MenuPitchBendBothInverted;
            break;
        case PitchBendMode::Pitch:
            currentLabel = i18n::StringId::MenuPitchBendPitch;
            break;
    }
    std::string pbLabel = std::string(i18n::str(i18n::StringId::MenuPitchBend)) + ": " +
                          i18n::str(currentLabel);
    menu->addEntry(pbMenu, UTF8String(pbLabel));
    pbMenu->forget();

    // No "Reset to Default" — there's no built-in theme. Users switch
    // between imported themes or re-import from the DLL.

    return menu;
}

bool Controller::isPrivateParameter(ParamID paramID) {
    return paramID == kNoteActive;
}

tresult PLUGIN_API Controller::getMidiControllerAssignment(int32 busIndex, int16 /*channel*/,
                                                           CtrlNumber midiControllerNumber,
                                                           ParamID &id) {
    if (busIndex != 0)
        return kResultFalse;

    switch (midiControllerNumber) {
        case ControllerNumbers::kPitchBend: {
            // Dynamic routing: kPitchBendRouting decides where the hardware
            // pitch wheel goes.
            //   Classic       → kVowel    (Delay Lama compat)
            //   Pitch         → kPitchBend
            //   Both / BothInv → kPitchWheelRaw (hidden hub; processor fans
            //                    out to pitch bend + vowel so the visible
            //                    kPitchBend slider and DAW automation lane
            //                    stay independent of wheel-driven vowel)
            // setParamNormalized(kPitchBendRouting) below notifies the host
            // via restartComponent(kMidiCCAssignmentChanged) when the mode
            // changes, so this function is re-queried.
            auto mode = pitchBendModeFromNormalized(
                static_cast<float>(getParamNormalized(kPitchBendRouting)));
            switch (mode) {
                case PitchBendMode::Classic:
                    id = kVowel;
                    break;
                case PitchBendMode::Pitch:
                    id = kPitchBend;
                    break;
                case PitchBendMode::Both:
                case PitchBendMode::BothInverted:
                    id = kPitchWheelRaw;
                    break;
            }
            return kResultTrue;
        }
        case ControllerNumbers::kCtrlModWheel: id = kVibrato; return kResultTrue;
        case ControllerNumbers::kCtrlPortaTime: id = kPortTime; return kResultTrue;
        case ControllerNumbers::kCtrlVolume: id = kLevel; return kResultTrue;
        case ControllerNumbers::kCtrlEffect1: id = kDelay; return kResultTrue;    // CC12
        case ControllerNumbers::kCtrlEffect2: id = kHeadSize; return kResultTrue; // CC13
        default: return kResultFalse;
    }
}

tresult PLUGIN_API Controller::setComponentState(IBStream *state) {
    // Mirrors Processor::setState: a stream saved by an older build with
    // fewer parameters stops early and leaves the rest at their defaults.
    // MemoryStream reports success with zero bytes at EOF, so the byte
    // count is what detects the short read, not the return code.
    for (int i = 0; i < kNumParams; i++) {
        float v = 0.0f;
        int32 numRead = 0;
        if (state->read(&v, sizeof(v), &numRead) != kResultOk || numRead != sizeof(v))
            break;
        if (!std::isfinite(v))
            continue;
        setParamNormalized(static_cast<ParamID>(i), static_cast<ParamValue>(v));
    }
    return kResultOk;
}

tresult Controller::beginEdit(ParamID tag) {
    if (tag == kPitchBend) {
        if (pbSpringState_ == PbSpring::Springing) {
            // User grabbed the pitch bend mid-animation. Kill the timer and
            // close the spring edit gesture before the user's new gesture
            // begins, so we don't nest edits on the same parameter.
            pitchBendSpringTimer_ = nullptr;
            pbSpringState_ = PbSpring::Idle;
            EditController::endEdit(kPitchBend);
        }
        pbSpringState_ = PbSpring::UserDragging;
    }
    return EditController::beginEdit(tag);
}

tresult Controller::endEdit(ParamID tag) {
    tresult result = EditController::endEdit(tag);
    if (tag == kPitchBend && pbSpringState_ == PbSpring::UserDragging) {
        pbSpringState_ = PbSpring::Idle;
        ParamValue current = getParamNormalized(kPitchBend);
        if (std::abs(current - 0.5) > 1e-4)
            startPitchBendSpring(current);
    }
    return result;
}

void Controller::startPitchBendSpring(double from) {
    pbSpringStart_ = from;
    pbSpringElapsedMs_ = 0.0;
    pbSpringState_ = PbSpring::Springing;
    // Bypass our override — we manage the gesture state ourselves.
    EditController::beginEdit(kPitchBend);
    pitchBendSpringTimer_ = VSTGUI::makeOwned<VSTGUI::CVSTGUITimer>(
        [this](VSTGUI::CVSTGUITimer *) { tickPitchBendSpring(); }, 16);
}

void Controller::finishPitchBendSpring() {
    if (pbSpringState_ != PbSpring::Springing)
        return;
    pitchBendSpringTimer_ = nullptr;
    pbSpringState_ = PbSpring::Idle;
    performEdit(kPitchBend, 0.5);
    setParamNormalized(kPitchBend, 0.5);
    EditController::endEdit(kPitchBend);
}

void Controller::tickPitchBendSpring() {
    if (pbSpringState_ != PbSpring::Springing)
        return;

    constexpr double kDurationMs = 180.0;
    pbSpringElapsedMs_ += 16.0;
    double t = pbSpringElapsedMs_ / kDurationMs;
    if (t > 1.0)
        t = 1.0;
    // Cubic ease-out
    double eased = 1.0 - std::pow(1.0 - t, 3.0);
    double v = pbSpringStart_ + (0.5 - pbSpringStart_) * eased;

    performEdit(kPitchBend, v);
    setParamNormalized(kPitchBend, v);

    if (t >= 1.0) {
        pitchBendSpringTimer_ = nullptr;
        pbSpringState_ = PbSpring::Idle;
        EditController::endEdit(kPitchBend);
    }
}

tresult PLUGIN_API Controller::setParamNormalized(ParamID tag, ParamValue value) {
    if (inSetParam_)
        return EditController::setParamNormalized(tag, value);
    inSetParam_ = true;
    tresult result = EditController::setParamNormalized(tag, value);
    inSetParam_ = false;
    if (result == kResultOk) {
        if (tag == kVowel) {
            if (monkView_)
                monkView_->setVowelValue(static_cast<float>(value));
        } else if (tag == kNoteActive) {
            // Drive the monk's hold-vs-idle state from the processor's
            // combined (MIDI || XY pad) signal only. Reacting to kXYNoteOn
            // directly would drop the monk to idle on pad release even when
            // a MIDI note is still held.
            if (monkView_)
                monkView_->setNoteActive(value > 0.5);
        } else if (tag == kPitchBendRouting) {
            // Tell the host to re-query IMidiMapping so the hardware pitch
            // wheel assignment flips immediately instead of waiting for a
            // plugin reload.
            if (componentHandler)
                componentHandler->restartComponent(kMidiCCAssignmentChanged);
        }
    }
    return result;
}

tresult PLUGIN_API Controller::initialize(FUnknown *context) {
    tresult result = EditController::initialize(context);
    if (result != kResultOk)
        return result;

    themeManager_.loadConfig();
    applyLanguagePreference(themeManager_.languagePref());

    parameters.addParameter(
        new RangeParameter(STR16("PortTime"), kPortTime, STR16("Hours"),
                           0.0, 1000.0, 500.0, 0, ParameterInfo::kCanAutomate));

    parameters.addParameter(
        STR16("Vowel"), STR16("Vowel"), 0, 0.5,
        ParameterInfo::kCanAutomate, kVowel);

    parameters.addParameter(
        STR16("Delay"), STR16("dB"), 0, 0.8,
        ParameterInfo::kCanAutomate, kDelay);

    parameters.addParameter(
        new RangeParameter(STR16("HeadSize"), kHeadSize, STR16("cm"),
                           0.0, 30.0, 15.0, 0, ParameterInfo::kCanAutomate));

    parameters.addParameter(
        STR16("Vibrato"), STR16(""), 0, 0.0,
        ParameterInfo::kCanAutomate, kVibrato);

    parameters.addParameter(
        STR16("Vib Rate"), STR16(""), 0, 0.5,
        ParameterInfo::kCanAutomate, kVibratoRate);

    parameters.addParameter(
        STR16("Breath"), STR16(""), 0, 0.5,
        ParameterInfo::kCanAutomate, kAspiration);

    // ADSR — RangeParameter so the host displays actual seconds
    parameters.addParameter(
        new RangeParameter(STR16("Attack"), kAttack, STR16("s"),
                           0.0, 5.0, 0.0, 0, ParameterInfo::kCanAutomate));

    parameters.addParameter(
        new RangeParameter(STR16("Decay"), kDecay, STR16("s"),
                           0.0, 5.0, 0.0, 0, ParameterInfo::kCanAutomate));

    parameters.addParameter(
        new RangeParameter(STR16("Sustain"), kSustain, STR16(""),
                           0.0, 1.0, 1.0, 0, ParameterInfo::kCanAutomate));

    parameters.addParameter(
        new RangeParameter(STR16("Release"), kRelease, STR16("s"),
                           0.0, 5.0, 0.0, 0, ParameterInfo::kCanAutomate));

    parameters.addParameter(
        new RangeParameter(STR16("Unison"), kUnison, STR16(""),
                           1.0, 10.0, 1.0, 9, ParameterInfo::kCanAutomate));

    parameters.addParameter(
        new RangeParameter(STR16("Detune"), kUnisonDetune, STR16("ct"),
                           0.0, 50.0, 0.0, 0, ParameterInfo::kCanAutomate));

    parameters.addParameter(
        STR16("Delay Rate"), STR16(""), 0, 0.5,
        ParameterInfo::kCanAutomate, kDelayRate);

    parameters.addParameter(
        STR16("Level"), STR16(""), 0, 1.0,
        ParameterInfo::kCanAutomate, kLevel);

    parameters.addParameter(
        STR16("Voice Spread"), STR16(""), 0, 0.0,
        ParameterInfo::kCanAutomate, kUnisonVoiceSpread);

    // XY pad parameters (automatable for recording pad performances)
    parameters.addParameter(STR16("XY Note"), STR16(""), 1, 0.0,
                            ParameterInfo::kCanAutomate, kXYNoteOn);
    parameters.addParameter(STR16("XY Vowel"), STR16(""), 0, 0.5,
                            ParameterInfo::kCanAutomate, kXYVowel);
    parameters.addParameter(STR16("XY Pitch"), STR16(""), 0, 0.5,
                            ParameterInfo::kCanAutomate, kXYPitchTarget);

    parameters.addParameter(
        new RangeParameter(STR16("Pitch Bend"), kPitchBend, STR16("st"),
                           -12.0, 12.0, 0.0, 0, ParameterInfo::kCanAutomate));

    // Hidden routing step: Classic (wheel → Vowel, Delay Lama compat) /
    // Both / Both inverted / Pitch (wheel → PitchBend). Persists in VST3
    // state so it travels with presets and DAW sessions. 4 steps, with
    // Classic at 0.0 and Pitch at 1.0 so old binary saves still map
    // correctly.
    parameters.addParameter(STR16("PB Routing"), STR16(""), 3, 0.0,
                            ParameterInfo::kIsHidden, kPitchBendRouting);

    // Hidden routing hub: IMidiMapping sends the hardware pitch wheel here
    // in Both / BothInverted modes. The processor fans out to PitchBend +
    // Vowel via outputParameterChanges, keeping the user-facing kPitchBend
    // slider and DAW automation lane independent of the wheel's vowel
    // coupling.
    parameters.addParameter(STR16("PW Raw"), STR16(""), 0, 0.5,
                            ParameterInfo::kIsHidden, kPitchWheelRaw);

    // Output parameter from processor (read-only, for monk animation)
    parameters.addParameter(STR16("Note Active"), STR16(""), 1, 0.0,
                            ParameterInfo::kIsHidden | ParameterInfo::kIsReadOnly, kNoteActive);

    return kResultOk;
}

} // namespace MonkSynth
