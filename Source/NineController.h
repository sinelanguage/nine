#pragma once
/**
 * NineController – Steinberg VST3 EditController implementation.
 *
 * Declares all 20 automatable parameters (one per TR909 control knob)
 * and synchronises state with the NineProcessor via IBStream.
 *
 * The host uses the generic parameter GUI; createView() returns nullptr
 * so no custom IPlugView implementation is needed.
 */

#pragma once
#include "public.sdk/source/vst/vsteditcontroller.h"
#include "NineIDs.h"

class NineController : public Steinberg::Vst::EditController
{
public:
    static const Steinberg::FUID uid;

    NineController()           = default;
    ~NineController() override = default;

    static Steinberg::FUnknown* createInstance(void*) {
        return static_cast<Steinberg::Vst::IEditController*>(new NineController());
    }

    // Declare all parameters and their defaults
    Steinberg::tresult PLUGIN_API initialize(Steinberg::FUnknown* context) override;

    // Read processor state so the controller knows the current parameter values
    Steinberg::tresult PLUGIN_API setComponentState(Steinberg::IBStream* state) override;

    // Return nullptr – host uses its built-in generic parameter display
    Steinberg::IPlugView* PLUGIN_API createView(Steinberg::FIDString name) override;
};
