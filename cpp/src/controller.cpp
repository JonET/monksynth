#include "controller.h"
#include "pluginterfaces/base/ibstream.h"
#include "adsr_curve.h"
#include "arc_knob.h"
#include "section_panel.h"
#include "value_readout.h"
#include "dll_extractor.h"
#include "i18n.h"
#include "info_button.h"
#include "info_view.h"
#include "monk_view.h"
#include "open_url.h"
#include "plugin_cids.h"
#include "setup_view.h"
#include "xy_pad.h"

#include "public.sdk/source/vst/vstparameters.h"
#include "vstgui/lib/cbitmap.h"
#include "vstgui/lib/cfileselector.h"
#include "vstgui/lib/cframe.h"
#include "vstgui/lib/controls/coptionmenu.h"
#include "vstgui/lib/cvstguitimer.h"
#include "vstgui/lib/platform/platformfactory.h"
#include "vstgui/uidescription/uiattributes.h"
#include "vstgui/uidescription/uidescription.h"

#include <cmath>
#include <stdexcept>

using namespace Steinberg;
using namespace Steinberg::Vst;
using namespace VSTGUI;

namespace MonkSynth {

// Quadratic taper: toPlain(n) = n² * max, toNormalized(p) = √(p/max).
// Used for ADSR time parameters so the knob has fine resolution at short
// times (where users actually live) while still reaching the full 5s at
// norm=1. Default of 0 stays at 0 — preserves the original Delay Lama
// "no envelope" behavior out of the box.
class SquaredRangeParameter : public Steinberg::Vst::RangeParameter {
  public:
    using RangeParameter::RangeParameter;

    Steinberg::Vst::ParamValue toPlain(Steinberg::Vst::ParamValue n) const override {
        return n * n * (maxPlain - minPlain) + minPlain;
    }

    Steinberg::Vst::ParamValue toNormalized(Steinberg::Vst::ParamValue p) const override {
        double frac = (p - minPlain) / (maxPlain - minPlain);
        if (frac <= 0.0)
            return 0.0;
        if (frac >= 1.0)
            return 1.0;
        return std::sqrt(frac);
    }
};

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
        const char *tmpl = themeManager_.advancedMode() ? "view_advanced" : "view";
        return new ThemedVST3Editor(this, tmpl, "editor.uidesc", &themeManager_);
    }
    return nullptr;
}

