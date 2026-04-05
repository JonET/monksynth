#include "monk_view.h"
#include "vstgui/lib/cvstguitimer.h"

using namespace VSTGUI;

namespace MonkSynth {

MonkView::MonkView(const CRect &size) : CView(size) { setTransparency(true); }

MonkView::~MonkView() noexcept {
    if (idleTimer_)
        idleTimer_->stop();
}

bool MonkView::attached(CView *parent) {
    if (CView::attached(parent)) {
        idleTimer_ =
            makeOwned<CVSTGUITimer>([this](CVSTGUITimer *) { onIdleTick(); }, kTickMs, true);
        return true;
    }
    return false;
}

bool MonkView::removed(CView *parent) {
    if (idleTimer_) {
        idleTimer_->stop();
        idleTimer_ = nullptr;
    }
    return CView::removed(parent);
}

void MonkView::setMonkBitmap(CBitmap *bmp) {
    monkBitmap_ = bmp;
    invalid();
}

void MonkView::setVowelValue(float v) {
    if (v < 0.0f)
        v = 0.0f;
    if (v > 1.0f)
        v = 1.0f;
    if (vowelValue_ != v) {
        vowelValue_ = v;
        if (noteActive_)
            invalid();
    }
}

void MonkView::setNoteActive(bool active) {
    if (noteActive_ != active) {
        noteActive_ = active;
        if (!active) {
            // Note ended — restart idle from HOLD at frame 5
            idlePhase_ = kHold;
            phaseTickCount_ = 0;
            currentIdleFrame_ = kHoldFrame;
        }
        invalid();
    }
}

void MonkView::onIdleTick() {
    if (noteActive_)
        return;

    int prevFrame = currentIdleFrame_;
    phaseTickCount_++;

    switch (idlePhase_) {

    case kHold:
        // Default to hold frame, override during blink windows
        if ((phaseTickCount_ >= kBlink1Start && phaseTickCount_ < kBlink1End) ||
            (phaseTickCount_ >= kBlink2Start && phaseTickCount_ < kBlink2End)) {
            currentIdleFrame_ = kBlinkFrame;
        } else {
            currentIdleFrame_ = kHoldFrame;
        }

        if (phaseTickCount_ >= kHoldTicks) {
            // Transition to shuffle
            idlePhase_ = kShuffle;
            phaseTickCount_ = 0;
            shuffleSeqPos_ = 0;
            currentIdleFrame_ = kShuffleSeq[0];
        }
        break;

    case kShuffle:
        if (phaseTickCount_ % kShuffleTicksPerStep == 0) {
            shuffleSeqPos_ = (shuffleSeqPos_ + 1) % kShuffleSeqLen;
            currentIdleFrame_ = kShuffleSeq[shuffleSeqPos_];
        }
        break;
    }

    if (currentIdleFrame_ != prevFrame)
        invalid();
}

void MonkView::drawFrame(CDrawContext *context, int frameIndex) {
    if (!monkBitmap_)
        return;

    if (frameIndex < 0)
        frameIndex = 0;
    if (frameIndex >= kTotalFrames)
        frameIndex = kTotalFrames - 1;

    int column = frameIndex / kRowsPerColumn;
    int row = frameIndex % kRowsPerColumn;
    CCoord srcX = column * kColumnWidth;
    CCoord srcY = row * kFrameHeight;

    CRect destRect = getViewSize();
    monkBitmap_->draw(context, destRect, CPoint(srcX, srcY));
}

void MonkView::draw(CDrawContext *context) {
    if (!monkBitmap_) {
        CView::draw(context);
        return;
    }

    if (noteActive_) {
        // Vowel-driven: map across columns 1-4 only (frames 6-29)
        int frameIndex = 6 + static_cast<int>(vowelValue_ * 23.0f + 0.5f);
        drawFrame(context, frameIndex);
    } else {
        // Idle animation: column 0 frames only
        drawFrame(context, currentIdleFrame_);
    }

    setDirty(false);
}

} // namespace MonkSynth
