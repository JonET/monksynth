#include "setup_view.h"
#include "i18n.h"
#include "draw_utils.h"
#include "vstgui/lib/idatapackage.h"
#include "open_url.h"
#include "theme_manager.h"
#include "version.h"

#include "vstgui/lib/cdrawcontext.h"
#include "vstgui/lib/cfont.h"
#include "vstgui/lib/cframe.h"
#include "vstgui/lib/cgraphicspath.h"
#include "vstgui/lib/cstring.h"

#include <algorithm>
#include <cctype>
#include <optional>

using namespace VSTGUI;

namespace MonkSynth {

SetupView::SetupView(const CRect &size) : CViewContainer(size) {
    setTransparency(false);

    // Import button position (relative to view)
    double bw = 220, bh = 36;
    double bx = (size.getWidth() - bw) / 2;
    double by = 308;
    importBtnRect_ = CRect(bx, by, bx + bw, by + bh);
}

// First .dll file path in a drag's data package, if any.
static std::optional<std::filesystem::path> dllInPackage(IDataPackage *pkg) {
    if (!pkg)
        return std::nullopt;
    for (uint32_t i = 0; i < pkg->getCount(); i++) {
        if (pkg->getDataType(i) != IDataPackage::kFilePath)
            continue;
        const void *buf = nullptr;
        IDataPackage::Type type;
        uint32_t size = pkg->getData(i, buf, type);
        if (!buf || size == 0)
            continue;
        std::string s(static_cast<const char *>(buf), size);
        while (!s.empty() && s.back() == '\0')
            s.pop_back();
        std::filesystem::path p = std::filesystem::u8path(s);
        std::string ext = p.extension().string();
        std::transform(ext.begin(), ext.end(), ext.begin(),
                       [](unsigned char c) { return std::tolower(c); });
        if (ext == ".dll")
            return p;
    }
    return std::nullopt;
}

// Accepts a dropped Delay Lama DLL. Owned by VSTGUI for the duration of a
// drag; holds the view alive alongside.
class SetupDropTarget : public DropTargetAdapter, public NonAtomicReferenceCounted {
  public:
    SetupDropTarget(SetupView *view, std::function<void(const std::filesystem::path &)> cb)
        : view_(view), cb_(std::move(cb)) {}

    DragOperation onDragEnter(DragEventData data) override { return onDragMove(data); }
    DragOperation onDragMove(DragEventData data) override {
        bool ok = dllInPackage(data.drag).has_value();
        view_->setDragHover(ok);
        return ok ? DragOperation::Copy : DragOperation::None;
    }
    void onDragLeave(DragEventData) override { view_->setDragHover(false); }
    bool onDrop(DragEventData data) override {
        view_->setDragHover(false);
        auto dll = dllInPackage(data.drag);
        if (!dll)
            return false;
        if (cb_)
            cb_(*dll);
        return true;
    }

