#pragma once

#include "vstgui/lib/cviewcontainer.h"

#include <functional>
#include <string>

namespace MonkSynth {

// First-run setup overlay. Drawn with VSTGUI primitives only (no bitmap assets).
// Prompts the user to import the classic theme from the original Delay Lama DLL.
class SetupView : public VSTGUI::CViewContainer {
  public:
    SetupView(const VSTGUI::CRect &size);

    using ImportCallback = std::function<void()>;

    void setImportCallback(ImportCallback cb) { importCb_ = std::move(cb); }

    void setStatusText(const std::string &text);

    void drawBackgroundRect(VSTGUI::CDrawContext *ctx, const VSTGUI::CRect &rect) override;
    VSTGUI::CMouseEventResult onMouseDown(VSTGUI::CPoint &where,
                                          const VSTGUI::CButtonState &buttons) override;
    VSTGUI::CMouseEventResult onMouseMoved(VSTGUI::CPoint &where,
                                           const VSTGUI::CButtonState &buttons) override;
    VSTGUI::CMouseEventResult onMouseExited(VSTGUI::CPoint &where,
                                            const VSTGUI::CButtonState &buttons) override;

  private:
    VSTGUI::CRect importBtnRect_;
    VSTGUI::CRect urlLinkRect_;
    VSTGUI::CRect openFolderRect_;
    std::string statusText_;
    ImportCallback importCb_;
};

} // namespace MonkSynth
