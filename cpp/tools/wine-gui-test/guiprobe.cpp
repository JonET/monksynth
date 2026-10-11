// guiprobe: the smallest VST3 host that can open MonkSynth's editor.
//
// It loads only the edit controller (no audio processing), opens the editor in
// a top-level window, then drives the same redraw path a DAW does: note on,
// then vowel and pitch bend sweeps via setParamNormalized. It prints PHASE and
// RECT lines on stdout so wine-gui-test can screenshot the right moments and
// crop to the editor.
//
// By default it opens two instances side by side, like two tracks in a DAW.
// They share the plugin module and so VSTGUI's global platform factory.
//
//   guiprobe <MonkSynth.vst3> [instances]
//
// Built with mingw-w64 inside the wine-gui-test container, against the VST3
// SDK's pluginterfaces headers only.

#include "pluginterfaces/base/ipluginbase.h"
#include "pluginterfaces/gui/iplugview.h"
#include "pluginterfaces/vst/ivsteditcontroller.h"
#include "pluginterfaces/vst/ivsthostapplication.h"

#include <ole2.h>
#include <windows.h>

#include <atomic>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

using namespace Steinberg;
using namespace Steinberg::Vst;

DEF_CLASS_IID(IPlugView)
DEF_CLASS_IID(IPlugFrame)
DEF_CLASS_IID(IEditController)
DEF_CLASS_IID(IComponentHandler)
DEF_CLASS_IID(IHostApplication)

namespace {

// Must match cpp/src/plugin_cids.h.
const TUID kControllerUID = INLINE_UID(0x4D4E4B53, 0x4354524C, 0x4D4E4B53, 0x594E5448);
constexpr ParamID kVowel = 1;
constexpr ParamID kPitchBend = 19;
constexpr ParamID kNoteActive = 200;

constexpr UINT_PTR kTimerId = 1;
constexpr UINT kTickMs = 16;
constexpr DWORD kStaticMs = 3000;
constexpr DWORD kSweepMs = 10000;
constexpr DWORD kSettleMs = 5000;

void phase(const char *name) {
    std::printf("PHASE %s %lu\n", name, GetTickCount());
    std::fflush(stdout);
}

// One object serves as host context, component handler and plug frame. The
// controller never outlives main(), so reference counting is cosmetic.
class Host : public IHostApplication, public IComponentHandler, public IPlugFrame {
  public:
    HWND hwnd = nullptr;

    tresult PLUGIN_API queryInterface(const TUID iid, void **obj) override {
        if (FUnknownPrivate::iidEqual(iid, FUnknown::iid) ||
            FUnknownPrivate::iidEqual(iid, IHostApplication::iid)) {
            *obj = static_cast<IHostApplication *>(this);
        } else if (FUnknownPrivate::iidEqual(iid, IComponentHandler::iid)) {
            *obj = static_cast<IComponentHandler *>(this);
        } else if (FUnknownPrivate::iidEqual(iid, IPlugFrame::iid)) {
            *obj = static_cast<IPlugFrame *>(this);
        } else {
            *obj = nullptr;
            return kNoInterface;
        }
        addRef();
        return kResultOk;
    }
    uint32 PLUGIN_API addRef() override { return ++refs_; }
    uint32 PLUGIN_API release() override { return --refs_; }

    // IHostApplication
    tresult PLUGIN_API getName(String128 name) override {
        const char16 n[] = u"guiprobe";
        std::memcpy(name, n, sizeof(n));
        return kResultOk;
    }
    tresult PLUGIN_API createInstance(TUID, TUID, void **obj) override {
        *obj = nullptr;
        return kNotImplemented;
    }

    // IComponentHandler
    tresult PLUGIN_API beginEdit(ParamID) override { return kResultOk; }
    tresult PLUGIN_API performEdit(ParamID, ParamValue) override { return kResultOk; }
    tresult PLUGIN_API endEdit(ParamID) override { return kResultOk; }
    tresult PLUGIN_API restartComponent(int32) override { return kResultOk; }

    // IPlugFrame
    tresult PLUGIN_API resizeView(IPlugView *view, ViewRect *r) override {
        RECT wr{0, 0, r->getWidth(), r->getHeight()};
        AdjustWindowRect(&wr, GetWindowLong(hwnd, GWL_STYLE), FALSE);
        SetWindowPos(hwnd, nullptr, 0, 0, wr.right - wr.left, wr.bottom - wr.top,
                     SWP_NOMOVE | SWP_NOZORDER);
        return view->onSize(r);
    }

