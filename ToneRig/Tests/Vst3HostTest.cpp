/*
    Loads the built ToneRig.vst3 bundle through the real VST3 ABI (the same way a
    DAW does) and pushes audio through it.

    This is deliberately not a JUCE test: it talks to the exported module entry
    points with dlopen/dlsym and the plain VST3 interfaces, so it verifies the
    shipped binary rather than the source.

    Usage: ToneRigVst3HostTest [path/to/ToneRig.vst3]
*/
#include <dlfcn.h>

#include <cmath>
#include <cstdio>
#include <cstring>
#include <vector>

#include "pluginterfaces/base/funknown.h"
#include "pluginterfaces/base/ipluginbase.h"
#include "pluginterfaces/gui/iplugview.h"
#include "pluginterfaces/vst/ivstaudioprocessor.h"
#include "pluginterfaces/vst/ivstcomponent.h"
#include "pluginterfaces/vst/ivsteditcontroller.h"
#include "pluginterfaces/vst/ivstmessage.h"

using namespace Steinberg;
using namespace Steinberg::Vst;

namespace
{
    int failures = 0;
    int checks   = 0;

    void check (bool condition, const char* what)
    {
        ++checks;
        if (condition)
            std::printf ("  ok   %s\n", what);
        else
        {
            ++failures;
            std::printf ("  FAIL %s\n", what);
        }
    }

    constexpr double kSampleRate = 48000.0;
    constexpr int    kBlockSize  = 512;

    bool finite (const std::vector<float>& v)
    {
        for (float x : v)
            if (! std::isfinite (x))
                return false;
        return true;
    }

    float peak (const std::vector<float>& v)
    {
        float p = 0.0f;
        for (float x : v)
            p = std::fmax (p, std::fabs (x));
        return p;
    }
}

