#pragma once
/**
 * NineIDs.h – UIDs and parameter IDs for the "nine" TR909 VST3 plugin.
 *
 * All plugin-wide identifiers are centralised here so that both the
 * Processor and the Controller can include a single header without
 * pulling in any of the SDK's heavier headers.
 *
 * The two FUIDs MUST be unique and MUST NOT change once a plugin is
 * released (hosts use them to identify saved presets).
 *
 *   NineProcessorUID   {4E696E65-54523930-39425344-544F4D52}  "NineTR909BSDTomR"
 *   NineControllerUID  {4E696E65-43544C52-39425344-4354524C}  "NineCTLR9BSDCTrL"
 */

#include "pluginterfaces/vst/vsttypes.h"

// ---------------------------------------------------------------------------
// Plugin class UIDs
// ---------------------------------------------------------------------------
static const Steinberg::FUID NineProcessorUID  (0x4E696E65, 0x54523930, 0x39425344, 0x544F4D52);
static const Steinberg::FUID NineControllerUID (0x4E696E65, 0x43544C52, 0x39425344, 0x4354524C);

// ---------------------------------------------------------------------------
// Parameter IDs  (must match order used in state serialisation)
// ---------------------------------------------------------------------------
enum NineParamID : Steinberg::Vst::ParamID
{
    // Bass Drum
    kBdTune   = 0,
    kBdDecay,
    kBdTone,
    kBdLevel,

    // Snare Drum
    kSdTune,
    kSdDecay,
    kSdSnappy,
    kSdLevel,

    // Hi Tom
    kHiTune,
    kHiDecay,
    kHiLevel,

    // Mid Tom
    kMidTune,
    kMidDecay,
    kMidLevel,

    // Lo Tom
    kLoTune,
    kLoDecay,
    kLoLevel,

    // Rim Shot
    kRsLevel,

    // Hand Clap
    kCpDecay,
    kCpLevel,

    kNumParams
};
