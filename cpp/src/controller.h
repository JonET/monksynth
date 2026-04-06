#pragma once

#include "theme_manager.h"

#include "public.sdk/source/vst/vsteditcontroller.h"
#include "vstgui/lib/cbitmap.h"
#include "vstgui/lib/platform/platformfactory.h"
#include "vstgui/plugin-bindings/vst3editor.h"
#include "vstgui/uidescription/uidescription.h"

namespace MonkSynth {

class Controller;
class MonkView;

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

class Controller : public Steinberg::Vst::EditController, public VSTGUI::VST3EditorDelegate {
  public:
    using VST3Editor = VSTGUI::VST3Editor;
    using UTF8StringPtr = VSTGUI::UTF8StringPtr;
    using IUIDescription = VSTGUI::IUIDescription;

    static Steinberg::FUnknown *createInstance(void *) {
        return static_cast<Steinberg::Vst::IEditController *>(new Controller());
    }

    // EditController overrides
    Steinberg::tresult PLUGIN_API initialize(Steinberg::FUnknown *context) override;
    Steinberg::IPlugView *PLUGIN_API createView(const char *name) override;
    Steinberg::tresult PLUGIN_API setParamNormalized(Steinberg::Vst::ParamID tag,
                                                     Steinberg::Vst::ParamValue value) override;
    Steinberg::tresult PLUGIN_API beginEdit(Steinberg::Vst::ParamID tag) override;
    Steinberg::tresult PLUGIN_API endEdit(Steinberg::Vst::ParamID tag) override;

    // VST3EditorDelegate overrides
    VSTGUI::CView *createCustomView(UTF8StringPtr name, const VSTGUI::UIAttributes &attributes,
                                    const IUIDescription *description, VST3Editor *editor) override;
    void didOpen(VST3Editor *editor) override;
    void willClose(VST3Editor *editor) override;
    VSTGUI::COptionMenu *createContextMenu(const VSTGUI::CPoint &pos, VST3Editor *editor) override;
    bool isPrivateParameter(Steinberg::Vst::ParamID paramID) override;

  private:
    void applyTheme(VST3Editor *editor);
    void showSetupOverlay(VST3Editor *editor);

    MonkView *monkView_ = nullptr;
    VST3Editor *currentEditor_ = nullptr;
    ThemeManager themeManager_;
    int noteRefCount_ = 0; // tracks active touches on vowel/pitch controls
};

} // namespace MonkSynth
