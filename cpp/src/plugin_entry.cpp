#include "plugin_cids.h"
#include "processor.h"
#include "controller.h"
#include "version.h"

#include "public.sdk/source/main/pluginfactory.h"

BEGIN_FACTORY_DEF(
    "MonkSynth",  // vendor
    "https://github.com/jonet",  // URL
    "mailto:")  // email

    DEF_CLASS2(
        INLINE_UID_FROM_FUID(MonkSynth::kProcessorUID),
        PClassInfo::kManyInstances,
        kVstAudioEffectClass,
        "MonkSynth",
        Steinberg::Vst::kDistributable,
        Steinberg::Vst::PlugType::kInstrumentSynth,
        MONKSYNTH_VERSION,
        kVstVersionString,
        MonkSynth::Processor::createInstance)

    DEF_CLASS2(
        INLINE_UID_FROM_FUID(MonkSynth::kControllerUID),
        PClassInfo::kManyInstances,
        kVstComponentControllerClass,
        "MonkSynth Controller",
        0,
        "",
        MONKSYNTH_VERSION,
        kVstVersionString,
        MonkSynth::Controller::createInstance)

END_FACTORY
