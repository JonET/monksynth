#pragma once

#include "theme_manager.h"

#include "pluginterfaces/vst/ivstmidicontrollers.h"
#include "public.sdk/source/vst/vsteditcontroller.h"
#include "vstgui/lib/cbitmap.h"
#include "vstgui/lib/cvstguitimer.h"
#include "vstgui/lib/platform/platformfactory.h"
#include "vstgui/plugin-bindings/vst3editor.h"
#include "vstgui/uidescription/uidescription.h"

#include <cstdint>
#include <filesystem>
#include <functional>
#include <optional>
#include <vector>

namespace MonkSynth {

class Controller;
class InfoButton;
class MonkView;
class OverlayView;
class SetupView;

// Subclass that applies theme bitmaps before views are created, so that
// controls like CAnimKnob see the real bitmap dimensions at init time.
class ThemedVST3Editor : public VSTGUI::VST3Editor {
  public:
    ThemedVST3Editor(Steinberg::Vst::EditController *controller, VSTGUI::UTF8StringPtr templateName,
                     VSTGUI::UTF8StringPtr xmlFile, ThemeManager *themeManager)
        : VST3Editor(controller, templateName, xmlFile), themeManager_(themeManager) {}

    void recreateUI() { requestRecreateView(); }

    bool PLUGIN_API open(void *parent, const VSTGUI::PlatformType &type) override {
        // Swap placeholder bitmaps for real theme assets before the base class
        // creates views.  CAnimKnob::setBackground() recalculates numSubPixmaps
        // from the bitmap height, so the bitmaps must already be full-size.
        if (themeManager_) {
            themeManager_->autoDetectClassicTheme();
            if (themeManager_->hasTheme())
                applyThemeBitmaps();
        }
        return VST3Editor::open(parent, type);
    }

  private:
    void applyThemeBitmaps() {
        auto *desc = getUIDescription();
        if (!desc || !themeManager_)
            return;

        for (auto &[name, filename] : ThemeManager::bitmapFileMap()) {
            auto path = themeManager_->resolveThemeBitmap(name);
            if (!path)
                continue;

            auto platformBmp = VSTGUI::getPlatformFactory().createBitmapFromPath(
                path->generic_u8string().c_str());
            if (!platformBmp)
                continue;

            VSTGUI::CBitmap *bmp = desc->getBitmap(name.c_str());
            if (bmp)
                bmp->setPlatformBitmap(platformBmp);
        }
    }

