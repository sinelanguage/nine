/**
 * NineFactory.cpp – VST3 plugin factory entry point.
 *
 * The macros from pluginfactory.h generate the GetPluginFactory() function
 * that is the sole DLL export required by the VST3 SDK.  The host calls it
 * once at load time to discover the available classes (Processor + Controller).
 */

#include "public.sdk/source/main/pluginfactory.h"
#include "NineProcessor.h"
#include "NineController.h"

using namespace Steinberg;
using namespace Steinberg::Vst;

BEGIN_FACTORY_DEF(
    "nine",                                    // vendor name
    "https://github.com/sinelanguage/nine",    // vendor URL
    ""                                         // vendor e-mail
)

// ---------- Processor (audio + MIDI) ----------
DEF_CLASS2(
    INLINE_UID_FROM_FUID(NineProcessor::uid),
    PClassInfo::kManyInstances,
    kVstAudioEffectClass,
    "nine",                     // class name (plugin display name)
    kDistributable,             // can run processor/controller on different machines
    "Instrument|Drums",         // VST3 sub-category string
    "1.0.0",                    // plug-in version
    kVstVersionString,          // SDK version
    NineProcessor::createInstance
)

// ---------- Controller (parameter + UI) ----------
DEF_CLASS2(
    INLINE_UID_FROM_FUID(NineController::uid),
    PClassInfo::kManyInstances,
    kVstComponentControllerClass,
    "nine Controller",
    0,                          // flags (unused for controllers)
    "",                         // sub-category (unused for controllers)
    "1.0.0",
    kVstVersionString,
    NineController::createInstance
)

END_FACTORY