CView *Controller::createCustomView(UTF8StringPtr name, const UIAttributes &attributes,
                                    const IUIDescription *description, VST3Editor *editor) {
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
    if (UTF8StringView(name) == "AdsrCurve") {
        auto *curve = new AdsrCurve(CRect(0, 0, 100, 60));
        curve->setAttack(static_cast<float>(getParamNormalized(kAttack)));
        curve->setDecay(static_cast<float>(getParamNormalized(kDecay)));
        curve->setSustain(static_cast<float>(getParamNormalized(kSustain)));
        curve->setRelease(static_cast<float>(getParamNormalized(kRelease)));
        adsrCurve_ = curve;
        return curve;
    }
    if (UTF8StringView(name) == "SectionPanel") {
        auto *panel = new SectionPanel(CRect(0, 0, 100, 100));
        if (auto *titleStr = attributes.getAttributeValue("section-title")) {
            panel->setTitle(*titleStr);
        }
        return panel;
    }
    if (UTF8StringView(name) == "ArcKnob") {
        int32_t tag = -1;
        if (auto *tagStr = attributes.getAttributeValue("control-tag")) {
            tag = description->getTagForName(tagStr->c_str());
            if (tag == -1)
                tag = static_cast<int32_t>(strtol(tagStr->c_str(), nullptr, 10));
        }
        return new ArcKnob(CRect(0, 0, 50, 50), editor, tag);
    }
    if (UTF8StringView(name) == "AdsrValueReadout") {
        int32_t tag = -1;
        if (auto *tagStr = attributes.getAttributeValue("control-tag")) {
            tag = description->getTagForName(tagStr->c_str());
            if (tag == -1)
                tag = static_cast<int32_t>(strtol(tagStr->c_str(), nullptr, 10));
        }
        return new AdsrValueReadout(CRect(0, 0, 60, 12), editor, tag);
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

void Controller::showSetupOverlay(VST3Editor *editor) {
    auto *frame = editor->getFrame();
    if (!frame)
        return;

    auto *setup = new SetupView(CRect(0, 0, 360, 510));
    auto *themedEditor = static_cast<ThemedVST3Editor *>(editor);

    setup->setImportCallback([this, frame, setup, themedEditor]() {
        auto *selector = CNewFileSelector::create(frame, CNewFileSelector::kSelectFile);
        if (!selector)
            return;

        selector->setTitle(i18n::str(i18n::StringId::FileSelectDll));
        selector->addFileExtension(
            CFileExtension(i18n::str(i18n::StringId::FileExtDll), "dll"));

        selector->run([this, setup, themedEditor](CNewFileSelector *sel) {
            try {
                if (sel->getNumSelectedFiles() == 0)
                    return;

                auto dllPath = pathFromUTF8(sel->getSelectedFile(0));
                auto configDir = themeManager_.getClassicThemeDir().parent_path().parent_path();
                auto result = extractClassicTheme(dllPath, configDir);

                if (result.success) {
                    themeManager_.setThemePath(result.themeDir);
                    // Defer UI recreation until after the file selector callback
                    // returns — recreating views inside the callback causes
                    // broken event handling on Linux.
                    Call::later([this, themedEditor]() {
                        auto *desc = themedEditor->getUIDescription();
                        if (desc)
                            desc->freePlatformResources();
                        applyTheme(themedEditor);
                        themedEditor->recreateUI();
                    });
                } else {
                    setup->setStatusText(result.error);
                }
            } catch (const std::exception &e) {
                setup->setStatusText(std::string("Error: ") + e.what());
            } catch (...) {
                setup->setStatusText("An unexpected error occurred.");
            }
        });
        selector->forget();
    });

    frame->addView(setup);
}

void Controller::showInfoOverlay(VST3Editor *editor) {
    auto *frame = editor->getFrame();
    if (!frame)
        return;

    auto *info = new InfoView(CRect(0, 0, 360, 510));
    info->setCloseCallback([frame, info]() { frame->removeView(info); });
    frame->addView(info);
}

void Controller::willClose(VST3Editor * /*editor*/) {
    monkView_ = nullptr;
    adsrCurve_ = nullptr;
    infoButton_ = nullptr;
    currentEditor_ = nullptr;
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

COptionMenu *Controller::createContextMenu(const CPoint & /*pos*/, VST3Editor *editor) {
    auto *menu = new COptionMenu();
    auto *themedEditor = static_cast<ThemedVST3Editor *>(editor);

    // Show current theme name as a disabled header.
    std::string themeName = themeManager_.getThemeName();
    CCommandMenuItem::Desc headerDesc(
        std::string(i18n::str(i18n::StringId::MenuThemePrefix)) + themeName);
    headerDesc.flags = CMenuItem::kDisabled;
    auto *header = new CCommandMenuItem(std::move(headerDesc));
    menu->addEntry(header);

    menu->addSeparator();

    // "Load Theme..." item.
    auto *loadItem =
        new CCommandMenuItem(CCommandMenuItem::Desc(i18n::str(i18n::StringId::MenuLoadTheme)));
    loadItem->setActions([this, themedEditor](CCommandMenuItem *) {
        // Defer file dialog opening until after the context menu's tracking
        // loop has ended.  On macOS, opening NSOpenPanel while the NSMenu is
        // still active crashes the host (e.g. Ableton Live).
        Call::later([this, themedEditor]() {
            auto *frame = themedEditor->getFrame();
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

            selector->run([this, themedEditor](CNewFileSelector *sel) {
                try {
                    if (sel->getNumSelectedFiles() > 0) {
                        auto file = pathFromUTF8(sel->getSelectedFile(0));
                        themeManager_.setThemePath(file.parent_path());
                        auto *desc = themedEditor->getUIDescription();
                        if (desc)
                            desc->freePlatformResources();
                        applyTheme(themedEditor);
                        themedEditor->recreateUI();
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
    importItem->setActions([this, themedEditor](CCommandMenuItem *) {
        Call::later([this, themedEditor]() {
            auto *frame = themedEditor->getFrame();
            if (!frame)
                return;

            auto *selector = CNewFileSelector::create(frame, CNewFileSelector::kSelectFile);
            if (!selector)
                return;

            selector->setTitle(i18n::str(i18n::StringId::FileSelectDll));
            selector->addFileExtension(
                CFileExtension(i18n::str(i18n::StringId::FileExtDll), "dll"));

            selector->run([this, themedEditor](CNewFileSelector *sel) {
                try {
                    if (sel->getNumSelectedFiles() == 0)
                        return;

                    auto dllPath = pathFromUTF8(sel->getSelectedFile(0));
                    auto configDir = themeManager_.getClassicThemeDir().parent_path().parent_path();
                    auto result = extractClassicTheme(dllPath, configDir);

                    if (result.success) {
                        themeManager_.setThemePath(result.themeDir);
                        auto *desc = themedEditor->getUIDescription();
                        if (desc)
                            desc->freePlatformResources();
                        applyTheme(themedEditor);
                        themedEditor->recreateUI();
                    }
                } catch (...) {
                }
            });
            selector->forget();
        });
    });
    menu->addEntry(importItem);

    menu->addSeparator();

    // "Show Advanced Parameters" — checkable toggle that swaps between the
    // "view" and "view_advanced" uidesc templates. The wider template exposes
    // ADSR, unison, aspiration, etc. State persists via ThemeManager.
    CCommandMenuItem::Desc advDesc(i18n::str(i18n::StringId::MenuAdvanced));
    if (themeManager_.advancedMode())
        advDesc.flags |= CMenuItem::kChecked;
    auto *advItem = new CCommandMenuItem(std::move(advDesc));
    advItem->setActions([this, themedEditor](CCommandMenuItem *) {
        bool next = !themeManager_.advancedMode();
        themeManager_.setAdvancedMode(next);
        // Deferred so the swap happens after the context-menu tracking loop
        // unwinds (matches the file-dialog pattern above).
        Call::later([themedEditor, next]() {
            themedEditor->exchangeView(next ? "view_advanced" : "view");
            // FL Studio interprets resizeView's ViewRect as physical pixels
            // (not logical), so at 125% Windows DPI a logical 360 arrives
            // as a 360-physical-pixel window — visually smaller than
            // intended. Multiply by the editor's current content scale
            // factor so the physical pixels match the desired logical
            // size. At 100% scale this is a no-op. Defer to a second loop
            // turn so it lands after recreateView's own requestResize.
            Call::later([themedEditor, next]() {
                double scale = themedEditor->contentScaleFactor();
                CPoint sz((next ? 760.0 : 360.0) * scale, 510.0 * scale);
                themedEditor->requestResize(sz);
            });
        });
    });
    menu->addEntry(advItem);

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
    for (int i = 0; i < kNumParams; i++) {
        float v;
        if (state->read(&v, sizeof(v), nullptr) != kResultOk)
            return kResultFalse;
        setParamNormalized(static_cast<ParamID>(i), static_cast<ParamValue>(v));
    }
    return kResultOk;
}

tresult PLUGIN_API Controller::beginEdit(ParamID tag) {
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

tresult PLUGIN_API Controller::endEdit(ParamID tag) {
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
        } else if (tag == kAttack) {
            if (adsrCurve_)
                adsrCurve_->setAttack(static_cast<float>(value));
        } else if (tag == kDecay) {
            if (adsrCurve_)
                adsrCurve_->setDecay(static_cast<float>(value));
        } else if (tag == kSustain) {
            if (adsrCurve_)
                adsrCurve_->setSustain(static_cast<float>(value));
        } else if (tag == kRelease) {
            if (adsrCurve_)
                adsrCurve_->setRelease(static_cast<float>(value));
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

    // ADSR — quadratic taper on A/D/R so norm=0 still means 0s (keeping
    // the original Delay Lama "no envelope by default" behavior) but the
    // knob travel has much finer resolution at short times.
    parameters.addParameter(
        new SquaredRangeParameter(STR16("Attack"), kAttack, STR16("s"),
                                  0.0, 3.0, 0.0, 0, ParameterInfo::kCanAutomate));

    parameters.addParameter(
        new SquaredRangeParameter(STR16("Decay"), kDecay, STR16("s"),
                                  0.0, 3.0, 0.0, 0, ParameterInfo::kCanAutomate));

    parameters.addParameter(
        new RangeParameter(STR16("Sustain"), kSustain, STR16(""),
                           0.0, 1.0, 1.0, 0, ParameterInfo::kCanAutomate));

    parameters.addParameter(
        new SquaredRangeParameter(STR16("Release"), kRelease, STR16("s"),
                                  0.0, 3.0, 0.0, 0, ParameterInfo::kCanAutomate));

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