    ThemeManager *themeManager_ = nullptr;
};

class Controller : public Steinberg::Vst::EditController,
                   public VSTGUI::VST3EditorDelegate,
                   public Steinberg::Vst::IMidiMapping {
  public:
    using VST3Editor = VSTGUI::VST3Editor;
    using UTF8StringPtr = VSTGUI::UTF8StringPtr;
    using IUIDescription = VSTGUI::IUIDescription;

    // COM interface plumbing for IMidiMapping
    Steinberg::tresult PLUGIN_API queryInterface(const Steinberg::TUID iid, void **obj) override {
        QUERY_INTERFACE(iid, obj, Steinberg::Vst::IMidiMapping::iid,
                        Steinberg::Vst::IMidiMapping)
        return EditController::queryInterface(iid, obj);
    }
    DELEGATE_REFCOUNT(EditController)

    static Steinberg::FUnknown *createInstance(void *) {
        return static_cast<Steinberg::Vst::IEditController *>(new Controller());
    }

    ~Controller() override { cancelDeferredUI(); }

    // EditController overrides
    Steinberg::tresult PLUGIN_API initialize(Steinberg::FUnknown *context) override;
    Steinberg::tresult PLUGIN_API terminate() override;
    Steinberg::IPlugView *PLUGIN_API createView(const char *name) override;
    Steinberg::tresult PLUGIN_API setParamNormalized(Steinberg::Vst::ParamID tag,
                                                     Steinberg::Vst::ParamValue value) override;
    Steinberg::tresult PLUGIN_API setComponentState(Steinberg::IBStream *state) override;
    Steinberg::tresult beginEdit(Steinberg::Vst::ParamID tag) override;
    Steinberg::tresult endEdit(Steinberg::Vst::ParamID tag) override;

    // VST3EditorDelegate overrides
    VSTGUI::CView *createCustomView(UTF8StringPtr name, const VSTGUI::UIAttributes &attributes,
                                    const IUIDescription *description, VST3Editor *editor) override;
    void didOpen(VST3Editor *editor) override;
    void willClose(VST3Editor *editor) override;
    VSTGUI::COptionMenu *createContextMenu(const VSTGUI::CPoint &pos, VST3Editor *editor) override;
    bool isPrivateParameter(Steinberg::Vst::ParamID paramID) override;

    // IMidiMapping
    Steinberg::tresult PLUGIN_API
    getMidiControllerAssignment(Steinberg::int32 busIndex, Steinberg::int16 channel,
                                Steinberg::Vst::CtrlNumber midiControllerNumber,
                                Steinberg::Vst::ParamID &id) override;

  private:
    void applyTheme(VST3Editor *editor);
    // Persists |themeDir| as the active theme and rebuilds the editor UI
    // with its bitmaps.
    // Persists the choice immediately and rebuilds the open editor (if any)
    // on the next deferred tick, since callers are click handlers, menu
    // actions and file-panel callbacks that can't rebuild the view tree
    // they are dispatching from.
    void selectTheme(const std::filesystem::path &themeDir, bool bundled);
    void rebuildEditorForTheme();
    // Adds a modal overlay to the editor's frame unless one is already open.
    void presentOverlay(VST3Editor *editor, OverlayView *view);
    void showSetupOverlay(VST3Editor *editor);
    // Extracts the classic theme from |dllPath| and switches to it; errors
    // are shown on |setup| when given.
    void importClassicFromDll(ThemedVST3Editor *editor, SetupView *setup,
                              const std::filesystem::path &dllPath);
    void showInfoOverlay(VST3Editor *editor);
    void showThemeInfoOverlay(VST3Editor *editor);

    // Pitch-wheel spring-back: after the user releases the pitch bend slider,
    // ease the value back to center in a fresh edit gesture so the return
    // also gets recorded into automation, matching how DAWs record a hardware
    // pitch wheel.
    enum class PbSpring { Idle, UserDragging, Springing };
    void startPitchBendSpring(double from);
    void tickPitchBendSpring();

    MonkView *monkView_ = nullptr;
    InfoButton *infoButton_ = nullptr;
    VST3Editor *currentEditor_ = nullptr;
    // The overlay currently shown on currentEditor_'s frame, if any. Only
    // compared by identity (never dereferenced) since recreateUI can destroy
    // it without going through the close callback.
    OverlayView *overlay_ = nullptr;
    ThemeManager themeManager_;
    int noteRefCount_ = 0;  // tracks active touches on vowel/pitch controls
    bool inSetParam_ = false; // re-entrancy guard for setParamNormalized

    VSTGUI::SharedPointer<VSTGUI::CVSTGUITimer> pitchBendSpringTimer_;
    // Settles the pitch bend and closes the edit gesture if a spring-back
    // is in flight; used when the editor closes mid-animation.
    void finishPitchBendSpring();

    // Deferred UI work. VSTGUI's Call::later cannot be cancelled, and a
    // closure firing after the host has released the editor is a
    // use-after-free (JUCE based hosts on Linux keep dispatching timers
    // after the window closes). These timers are owned here and stopped in
    // willClose and terminate. Closures read currentEditor_ when they run
    // rather than trusting a captured editor pointer.
    void deferUI(std::function<void()> fn);
    void cancelDeferredUI();
    std::vector<VSTGUI::SharedPointer<VSTGUI::CVSTGUITimer>> deferredUI_;
    // While the setup screen is showing, polls the themes folder so a DLL
    // dropped in is imported without another click. Non-null exactly while
    // the setup screen is up on currentEditor_.
    VSTGUI::SharedPointer<VSTGUI::CVSTGUITimer> setupDllWatch_;
    void stopSetupDllWatch();
    // Identity of a DLL file on disk; a changed size or mtime is a new file.
    struct DllKey {
        std::filesystem::path path;
        std::uintmax_t size = 0;
        std::filesystem::file_time_type mtime;
        static DllKey of(const std::filesystem::path &p);
        bool operator==(const DllKey &o) const {
            return path == o.path && size == o.size && mtime == o.mtime;
        }
    };
    // The last DLL that failed to import; findFolderDll() skips it.
    std::optional<DllKey> failedDll_;
    // First *.dll in the themes or config folder, if any.
    std::optional<std::filesystem::path> findFolderDll() const;
    PbSpring pbSpringState_ = PbSpring::Idle;
    double pbSpringStart_ = 0.5;
    double pbSpringElapsedMs_ = 0.0;
};

} // namespace MonkSynth