int main (int argc, char** argv)
{
    const char* path =
#ifdef TONERIG_VST3_PATH
        argc > 1 ? argv[1] : TONERIG_VST3_PATH;
#else
        argc > 1 ? argv[1] : "build/ToneRig_artefacts/Release/VST3/ToneRig.vst3";
#endif

    std::printf ("ToneRig VST3 host test\n=========================\n");
    std::printf ("module: %s\n\n", path);

    //== 1. load the module ===================================================
    void* module = dlopen (path, RTLD_NOW | RTLD_LOCAL);
    if (module == nullptr)
    {
        std::printf ("  FAIL dlopen: %s\n", dlerror());
        return 1;
    }
    check (true, "dlopen() of the .vst3 bundle succeeded");

    auto moduleEntry = reinterpret_cast<bool (*) (void*)> (dlsym (module, "ModuleEntry"));
    auto getFactory  = reinterpret_cast<IPluginFactory* (*)()> (dlsym (module, "GetPluginFactory"));
    check (getFactory != nullptr, "module exports GetPluginFactory()");

    if (getFactory == nullptr)
        return 1;

    if (moduleEntry != nullptr)
        check (moduleEntry (module), "ModuleEntry() returned true");

    IPluginFactory* factory = getFactory();
    check (factory != nullptr, "GetPluginFactory() returned a factory");

    if (factory == nullptr)
        return 1;

    //== 2. find the registered classes =======================================
    const int32 numClasses = factory->countClasses();
    check (numClasses >= 1, "factory exposes at least one class");

    FUID audioClassUid, controllerClassUid;
    bool foundAudioModule = false, foundEditController = false;

    for (int32 i = 0; i < numClasses; ++i)
    {
        PClassInfo info {};
        if (factory->getClassInfo (i, &info) != kResultOk)
            continue;

        if (std::strcmp (info.category, kVstAudioEffectClass) == 0)
        {
            audioClassUid = FUID (info.cid);
            foundAudioModule = true;
        }
        else if (std::strcmp (info.category, kVstComponentControllerClass) == 0)
        {
            controllerClassUid = FUID (info.cid);
            foundEditController = true;
        }
    }

    check (foundAudioModule, "an 'Audio Module Class' is registered");
    check (foundEditController, "an 'Component Controller Class' is registered");

    if (! foundAudioModule)
        return 1;

    //== 3. instantiate processor + controller ================================
    IComponent* component = nullptr;
    tresult result = factory->createInstance (audioClassUid, IComponent::iid,
                                              reinterpret_cast<void**> (&component));
    check (result == kResultOk && component != nullptr, "IComponent instance created");

    if (component == nullptr)
        return 1;

    IAudioProcessor* processor = nullptr;
    result = component->queryInterface (IAudioProcessor::iid,
                                        reinterpret_cast<void**> (&processor));
    check (result == kResultOk && processor != nullptr, "component implements IAudioProcessor");

    if (processor == nullptr)
        return 1;

    // The class id the component asks its host to use for the controller.
    TUID requestedControllerTuid {};
    FUID requestedControllerUid;
    if (component->getControllerClassId (requestedControllerTuid) == kResultOk)
        requestedControllerUid = FUID (requestedControllerTuid);

    if (requestedControllerUid.isValid() && controllerClassUid.isValid())
        check (requestedControllerUid == controllerClassUid,
               "component asks for the controller class that the module exports");

    IEditController* controller = nullptr;
    result = factory->createInstance (controllerClassUid, IEditController::iid,
                                      reinterpret_cast<void**> (&controller));
    check (result == kResultOk && controller != nullptr, "IEditController instance created");

    //== 4. buses =============================================================
    const int32 numAudioIn  = component->getBusCount (kAudio, kInput);
    const int32 numAudioOut = component->getBusCount (kAudio, kOutput);

    BusInfo inInfo {};
    if (numAudioIn > 0)
        component->getBusInfo (kAudio, kInput, 0, inInfo);

    check (numAudioIn == 1, "exposes exactly one audio input bus");
    check (numAudioOut == 1, "exposes exactly one audio output bus");
    check (inInfo.channelCount == 2, "the audio input bus is stereo");
    check (inInfo.busType == kMain, "the audio input bus is the main bus");

    //== 5. initialise both halves and connect them ===========================
    check (component->initialize (nullptr) == kResultOk, "IComponent::initialize() succeeded");

    if (controller != nullptr)
        check (controller->initialize (nullptr) == kResultOk, "IEditController::initialize() succeeded");

    if (controller != nullptr)
    {
        IConnectionPoint* componentCP = nullptr;
        IConnectionPoint* controllerCP = nullptr;
        component->queryInterface (IConnectionPoint::iid, reinterpret_cast<void**> (&componentCP));
        controller->queryInterface (IConnectionPoint::iid, reinterpret_cast<void**> (&controllerCP));

        if (controllerCP != nullptr && componentCP != nullptr)
        {
            controllerCP->connect (componentCP);
            componentCP->connect (controllerCP);
        }

        check (controller->getParameterCount() >= 30,
               "the controller publishes the automatable parameters");
        std::printf ("      parameter count: %d\n", (int) controller->getParameterCount());

        ParameterInfo pInfo {};
        if (controller->getParameterInfo (0, pInfo) == kResultOk)
        {
            char title[128] {};
            for (int i = 0; i < 127 && pInfo.title[i] != 0; ++i)
                title[i] = (char) pInfo.title[i];
            std::printf ("      first parameter: \"%s\"\n", title);
        }
    }

    //== 5b. the editor view must be constructible ============================
    if (controller != nullptr)
    {
        IPlugView* view = controller->createView (ViewType::kEditor);
        check (view != nullptr,
               "the plugin creates its editor view (GUI code runs without a crash)");

        if (view != nullptr)
        {
            ViewRect rect {};
            check (view->getSize (&rect) == kResultOk && rect.getWidth() > 200 && rect.getHeight() > 150,
                   "the editor reports a sensible default size");

            view->release();
        }
    }

    //== 6. activate and process ==============================================
    check (component->activateBus (kAudio, kInput, 0, true) == kResultOk,
           "activated the input bus");
    check (component->activateBus (kAudio, kOutput, 0, true) == kResultOk,
           "activated the output bus");

    ProcessSetup setup {};
    setup.processMode        = kRealtime;
    setup.symbolicSampleSize = kSample32;
    setup.maxSamplesPerBlock = kBlockSize;
    setup.sampleRate         = kSampleRate;

    check (processor->setupProcessing (setup) == kResultOk,
           "setupProcessing(48 kHz, 512 samples) succeeded");
    check (processor->getLatencySamples() == 0, "reports zero latency");

    check (component->setActive (true) == kResultOk, "setActive(true) succeeded");
    check (processor->setProcessing (true) == kResultOk, "setProcessing(true) succeeded");

    std::vector<float> inL (kBlockSize), inR (kBlockSize);
    std::vector<float> outL (kBlockSize, 0.0f), outR (kBlockSize, 0.0f);

    AudioBusBuffers inputBus {};
    inputBus.numChannels = 2;
    float* inChannels[2] { inL.data(), inR.data() };
    inputBus.channelBuffers32 = inChannels;

    AudioBusBuffers outputBus {};
    outputBus.numChannels = 2;
    float* outChannels[2] { outL.data(), outR.data() };
    outputBus.channelBuffers32 = outChannels;

    ProcessData data {};
    data.processMode            = kRealtime;
    data.symbolicSampleSize     = kSample32;
    data.numSamples             = kBlockSize;
    data.numInputs              = 1;
    data.numOutputs             = 1;
    data.inputs                 = &inputBus;
    data.outputs                = &outputBus;
    data.inputParameterChanges  = nullptr;
    data.outputParameterChanges = nullptr;
    data.inputEvents            = nullptr;
    data.outputEvents           = nullptr;
    data.processContext         = nullptr;

    std::vector<float> captured;
    bool allOk = true;

    for (int block = 0; block < 3; ++block)
    {
        for (int i = 0; i < kBlockSize; ++i)
        {
            const double t = (double) (block * kBlockSize + i) / kSampleRate;
            inL[(size_t) i] = inR[(size_t) i] = (float) (0.3 * std::sin (2.0 * M_PI * 220.0 * t));
        }
        std::fill (outL.begin(), outL.end(), 0.0f);
        std::fill (outR.begin(), outR.end(), 0.0f);

        if (processor->process (data) != kResultOk)
            allOk = false;

        captured.insert (captured.end(), outL.begin(), outL.end());
        captured.insert (captured.end(), outR.begin(), outR.end());
    }

    check (allOk, "process() returned kResultOk for every block");
    check (finite (captured), "processed output contains no NaN/Inf");
    check (peak (captured) > 0.01f, "audio actually flows through the plugin");
    check (peak (captured) <= 1.0f, "output stays inside full scale");
    std::printf ("      output peak for a 0.3 peak input: %.4f\n", peak (captured));

    //== 7. silence in, silence out ===========================================
    std::fill (inL.begin(), inL.end(), 0.0f);
    std::fill (inR.begin(), inR.end(), 0.0f);

    std::vector<float> lastBlock;
    for (int block = 0; block < 12; ++block)
    {
        std::fill (outL.begin(), outL.end(), 0.0f);
        std::fill (outR.begin(), outR.end(), 0.0f);
        processor->process (data);
        lastBlock = outL;
    }
    std::printf ("      settled silence level: %.3e\n", peak (lastBlock));
    check (peak (lastBlock) < 1.0e-5f, "silent input produces silent output once settled");

    //== 8. shut down =========================================================
    processor->setProcessing (false);
    component->setActive (false);
    component->terminate();
    check (true, "shut the plugin down cleanly");

    std::printf ("\n=========================\n%d checks, %d failures\n", checks, failures);
    return failures == 0 ? 0 : 1;
}