  private:
    SharedPointer<SetupView> view_;
    std::function<void(const std::filesystem::path &)> cb_;
};

SharedPointer<IDropTarget> SetupView::getDropTarget() {
    return makeOwned<SetupDropTarget>(this, dllDropCb_);
}

void SetupView::setDragHover(bool hover) {
    if (dragHover_ == hover)
        return;
    dragHover_ = hover;
    setDirty(true);
}

void SetupView::setBuiltInThemes(std::vector<ThemeManager::InstalledTheme> themes) {
    builtInThemes_ = std::move(themes);
    setDirty(true);
}

void SetupView::setStatusText(const std::string &text) {
    statusText_ = text;
    setDirty(true);
}

void SetupView::drawBackgroundRect(CDrawContext *ctx, const CRect & /*rect*/) {
    CRect bounds = getViewSize();
    const char *font = i18n::uiFont();

    // Enable anti-aliasing for sharp rendering on HiDPI
    ctx->setDrawMode(kAntiAliasing | kNonIntegralMode);

    // Dark background
    ctx->setFillColor(CColor(30, 30, 35, 255));
    ctx->drawRect(bounds, kDrawFilled);

    // Accent line
    CRect accent(bounds.left + 30, bounds.top + 40, bounds.right - 30, bounds.top + 42);
    ctx->setFillColor(CColor(200, 150, 50, 255));
    ctx->drawRect(accent, kDrawFilled);

    // Drop highlight while a DLL is dragged over the view
    if (dragHover_) {
        CRect border(bounds);
        border.inset(3, 3);
        ctx->setFrameColor(CColor(200, 150, 50, 255));
        ctx->setLineWidth(3);
        ctx->drawRect(border, kDrawStroked);
        ctx->setLineWidth(1);
    }

    auto *titleFont = new CFontDesc(font, 24, kBoldFace);
    auto *bodyFont = new CFontDesc(font, 13);
    auto *linkFont = new CFontDesc(font, 13, kUnderlineFace);
    auto *smallFont = new CFontDesc(font, 11);
    auto *btnFont = new CFontDesc(font, 14, kBoldFace);
    auto *sectionFont = new CFontDesc(font, 14, kBoldFace);

    // Title
    ctx->setFont(titleFont);
    ctx->setFontColor(CColor(230, 230, 230, 255));
    CRect titleRect(bounds.left, bounds.top + 55, bounds.right, bounds.top + 85);
    ctx->drawString("MonkSynth", titleRect, kCenterText);

    // Version
    ctx->setFont(smallFont);
    ctx->setFontColor(CColor(140, 140, 140, 255));
    CRect verRect(bounds.left, bounds.top + 90, bounds.right, bounds.top + 108);
    ctx->drawString(MONKSYNTH_VERSION " " MONKSYNTH_VERSION_LABEL, verRect, kCenterText);

    // Body text
    ctx->setFont(bodyFont);
    ctx->setFontColor(CColor(190, 190, 190, 255));

    const char *lines[] = {
        i18n::str(i18n::StringId::SetupNeedsTheme),
        "",
        i18n::str(i18n::StringId::SetupImportFromClassic1),
        i18n::str(i18n::StringId::SetupImportFromClassic2),
        "",
        i18n::str(i18n::StringId::SetupDownloadFrom),
    };
    double lineY = bounds.top + 130;
    for (const char *line : lines) {
        CRect lr(bounds.left + 20, lineY, bounds.right - 20, lineY + 18);
        ctx->drawString(line, lr, kCenterText);
        lineY += 19;
    }

    // Clickable URL link
    ctx->setFont(linkFont);
    ctx->setFontColor(CColor(130, 170, 255, 255));
    CRect linkRect(bounds.left + 20, lineY, bounds.right - 20, lineY + 18);
    ctx->drawString("www.audionerdz.nl/download.htm", linkRect, kCenterText);
    urlLinkRect_ = linkRect;
    urlLinkRect_.offset(-bounds.left, -bounds.top);
    lineY += 19;

    // Remaining body text
    ctx->setFont(bodyFont);
    ctx->setFontColor(CColor(190, 190, 190, 255));
    const char *lines2[] = {
        i18n::str(i18n::StringId::SetupThenClick1),
        i18n::str(i18n::StringId::SetupThenClick2),
    };
    for (const char *line : lines2) {
        CRect lr(bounds.left + 20, lineY, bounds.right - 20, lineY + 18);
        ctx->drawString(line, lr, kCenterText);
        lineY += 19;
    }

    // Import button (rounded rect)
    CRect btn = importBtnRect_;
    btn.offset(bounds.left, bounds.top);
    ctx->setFillColor(CColor(200, 150, 50, 255));
    drawRoundRect(ctx, btn, 6);

    ctx->setFont(btnFont);
    ctx->setFontColor(CColor(30, 30, 35, 255));
    ctx->drawString(i18n::str(i18n::StringId::SetupImportButton), btn, kCenterText);

    // Status text: up to two lines split on '\n' (message + hint), squeezed
    // into the gap between the button and the built-in theme link.
    if (!statusText_.empty()) {
        ctx->setFont(smallFont);
        ctx->setFontColor(CColor(220, 180, 100, 255));
        double sy = btn.bottom + 4;
        size_t start = 0;
        for (int line = 0; line < 2 && start <= statusText_.size(); line++) {
            size_t nl = statusText_.find('\n', start);
            std::string text = statusText_.substr(start, nl == std::string::npos ? std::string::npos
                                                                                  : nl - start);
            CRect statusRect(bounds.left + 20, sy, bounds.right - 20, sy + 13);
            // OS error text and the ja/ko strings can exceed the width.
            text = ellipsize(ctx, text, statusRect.getWidth());
            ctx->drawString(text.c_str(), statusRect, kCenterText);
            sy += 13;
            if (nl == std::string::npos)
                break;
            start = nl + 1;
        }
    }

    // Built-in theme shortcut: "Or use the built-in theme: <name>" on one
    // line, with only the name drawn as a link. Deliberately low-key so the
    // classic import above stays the obvious path.
    builtInLinkRect_ = CRect();
    if (!builtInThemes_.empty()) {
        const std::string prefix = i18n::str(i18n::StringId::SetupOrBuiltIn);
        const CCoord usable = bounds.getWidth() - 40;

        ctx->setFont(bodyFont);
        CCoord wPrefix = ctx->getStringWidth(prefix.c_str());
        // Theme names are unbounded community input: keep the whole line
        // inside the view.
        ctx->setFont(linkFont);
        std::string name = ellipsize(ctx, builtInThemes_.front().name, usable - wPrefix);
        CCoord wName = ctx->getStringWidth(name.c_str());

        double ly = btn.bottom + 30;
        double x0 = bounds.left + (bounds.getWidth() - (wPrefix + wName)) / 2;

        ctx->setFont(bodyFont);
        ctx->setFontColor(CColor(150, 150, 155, 255));
        CRect prefixRect(x0, ly, x0 + wPrefix, ly + 18);
        ctx->drawString(prefix.c_str(), prefixRect, kLeftText);

        ctx->setFont(linkFont);
        ctx->setFontColor(CColor(130, 170, 255, 255));
        CRect nameRect(x0 + wPrefix, ly, x0 + wPrefix + wName, ly + 18);
        ctx->drawString(name.c_str(), nameRect, kLeftText);
        builtInLinkRect_ = nameRect;
        builtInLinkRect_.offset(-bounds.left, -bounds.top);
    }

    // ---- Contribute section ----
    double cy = bounds.top + 400;
    CRect sepRect(bounds.left + 30, cy, bounds.right - 30, cy + 1);
    ctx->setFillColor(CColor(200, 150, 50, 120));
    ctx->drawRect(sepRect, kDrawFilled);
    cy += 8;

    ctx->setFont(sectionFont);
    ctx->setFontColor(CColor(200, 150, 50, 255));
    CRect headerRect(bounds.left + 20, cy, bounds.right - 20, cy + 20);
    ctx->drawString(i18n::str(i18n::StringId::ContributeHeader), headerRect, kCenterText);
    cy += 22;

    ctx->setFont(bodyFont);
    ctx->setFontColor(CColor(190, 190, 190, 255));
    CRect shareRect(bounds.left + 20, cy, bounds.right - 20, cy + 18);
    ctx->drawString(i18n::str(i18n::StringId::ContributeShare), shareRect, kCenterText);
    cy += 18;

    ctx->setFontColor(CColor(150, 150, 155, 255));
    CRect lookingRect(bounds.left + 20, cy, bounds.right - 20, cy + 18);
    ctx->drawString(i18n::str(i18n::StringId::ContributeLookingFor), lookingRect, kCenterText);
    cy += 20;

    ctx->setFont(linkFont);
    ctx->setFontColor(CColor(130, 170, 255, 255));
    CRect folderRect(bounds.left + 20, cy, bounds.right - 20, cy + 18);
    ctx->drawString(i18n::str(i18n::StringId::ContributeOpenFolder), folderRect, kCenterText);
    openFolderRect_ = folderRect;
    openFolderRect_.offset(-bounds.left, -bounds.top);

    titleFont->forget();
    bodyFont->forget();
    linkFont->forget();
    smallFont->forget();
    btnFont->forget();
    sectionFont->forget();
}

CMouseEventResult SetupView::onMouseDown(CPoint &where, const CButtonState &buttons) {
    if (!(buttons & kLButton))
        return kMouseEventNotHandled;

    CRect bounds = getViewSize();
    CPoint local = where;
    local.offset(-bounds.left, -bounds.top);

    if (importBtnRect_.pointInside(local)) {
        if (importCb_)
            importCb_();
        return kMouseEventHandled;
    }

    if (urlLinkRect_.pointInside(local)) {
        openURL("http://www.audionerdz.nl/download.htm");
        return kMouseEventHandled;
    }

    if (openFolderRect_.pointInside(local)) {
        openFolder(ThemeManager::getThemesDir());
        return kMouseEventHandled;
    }

    if (!builtInThemes_.empty() && builtInLinkRect_.pointInside(local)) {
        if (builtInCb_)
            builtInCb_(builtInThemes_.front().path);
        return kMouseEventHandled;
    }

    return kMouseEventNotHandled;
}

CMouseEventResult SetupView::onMouseMoved(CPoint &where, const CButtonState & /*buttons*/) {
    CRect bounds = getViewSize();
    CPoint local = where;
    local.offset(-bounds.left, -bounds.top);

    auto *frame = getFrame();
    if (frame) {
        if (importBtnRect_.pointInside(local) || urlLinkRect_.pointInside(local) ||
            openFolderRect_.pointInside(local) ||
            (!builtInThemes_.empty() && builtInLinkRect_.pointInside(local)))
            frame->setCursor(kCursorHand);
        else
            frame->setCursor(kCursorDefault);
    }
    return kMouseEventHandled;
}

CMouseEventResult SetupView::onMouseExited(CPoint & /*where*/, const CButtonState & /*buttons*/) {
    auto *frame = getFrame();
    if (frame)
        frame->setCursor(kCursorDefault);
    return kMouseEventHandled;
}

} // namespace MonkSynth
