#include "controller.h"
#include "dll_extractor.h"
#include "monk_view.h"
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
#include "vstgui/uidescription/uidescription.h"

using namespace Steinberg;
using namespace Steinberg::Vst;
using namespace VSTGUI;

namespace MonkSynth {

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
        monkView_ = view;
        return view;
    }
    if (UTF8StringView(name) == "XYPad") {
        return new XYPadView(CRect(0, 0, 100, 100), nullptr, this);
    }
    return nullptr;
}

void Controller::didOpen(VST3Editor *editor) {
    currentEditor_ = editor;
    // Theme bitmaps are applied in ThemedVST3Editor::open() before views are
    // created, so CAnimKnob sees the real bitmap dimensions at init time.
    // We only need to show the setup overlay when no theme is available.
    if (!themeManager_.hasTheme()) {
        showSetupOverlay(editor);
    }
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

        selector->setTitle("Select Delay Lama DLL");
        selector->addFileExtension(CFileExtension("DLL Files", "dll"));

        selector->run([this, setup, themedEditor](CNewFileSelector *sel) {
            if (sel->getNumSelectedFiles() == 0)
                return;

            auto configDir = themeManager_.getClassicThemeDir().parent_path().parent_path();
            auto result = extractClassicTheme(sel->getSelectedFile(0), configDir);

            if (result.success) {
                themeManager_.setThemePath(result.themeDir);
                auto *desc = themedEditor->getUIDescription();
                if (desc)
                    desc->freePlatformResources();
                applyTheme(themedEditor);
                themedEditor->recreateUI();
            } else {
                setup->setStatusText(result.error);
            }
        });
        selector->forget();
    });

    frame->addView(setup);
}

void Controller::willClose(VST3Editor * /*editor*/) {
    monkView_ = nullptr;
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
    CCommandMenuItem::Desc headerDesc("Theme: " + themeName);
    headerDesc.flags = CMenuItem::kDisabled;
    auto *header = new CCommandMenuItem(std::move(headerDesc));
    menu->addEntry(header);

    menu->addSeparator();

    // "Load Theme..." item.
    auto *loadItem = new CCommandMenuItem(CCommandMenuItem::Desc("Load Theme..."));
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

            selector->setTitle("Select theme.json in Theme Folder");
            selector->addFileExtension(CFileExtension("JSON Files", "json"));
            if (themeManager_.hasTheme())
                selector->setInitialDirectory(
                    UTF8String(themeManager_.themePath().generic_u8string()));

            selector->run([this, themedEditor](CNewFileSelector *sel) {
                if (sel->getNumSelectedFiles() > 0) {
                    std::filesystem::path file(sel->getSelectedFile(0));
                    themeManager_.setThemePath(file.parent_path());
                    auto *desc = themedEditor->getUIDescription();
                    if (desc)
                        desc->freePlatformResources();
                    applyTheme(themedEditor);
                    themedEditor->recreateUI();
                }
            });
            selector->forget();
        });
    });
    menu->addEntry(loadItem);

    // "Import Classic Skin from DLL..." item.
    auto *importItem =
        new CCommandMenuItem(CCommandMenuItem::Desc("Import Classic Skin from DLL..."));
    importItem->setActions([this, themedEditor](CCommandMenuItem *) {
        Call::later([this, themedEditor]() {
            auto *frame = themedEditor->getFrame();
            if (!frame)
                return;

            auto *selector = CNewFileSelector::create(frame, CNewFileSelector::kSelectFile);
            if (!selector)
                return;

            selector->setTitle("Select Delay Lama DLL");
            selector->addFileExtension(CFileExtension("DLL Files", "dll"));

            selector->run([this, themedEditor](CNewFileSelector *sel) {
                if (sel->getNumSelectedFiles() == 0)
                    return;

                auto configDir = themeManager_.getClassicThemeDir().parent_path().parent_path();
                auto result = extractClassicTheme(sel->getSelectedFile(0), configDir);

                if (result.success) {
                    themeManager_.setThemePath(result.themeDir);
                    auto *desc = themedEditor->getUIDescription();
                    if (desc)
                        desc->freePlatformResources();
                    applyTheme(themedEditor);
                    themedEditor->recreateUI();
                }
            });
            selector->forget();
        });
    });
    menu->addEntry(importItem);

    // No "Reset to Default" — there's no built-in theme. Users switch
    // between imported themes or re-import from the DLL.

    return menu;
}

bool Controller::isPrivateParameter(ParamID paramID) {
    return paramID == kXYNoteOn || paramID == kXYPitch || paramID == kNoteActive;
}

tresult PLUGIN_API Controller::beginEdit(ParamID tag) {
    return EditController::beginEdit(tag);
}

tresult PLUGIN_API Controller::endEdit(ParamID tag) {
    return EditController::endEdit(tag);
}

tresult PLUGIN_API Controller::setParamNormalized(ParamID tag, ParamValue value) {
    tresult result = EditController::setParamNormalized(tag, value);
    if (result == kResultOk && monkView_) {
        if (tag == kVowel)
            monkView_->setVowelValue(static_cast<float>(value));
        else if (tag == kXYNoteOn || tag == kNoteActive)
            monkView_->setNoteActive(value > 0.5);
    }
    return result;
}

tresult PLUGIN_API Controller::initialize(FUnknown *context) {
    tresult result = EditController::initialize(context);
    if (result != kResultOk)
        return result;

    themeManager_.loadConfig();

    parameters.addParameter(
        STR16("Glide"), STR16(""), 0, 0.5,
        ParameterInfo::kCanAutomate, kPortTime);

    parameters.addParameter(
        STR16("Vowel"), STR16(""), 0, 0.5,
        ParameterInfo::kCanAutomate, kVowel);

    parameters.addParameter(
        STR16("Delay"), STR16(""), 0, 0.8,
        ParameterInfo::kCanAutomate, kDelay);

    parameters.addParameter(
        STR16("Voice"), STR16(""), 0, 0.5,
        ParameterInfo::kCanAutomate, kHeadSize);

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

    // Private parameters for XY pad (not automatable, not visible to host)
    parameters.addParameter(STR16("XY Note"), STR16(""), 1, 0.0, ParameterInfo::kIsHidden,
                            kXYNoteOn);
    parameters.addParameter(STR16("XY Pitch"), STR16(""), 0, 0.5, ParameterInfo::kIsHidden,
                            kXYPitch);

    // Output parameter from processor (read-only, for monk animation)
    parameters.addParameter(STR16("Note Active"), STR16(""), 1, 0.0,
                            ParameterInfo::kIsHidden | ParameterInfo::kIsReadOnly, kNoteActive);

    return kResultOk;
}

} // namespace MonkSynth
