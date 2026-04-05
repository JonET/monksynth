#pragma once

#include "theme_manager.h"

#include "public.sdk/source/vst/vsteditcontroller.h"
#include "vstgui/plugin-bindings/vst3editor.h"

namespace MonkSynth {

class Controller;
class MonkView;

// Thin subclass to expose requestRecreateView() to the controller.
class ThemedVST3Editor : public VSTGUI::VST3Editor {
  public:
    using VST3Editor::VST3Editor;
    void recreateUI() { requestRecreateView(); }
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