  private:
    std::atomic<uint32> refs_{1};
};

struct Instance {
    Host host;
    IEditController *controller = nullptr;
    IPlugView *view = nullptr;
};

std::vector<std::unique_ptr<Instance>> instances;
DWORD startTick = 0;
int phaseIndex = -1;

void printRect(HWND hwnd) {
    RECT rc;
    GetClientRect(hwnd, &rc);
    POINT tl{rc.left, rc.top};
    ClientToScreen(hwnd, &tl);
    std::printf("RECT %ld %ld %ld %ld\n", tl.x, tl.y, rc.right - rc.left, rc.bottom - rc.top);
    std::fflush(stdout);
}

void tick() {
    // Start the clock on the first tick, not at attach: under emulation the
    // first paint can take longer than the whole static phase.
    if (phaseIndex < 0)
        startTick = GetTickCount();
    DWORD t = GetTickCount() - startTick;
    int next = t < kStaticMs                        ? 0
               : t < kStaticMs + kSweepMs           ? 1
               : t < kStaticMs + kSweepMs + kSettleMs ? 2
                                                    : 3;
    if (next != phaseIndex) {
        phaseIndex = next;
        static const char *names[] = {"static", "sweep", "settle", "done"};
        if (next == 1)
            for (auto &inst : instances)
                inst->controller->setParamNormalized(kNoteActive, 1.0);
        phase(names[next]);
        if (next == 3)
            PostQuitMessage(0);
    }
    if (phaseIndex == 1) {
        // Different periods so vowel and pitch changes land on different ticks,
        // like a hand moving the XY pad, and a phase offset per instance so the
        // editors repaint at different moments.
        double s = (t - kStaticMs) / 1000.0;
        for (size_t i = 0; i < instances.size(); ++i) {
            double o = i * 0.7;
            instances[i]->controller->setParamNormalized(kVowel, 0.5 + 0.5 * std::sin(s * 2.1 + o));
            instances[i]->controller->setParamNormalized(kPitchBend,
                                                         0.5 + 0.5 * std::sin(s * 3.3 + o));
        }
    }
}

LRESULT CALLBACK wndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
    case WM_TIMER:
        if (wp == kTimerId)
            tick();
        return 0;
    case WM_CLOSE:
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

void step(const char *what) {
    std::printf("STEP %s\n", what);
    std::fflush(stdout);
}

int fail(const char *what) {
    std::printf("FAIL %s\n", what);
    std::fflush(stdout);
    return 1;
}

// Accept either the .vst3 bundle directory or the DLL inside it.
std::wstring resolveDll(const std::wstring &arg) {
    DWORD attrs = GetFileAttributesW(arg.c_str());
    if (attrs != INVALID_FILE_ATTRIBUTES && (attrs & FILE_ATTRIBUTE_DIRECTORY)) {
        std::wstring name = arg.substr(arg.find_last_of(L"\\/") + 1);
        return arg + L"\\Contents\\x86_64-win\\" + name;
    }
    return arg;
}

} // namespace

int wmain(int argc, wchar_t **argv) {
    if (argc < 2) {
        std::printf("usage: guiprobe <MonkSynth.vst3> [instances]\n");
        return 2;
    }
    int count = argc > 2 ? _wtoi(argv[2]) : 2;
    if (count < 1)
        count = 1;

    // VSTGUI creates its WIC and Direct2D factories with CoCreateInstance when
    // the module initializes, so COM must be up first, as it is in any DAW.
    if (FAILED(OleInitialize(nullptr)))
        return fail("OleInitialize");

    std::wstring dll = resolveDll(argv[1]);
    step("LoadLibrary");
    HMODULE mod = LoadLibraryW(dll.c_str());
    if (!mod)
        return fail("LoadLibrary");

    step("InitDll");
    using BoolFn = bool (*)();
    if (auto initDll = reinterpret_cast<BoolFn>(GetProcAddress(mod, "InitDll")))
        if (!initDll())
            return fail("InitDll");

    auto getFactory = reinterpret_cast<GetFactoryProc>(GetProcAddress(mod, "GetPluginFactory"));
    if (!getFactory)
        return fail("GetPluginFactory missing");
    IPluginFactory *factory = getFactory();
    if (!factory)
        return fail("GetPluginFactory returned null");

    WNDCLASSW wc{};
    wc.lpfnWndProc = wndProc;
    wc.hInstance = GetModuleHandleW(nullptr);
    wc.lpszClassName = L"guiprobe";
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    RegisterClassW(&wc);
    const DWORD style = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU;
    int nextX = 40;

    for (int i = 0; i < count; ++i) {
        auto inst = std::make_unique<Instance>();
        Host &host = inst->host;

        step("createInstance");
        if (factory->createInstance(kControllerUID, IEditController::iid,
                                    reinterpret_cast<void **>(&inst->controller)) != kResultOk)
            return fail("create controller");
        if (inst->controller->initialize(static_cast<IHostApplication *>(&host)) != kResultOk)
            return fail("controller initialize");
        inst->controller->setComponentHandler(&host);

        step("createView");
        inst->view = inst->controller->createView(ViewType::kEditor);
        if (!inst->view)
            return fail("createView");
        if (inst->view->isPlatformTypeSupported(kPlatformTypeHWND) != kResultTrue)
            return fail("HWND not supported");

        ViewRect size;
        inst->view->getSize(&size);
        RECT wr{0, 0, size.getWidth(), size.getHeight()};
        AdjustWindowRect(&wr, style, FALSE);
        host.hwnd = CreateWindowW(L"guiprobe", L"guiprobe", style, nextX, 40, wr.right - wr.left,
                                  wr.bottom - wr.top, nullptr, nullptr, wc.hInstance, nullptr);
        if (!host.hwnd)
            return fail("CreateWindow");
        nextX += wr.right - wr.left + 40;

        step("attached");
        inst->view->setFrame(&host);
        if (inst->view->attached(host.hwnd, kPlatformTypeHWND) != kResultOk)
            return fail("attached");
        ShowWindow(host.hwnd, SW_SHOW);
        UpdateWindow(host.hwnd);
        printRect(host.hwnd);
        instances.push_back(std::move(inst));
    }

    HWND timerWindow = instances.front()->host.hwnd;
    SetTimer(timerWindow, kTimerId, kTickMs, nullptr);

    MSG msg;
    while (GetMessageW(&msg, nullptr, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    KillTimer(timerWindow, kTimerId);
    for (auto &inst : instances) {
        inst->view->removed();
        inst->view->release();
        inst->controller->setComponentHandler(nullptr);
        inst->controller->terminate();
        inst->controller->release();
        DestroyWindow(inst->host.hwnd);
    }
    instances.clear();
    if (auto exitDll = reinterpret_cast<BoolFn>(GetProcAddress(mod, "ExitDll")))
        exitDll();
    OleUninitialize();
    return 0;
}
