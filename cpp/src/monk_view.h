#pragma once

#include "vstgui/vstgui.h"

namespace VSTGUI {
class CVSTGUITimer;
}

namespace MonkSynth {

// Custom CView that draws the monk animation from a sprite sheet.
// The monk-strip.png contains a grid of 311x311 frames arranged in
// 5 columns x 6 rows (30 frames total).
//
// When a note is active, frame selection is driven by the vowel
// parameter across columns 1-4 (frames 6-29). When idle, a state
// machine plays the idle animation:
//   HOLD  — frame 5 for ~4.8s, with two blinks to frame 2
//   SHUFFLE — loop through the 24-step vowel sequence at ~200ms/step
class MonkView : public VSTGUI::CView {
  public:
    MonkView(const VSTGUI::CRect &size);
    ~MonkView() noexcept override;

    void draw(VSTGUI::CDrawContext *context) override;
    bool attached(VSTGUI::CView *parent) override;
    bool removed(VSTGUI::CView *parent) override;

    void setMonkBitmap(VSTGUI::CBitmap *bmp);
    void setVowelValue(float v);
    void setNoteActive(bool active);

  private:
    void onIdleTick();
    void drawFrame(VSTGUI::CDrawContext *context, int frameIndex);

    VSTGUI::SharedPointer<VSTGUI::CBitmap> monkBitmap_;
    VSTGUI::SharedPointer<VSTGUI::CVSTGUITimer> idleTimer_;
    float vowelValue_ = 0.0f;
    bool noteActive_ = false;

    // Idle animation state machine
    enum IdlePhase { kHold, kShuffle };
    IdlePhase idlePhase_ = kHold;
    int phaseTickCount_ = 0;
    int shuffleSeqPos_ = 0;
    int currentIdleFrame_ = 5;

    static constexpr int kFrameWidth = 311;
    static constexpr int kFrameHeight = 311;
    static constexpr int kColumnWidth = 314;
    static constexpr int kRowsPerColumn = 6;
    static constexpr int kNumColumns = 5;
    static constexpr int kTotalFrames = kNumColumns * kRowsPerColumn;

    static constexpr int kTickMs = 100;

    // HOLD phase: frame 5 for ~48 ticks (4.8s), with two blinks
    // Blink timing: frame 2 at ~1.45s and ~3.1s,
    // each lasting ~3 ticks (300ms)
    static constexpr int kHoldTicks = 48;
    static constexpr int kHoldFrame = 5;
    static constexpr int kBlinkFrame = 2;
    static constexpr int kBlink1Start = 14; // ~1.4s
    static constexpr int kBlink1End = 17;   // ~1.7s
    static constexpr int kBlink2Start = 31; // ~3.1s
    static constexpr int kBlink2End = 34;   // ~3.4s

    // SHUFFLE: 24-step idle vowel sequence (frame indices)
    static constexpr int kShuffleSeqLen = 24;
    static constexpr int kShuffleSeq[kShuffleSeqLen] = {
        5, 3, 4, 3, 2, 1, 0, 1, // group 1
        5, 3, 4, 3, 5, 1, 0, 1, // group 2
        2, 3, 4, 3, 5, 1, 0, 1, // group 3
    };
    // ~200ms per step = 2 ticks
    static constexpr int kShuffleTicksPerStep = 2;
};

} // namespace MonkSynth
