#pragma once

#include "vstgui/lib/ccolor.h"
#include "vstgui/lib/cfont.h"
#include "vstgui/lib/cviewcontainer.h"
#include "vstgui/lib/vstguifwd.h"

#include <string>

namespace MonkSynth {

class SectionPanel : public VSTGUI::CViewContainer {
  public:
    SectionPanel(const VSTGUI::CRect &size);

    void setTitle(const std::string &t);
    const std::string &title() const { return title_; }

    void drawBackgroundRect(VSTGUI::CDrawContext *pContext, const VSTGUI::CRect &updateRect) override;

    void setTitleHeight(VSTGUI::CCoord h) { titleHeight_ = h; invalid(); }
    void setCornerRadius(VSTGUI::CCoord r) { cornerRadius_ = r; invalid(); }

    CLASS_METHODS(SectionPanel, CViewContainer)

  private:
    std::string title_;
    VSTGUI::CCoord titleHeight_ = 22.0;
    VSTGUI::CCoord cornerRadius_ = 4.0;
    VSTGUI::CColor titleBg_{0, 0, 0, 164};       // ~0.644 alpha
    VSTGUI::CColor contentBg_{0, 0, 0, 86};      // ~0.336 alpha
    VSTGUI::CColor titleText_{255, 255, 255, 255};
    VSTGUI::SharedPointer<VSTGUI::CFontDesc> titleFont_;
};

} // namespace MonkSynth
