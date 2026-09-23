#include "theme_browser_view.h"
#include "gallery_ui.h"
#include "i18n.h"
#include "open_url.h"
#include "theme_manager.h"

#include "vstgui/lib/cdrawcontext.h"
#include "vstgui/lib/cfont.h"
#include "vstgui/lib/cframe.h"
#include "vstgui/lib/cgraphicspath.h"
#include "vstgui/lib/events.h"
#include "vstgui/lib/platform/platformfactory.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

using namespace VSTGUI;
using namespace MonkSynth::gallery_ui;
namespace fs = std::filesystem;

namespace MonkSynth {

namespace {

// Layout, in view coordinates (the view is the 360x510 editor).
constexpr CCoord kHeaderH = 60;
constexpr CCoord kFooterH = 132;
constexpr CCoord kPad = 12;
constexpr CCoord kRowH = 72;
constexpr CCoord kRowGap = 4;
constexpr CCoord kThumb = 56;
constexpr CCoord kCardGap = 12;
constexpr CCoord kCardH = 160;
constexpr CCoord kMessageH = 96;

const CColor kRowHover(30, 27, 24);
const CColor kIcon(201, 193, 181);

} // namespace

ThemeBrowserView::ThemeBrowserView(const CRect &size, ThemeGallery *gallery,
                                   ActiveThemeFn activeTheme, ApplyFn onApply)
    : OverlayView(size, false), gallery_(gallery), activeTheme_(std::move(activeTheme)),
      onApply_(std::move(onApply)) {
    sync();
    if (snap_.state == ThemeGallery::IndexState::Idle ||
        snap_.state == ThemeGallery::IndexState::Failed || snap_.themes.empty())
        gallery_->refresh();
    timer_ = makeOwned<CVSTGUITimer>([this](CVSTGUITimer *) { poll(); }, 100);
}

ThemeBrowserView::~ThemeBrowserView() {
    if (timer_)
        timer_->stop();
}

bool ThemeBrowserView::removed(CView *parent) {
    if (timer_) {
        timer_->stop();
        timer_ = nullptr;
    }
    return OverlayView::removed(parent);
}

// --- State ---

void ThemeBrowserView::poll() {
    if (gallery_->revision() != seenRevision_) {
        sync();
        invalid();
    }
    // A theme updated while in use: switch to the new files. This rebuilds
    // the editor, which closes the browser, so do it last.
    for (const auto &id : gallery_->takeFinishedInstalls()) {
        fs::path dir = ThemeGallery::installDir(id);
        std::error_code ec;
        fs::path active = activeTheme_ ? activeTheme_() : fs::path();
        if (!active.empty() && fs::equivalent(dir, active, ec)) {
            if (onApply_)
                onApply_(dir, false);
            return;
        }
    }
}

void ThemeBrowserView::sync() {
    seenRevision_ = gallery_->revision();
    snap_ = gallery_->snapshot();
    fs::path active = activeTheme_ ? activeTheme_() : fs::path();
    states_.clear();
    for (const auto &t : snap_.themes)
        states_[t.id] = themeState(t, snap_.status[t.id], active);
    // Default the selection to the theme in use, else the first one.
    bool known = std::any_of(snap_.themes.begin(), snap_.themes.end(),
                             [&](const GalleryTheme &t) { return t.id == selectedId_; });
    if (!known && !snap_.themes.empty()) {
        selectedId_ = snap_.themes.front().id;
        for (const auto &t : snap_.themes)
            if (states_[t.id].active)
                selectedId_ = t.id;
    }
}

CBitmap *ThemeBrowserView::thumbFor(const std::string &id) {
    auto it = thumbs_.find(id);
    if (it != thumbs_.end())
        return it->second;
    auto st = snap_.status.find(id);
    if (st == snap_.status.end())
        return nullptr;
    // Thumbnails are 112 px, drawn at 56 pt.
    auto bmp = loadImage(st->second.thumbPath, 2.0);
    if (bmp)
        thumbs_[id] = bmp;
    return bmp;
}

const GalleryTheme *ThemeBrowserView::selectedTheme() const {
    for (const auto &t : snap_.themes)
        if (t.id == selectedId_)
            return &t;
    return nullptr;
}

// --- Drawing ---

void ThemeBrowserView::drawBody(CDrawContext *ctx, const CRect &bounds) {
    hits_.clear();
    ctx->setFillColor(kBg);
    ctx->drawRect(bounds, kDrawFilled);

    drawHeader(ctx, bounds);
    CRect list(bounds.left, bounds.top + kHeaderH, bounds.right, bounds.bottom - kFooterH);
    listArea_ = list;
    listArea_.offset(-bounds.left, -bounds.top);
    drawList(ctx, list);
    drawFooter(ctx, CRect(bounds.left, bounds.bottom - kFooterH, bounds.right, bounds.bottom));
}

void ThemeBrowserView::drawHeader(CDrawContext *ctx, const CRect &bounds) {
    CPoint o = bounds.getTopLeft();
    ctx->setFillColor(kLine);
    ctx->drawRect(CRect(bounds.left, bounds.top + kHeaderH - 1, bounds.right, bounds.top + kHeaderH),
                  kDrawFilled);

    CRect back(4, 8, 48, 52);
    CRect folder(bounds.getWidth() - 52, 8, bounds.getWidth() - 8, 52);
    for (const Hit &h : {Hit{back, Action::Close, {}}, Hit{folder, Action::OpenFolder, {}}}) {
        hits_.push_back(h);
        if (hoverAction_ == h.action) {
            CRect r = h.rect;
            r.offset(o.x, o.y);
            fillRound(ctx, r, 10, kQuiet);
        }
    }

    ctx->setFrameColor(kIcon);
    ctx->setLineWidth(2);
    ctx->setLineStyle(CLineStyle(CLineStyle::kLineCapRound, CLineStyle::kLineJoinRound));
    if (auto *path = ctx->createGraphicsPath()) {
        CPoint c(o.x + back.getCenter().x, o.y + back.getCenter().y);
        path->beginSubpath(CPoint(c.x + 3, c.y - 7));
        path->addLine(CPoint(c.x - 4, c.y));
        path->addLine(CPoint(c.x + 3, c.y + 7));
        ctx->drawGraphicsPath(path, CDrawContext::kPathStroked);
        path->forget();
    }
    if (auto *path = ctx->createGraphicsPath()) {
        CPoint c(o.x + folder.getCenter().x, o.y + folder.getCenter().y);
        path->beginSubpath(CPoint(c.x - 9, c.y - 6));
        path->addLine(CPoint(c.x - 3, c.y - 6));
        path->addLine(CPoint(c.x - 1, c.y - 4));
        path->addLine(CPoint(c.x + 9, c.y - 4));
        path->addLine(CPoint(c.x + 9, c.y + 7));
        path->addLine(CPoint(c.x - 9, c.y + 7));
        path->closeSubpath();
        ctx->drawGraphicsPath(path, CDrawContext::kPathStroked);
        path->forget();
    }
    ctx->setLineStyle(kLineSolid);
    ctx->setLineWidth(1);

    auto title = font(18, true);
    text(ctx, i18n::str(i18n::StringId::GalleryTitle),
         CRect(o.x + 52, o.y, o.x + folder.left - 8, o.y + kHeaderH), kText, title);
}

void ThemeBrowserView::drawList(CDrawContext *ctx, const CRect &area) {
    CPoint o(area.left, area.top - kHeaderH); // view origin
    CRect inner(area.left + kPad, 0, area.right - kPad, 0);
    CCoord y = area.top + kPad - scrollY_;

    auto clickable = [&](CRect r, Action action, const std::string &id) {
        r.bound(area);
        if (r.isEmpty())
            return;
        r.offset(-o.x, -o.y);
        hits_.push_back({r, action, id});
    };

    ConcatClip clip(*ctx, area);
    auto nameFont = font(14, true);
    auto smallFont = font(12);
    auto pillFont = font(12, true);

    bool loading = snap_.state == ThemeGallery::IndexState::Loading ||
                   snap_.state == ThemeGallery::IndexState::Idle;
    if (snap_.themes.empty()) {
        CRect box(inner.left, y, inner.right, y + kMessageH);
        if (loading) {
            text(ctx, i18n::str(i18n::StringId::GalleryLoading), box, kMuted, smallFont,
                 kCenterText);
        } else {
            CRect msg(box.left, box.top + 12, box.right, box.top + 32);
            text(ctx, i18n::str(i18n::StringId::GalleryOffline), msg, kText, smallFont,
                 kCenterText);
            if (!snap_.indexError.empty()) {
                CRect why(box.left, msg.bottom, box.right, msg.bottom + 18);
                ctx->setFont(smallFont);
                text(ctx, ellipsize(ctx, snap_.indexError, why.getWidth()), why, kMuted,
                     smallFont, kCenterText);
            }
            std::string retry = i18n::str(i18n::StringId::GalleryRetry);
            CCoord w = buttonWidth(ctx, retry, 120);
            CRect btn(box.getCenter().x - w / 2, box.bottom - 40, box.getCenter().x + w / 2,
                      box.bottom);
            button(ctx, btn, retry, Style::Ghost, hoverAction_ == Action::Retry);
            clickable(btn, Action::Retry, {});
        }
        y += kMessageH;
    }

    for (const auto &t : snap_.themes) {
        CRect row(inner.left, y, inner.right, y + kRowH);
        y += kRowH + kRowGap;
        if (!row.rectOverlap(area))
            continue;
        clickable(row, Action::Select, t.id);

        const ThemeState &state = states_[t.id];
        bool selected = t.id == selectedId_;
        if (selected) {
            fillRound(ctx, row, 12, kRowSelected);
            strokeRound(ctx, row, 12, kAccent);
        } else if (hoverAction_ == Action::Select && hoverId_ == t.id) {
            fillRound(ctx, row, 12, kRowHover);
        }

        CRect thumb(row.left + 8, row.top + 8, row.left + 8 + kThumb, row.top + 8 + kThumb);
        fillRound(ctx, thumb, 8, kStage);
        if (auto *bmp = thumbFor(t.id))
            ctx->drawBitmap(bmp, thumb);

        CRect pillRect = drawPill(ctx, pillFor(state), row.right - 8, row.getCenter().y);
        bool update = state.update;
        bool installed = state.copy.installed();

        CCoord tx = thumb.right + 12;
        CCoord tw = pillRect.left - 8 - tx;
        ctx->setFont(nameFont);
        text(ctx, ellipsize(ctx, t.name, tw), CRect(tx, row.top + 15, tx + tw, row.top + 35),
             kText, nameFont);
        std::string sub;
        if (!t.author.empty())
            sub = std::string(i18n::str(i18n::StringId::GalleryBy)) + t.author;
        if (!installed || update)
            sub += (sub.empty() ? "" : " \xC2\xB7 ") + formatSize(t.size);
        ctx->setFont(smallFont);
        text(ctx, ellipsize(ctx, sub, tw), CRect(tx, row.top + 37, tx + tw, row.top + 55), kMuted,
             smallFont);
    }

    // "Make your own theme" card, last in the list.
    y += kCardGap - kRowGap;
    CRect card(inner.left, y, inner.right, y + kCardH);
    y += kCardH + kPad;
    if (card.rectOverlap(area)) {
        strokeRound(ctx, card, 12, kBorder, true);
        CRect c(card.left + 16, card.top + 16, card.right - 16, card.bottom - 16);
        auto cardTitle = font(17, true);
        text(ctx, i18n::str(i18n::StringId::GalleryMakeOwnTitle),
             CRect(c.left, c.top, c.right, c.top + 22), kText, cardTitle);
        ctx->setFont(smallFont);
        auto lines = wrapText(ctx, i18n::str(i18n::StringId::GalleryMakeOwnBody), c.getWidth(), 3);
        CCoord ly = c.top + 28;
        for (const auto &line : lines) {
            text(ctx, line, CRect(c.left, ly, c.right, ly + 16), kMuted, smallFont);
            ly += 17;
        }
        CCoord bw = (c.getWidth() - 8) / 2;
        CRect spec(c.left, c.bottom - 40, c.left + bw, c.bottom);
        CRect submit(spec.right + 8, c.bottom - 40, c.right, c.bottom);
        button(ctx, spec, i18n::str(i18n::StringId::GalleryThemeSpec), Style::Ghost,
               hoverAction_ == Action::Spec);
        button(ctx, submit, i18n::str(i18n::StringId::GallerySubmit), Style::Ghost,
               hoverAction_ == Action::Submit);
        clickable(spec, Action::Spec, {});
        clickable(submit, Action::Submit, {});
    }

    // Content height and a slim scroll indicator.
    CCoord content = y + scrollY_ - area.top;
    maxScroll_ = std::max<CCoord>(0, content - area.getHeight());
    if (scrollY_ > maxScroll_)
        scrollY_ = maxScroll_;
    if (maxScroll_ > 0) {
        CCoord track = area.getHeight() - 8;
        CCoord h = std::max<CCoord>(24, track * area.getHeight() / content);
        CCoord ty = area.top + 4 + (track - h) * (scrollY_ / maxScroll_);
        fillRound(ctx, CRect(area.right - 6, ty, area.right - 2, ty + h), 2,
                  CColor(255, 255, 255, 40));
    }
}

void ThemeBrowserView::drawFooter(CDrawContext *ctx, const CRect &area) {
    CPoint o(area.left, area.bottom - getViewSize().getHeight());
    ctx->setFillColor(kLine);
    ctx->drawRect(CRect(area.left, area.top, area.right, area.top + 1), kDrawFilled);

    const GalleryTheme *t = selectedTheme();
    if (!t)
        return;
    const ThemeState &state = states_[t->id];
    const auto &status = snap_.status[t->id];
    bool active = state.active;
    bool update = state.update;

    CRect c(area.left + 16, area.top + 12, area.right - 16, area.bottom - 14);
    auto nameFont = font(14, true);
    auto smallFont = font(12);

    std::string version = t->version.empty() ? "" : "v" + t->version;
    ctx->setFont(smallFont);
    CCoord vw = version.empty() ? 0 : std::ceil(ctx->getStringWidth(version.c_str())) + 8;
    text(ctx, version, CRect(c.right - vw, c.top, c.right, c.top + 20), kMuted, smallFont,
         kRightText);
    ctx->setFont(nameFont);
    text(ctx, ellipsize(ctx, t->name, c.getWidth() - vw), CRect(c.left, c.top, c.right - vw, c.top + 20),
         kText, nameFont);

    bool isError = !status.error.empty();
    std::string desc = isError ? status.error
                               : (t->description.empty()
                                      ? i18n::str(i18n::StringId::GalleryNoDescription)
                                      : t->description);
    ctx->setFont(smallFont);
    CCoord ly = c.top + 23;
    for (const auto &line : wrapText(ctx, desc, c.getWidth(), 2)) {
        text(ctx, line, CRect(c.left, ly, c.right, ly + 16), isError ? kError : kMuted, smallFont);
        ly += 17;
    }

    CRect row(c.left, c.bottom - 44, c.right, c.bottom);
    auto addHit = [&](CRect r, Action a) {
        r.offset(-o.x, -o.y);
        hits_.push_back({r, a, t->id});
    };
    auto hovered = [&](Action a) { return hoverAction_ == a; };

    if (status.installing) {
        progressBar(ctx, row, status.progress);
        return;
    }

    std::string sizeSuffix = " \xC2\xB7 " + formatSize(t->size);
    if (!state.copy.installed()) {
        button(ctx, row, i18n::str(i18n::StringId::GalleryDownload) + sizeSuffix, Style::Primary,
               hovered(Action::Download));
        addHit(row, Action::Download);
        return;
    }
    if (active) {
        if (update) {
            button(ctx, row, i18n::str(i18n::StringId::GalleryUpdate) + sizeSuffix, Style::Primary,
                   hovered(Action::Download));
            addHit(row, Action::Download);
        } else {
            button(ctx, row, i18n::str(i18n::StringId::GalleryCurrent), Style::Quiet, false);
        }
        return;
    }

    // Installed, not in use: Apply, plus Update or Remove beside it.
    std::string second;
    Action secondAction = Action::None;
    if (update) {
        second = i18n::str(i18n::StringId::GalleryUpdate);
        secondAction = Action::Download;
    } else if (state.copy.user) {
        second = i18n::str(i18n::StringId::GalleryRemove);
        secondAction = Action::Remove;
    }
    CRect apply(row);
    if (secondAction != Action::None) {
        CCoord w = buttonWidth(ctx, second, 96);
        CRect r(row.right - w, row.top, row.right, row.bottom);
        apply.right = r.left - 8;
        button(ctx, r, second, Style::Ghost, hovered(secondAction));
        addHit(r, secondAction);
    }
    button(ctx, apply, i18n::str(i18n::StringId::GalleryApply), Style::Primary,
           hovered(Action::Apply));
    addHit(apply, Action::Apply);
}

// --- Input ---

const ThemeBrowserView::Hit *ThemeBrowserView::hitAt(const CPoint &local) const {
    // Later hits (footer, overlays) win over earlier ones.
    for (auto it = hits_.rbegin(); it != hits_.rend(); ++it)
        if (it->rect.pointInside(local))
            return &*it;
    return nullptr;
}

bool ThemeBrowserView::hitLink(const CPoint &local, bool click) {
    const Hit *hit = hitAt(local);
    if (hit && click)
        perform(*hit);
    return hit != nullptr;
}

void ThemeBrowserView::perform(const Hit &hit) {
    switch (hit.action) {
    case Action::None:
        break;
    case Action::Close:
        requestClose();
        break;
    case Action::OpenFolder:
        openFolder(ThemeManager::getThemesDir());
        break;
    case Action::Retry:
        gallery_->refresh();
        break;
    case Action::Select:
        selectedId_ = hit.id;
        invalid();
        break;
    case Action::Download:
        gallery_->install(hit.id);
        break;
    case Action::Apply: {
        auto copy = ThemeGallery::installedCopy(hit.id);
        if (copy.installed() && onApply_)
            onApply_(copy.path, !copy.user);
        break;
    }
    case Action::Remove: {
        // Only ever a folder directly inside the user themes dir, named by a
        // validated gallery id, and never the theme in use.
        auto copy = ThemeGallery::installedCopy(hit.id);
        fs::path active = activeTheme_ ? activeTheme_() : fs::path();
        if (copy.user && !ThemeGallery::isActive(copy, active)) {
            std::error_code ec;
            fs::remove_all(ThemeGallery::installDir(hit.id), ec);
            sync();
            invalid();
        }
        break;
    }
    case Action::Spec:
        openURL(std::string(ThemeGallery::kRepoUrl) + "#what-goes-in-a-theme-folder");
        break;
    case Action::Submit:
        openURL(std::string(ThemeGallery::kRepoUrl) + "#submitting-a-theme");
        break;
    }
}

CMouseEventResult ThemeBrowserView::onMouseDown(CPoint &where, const CButtonState &buttons) {
    if (!(buttons & kLButton))
        return kMouseEventNotHandled;
    CPoint local = where;
    local.offset(-getViewSize().left, -getViewSize().top);
    hitLink(local, true);
    return kMouseEventHandled; // modal: nothing reaches the editor underneath
}

CMouseEventResult ThemeBrowserView::onMouseMoved(CPoint &where, const CButtonState & /*buttons*/) {
    CPoint local = where;
    local.offset(-getViewSize().left, -getViewSize().top);
    const Hit *hit = hitAt(local);
    Action action = hit ? hit->action : Action::None;
    std::string id = hit ? hit->id : std::string();
    if (action != hoverAction_ || id != hoverId_) {
        hoverAction_ = action;
        hoverId_ = id;
        invalid();
    }
    if (auto *frame = getFrame())
        frame->setCursor(hit ? kCursorHand : kCursorDefault);
    return kMouseEventHandled;
}

CMouseEventResult ThemeBrowserView::onMouseExited(CPoint & /*where*/,
                                                  const CButtonState & /*buttons*/) {
    if (hoverAction_ != Action::None) {
        hoverAction_ = Action::None;
        hoverId_.clear();
        invalid();
    }
    if (auto *frame = getFrame())
        frame->setCursor(kCursorDefault);
    return kMouseEventHandled;
}

void ThemeBrowserView::onMouseWheelEvent(MouseWheelEvent &event) {
    CPoint local = event.mousePosition;
    local.offset(-getViewSize().left, -getViewSize().top);
    if (!listArea_.pointInside(local) || maxScroll_ <= 0 || event.deltaY == 0.)
        return;
    // Same direction handling as VSTGUI's CScrollbar.
    CCoord delta = event.deltaY;
    if (event.flags & MouseWheelEvent::DirectionInvertedFromDevice)
        delta = -delta;
    CCoord pixels = (event.flags & MouseWheelEvent::PreciseDeltas) ? delta * 10 : delta * 40;
    CCoord next = std::clamp<CCoord>(scrollY_ - pixels, 0, maxScroll_);
    if (next != scrollY_) {
        scrollY_ = next;
        invalid();
    }
    event.consumed = true;
}

} // namespace MonkSynth
