#include "NineController.h"
#include "base/source/fstreamer.h"
#include "pluginterfaces/vst/ivstparameterchanges.h"
#include "public.sdk/source/vst/vstparameters.h"

using namespace Steinberg;
using namespace Steinberg::Vst;

// ---------------------------------------------------------------------------
// Plugin controller UID – defined in NineIDs.h, assigned here
// ---------------------------------------------------------------------------
const FUID NineController::uid = NineControllerUID;

// ---------------------------------------------------------------------------
// Parameter table: {name, default normalised value}
// Order MUST match NineParamID enum in NineIDs.h
// ---------------------------------------------------------------------------
namespace {
struct ParamDef { const TChar* name; ParamValue def; };
static const ParamDef kParamDefs[kNumParams] = {
    // Bass Drum
    { STR16("BD Tune"),      0.5 },
    { STR16("BD Decay"),     0.5 },
    { STR16("BD Attack"),    0.5 },
    { STR16("BD Level"),     0.8 },
    // Snare
    { STR16("SD Tune"),      0.5 },
    { STR16("SD Decay"),     0.5 },
    { STR16("SD Snappy"),    0.5 },
    { STR16("SD Level"),     0.8 },
    // Hi Tom
    { STR16("Hi Tom Tune"),  0.6 },
    { STR16("Hi Tom Decay"), 0.4 },
    { STR16("Hi Tom Level"), 0.8 },
    // Mid Tom
    { STR16("Mid Tom Tune"),  0.5 },
    { STR16("Mid Tom Decay"), 0.5 },
    { STR16("Mid Tom Level"), 0.8 },
    // Lo Tom
    { STR16("Lo Tom Tune"),  0.4 },
    { STR16("Lo Tom Decay"), 0.6 },
    { STR16("Lo Tom Level"), 0.8 },
    // Rim Shot
    { STR16("RS Level"),     0.8 },
    // Clap
    { STR16("CP Decay"),     0.5 },
    { STR16("CP Level"),     0.8 },
};
} // anonymous namespace

// ---------------------------------------------------------------------------
// initialize – add all parameters to the ParameterContainer
// ---------------------------------------------------------------------------
tresult PLUGIN_API NineController::initialize(FUnknown* context)
{
    tresult result = EditController::initialize(context);
    if (result != kResultTrue)
        return result;

    for (int32 i = 0; i < kNumParams; ++i)
    {
        // RangeParameter: 0..1 range, continuous (stepCount = 0), automatable
        parameters.addParameter(
            new RangeParameter(
                kParamDefs[i].name,         // title
                static_cast<ParamID>(i),    // tag
                nullptr,                    // units (dimensionless)
                0.0,                        // minPlain
                1.0,                        // maxPlain
                kParamDefs[i].def,          // defaultValuePlain
                0,                          // stepCount (0 = continuous)
                ParameterInfo::kCanAutomate
            )
        );
    }

    return kResultTrue;
}

// ---------------------------------------------------------------------------
// setComponentState – read the processor's binary state, update our params
// State format: [int8 version][float × kNumParams] – same as NineProcessor
// ---------------------------------------------------------------------------
tresult PLUGIN_API NineController::setComponentState(IBStream* state)
{
    if (!state)
        return kResultFalse;

    IBStreamer s(state, kLittleEndian);
    int8 version = 0;
    if (!s.readInt8(version) || version < 1)
        return kResultFalse;

    for (int32 i = 0; i < kNumParams; ++i)
    {
        float v = static_cast<float>(kParamDefs[i].def);
        s.readFloat(v); // tolerate short streams (older presets)
        setParamNormalized(static_cast<ParamID>(i), static_cast<ParamValue>(v));
    }

    return kResultTrue;
}

// ---------------------------------------------------------------------------
// createView – no custom GUI; host uses its generic parameter display
// ---------------------------------------------------------------------------
IPlugView* PLUGIN_API NineController::createView(FIDString /*name*/)
{
    return nullptr;
}
