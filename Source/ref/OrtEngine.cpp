#include "OrtEngine.h"

#include <thread>
#include <algorithm>

#if defined (_WIN32) && defined (EZ_HAS_ORT)
 #ifndef NOMINMAX
  #define NOMINMAX
 #endif
 #ifndef WIN32_LEAN_AND_MEAN
  #define WIN32_LEAN_AND_MEAN
 #endif
 #include <windows.h>
 #include <onnxruntime_c_api.h>
 #define EZ_ORT_ENABLED 1
#else
 #define EZ_ORT_ENABLED 0
#endif

namespace ez
{
#if EZ_ORT_ENABLED

struct OrtEngine::Impl
{
    using GetApiBaseFn = const OrtApiBase* (ORT_API_CALL*) (void);
    using AppendDmlFn  = OrtStatus* (ORT_API_CALL*) (OrtSessionOptions*, int);

    const OrtApi* api = nullptr;
    OrtEnv* env = nullptr;
    OrtSessionOptions* opts = nullptr;
    OrtSession* session = nullptr;
    OrtMemoryInfo* mem = nullptr;
    std::string inName { "input" }, outName { "output" };
    std::string status { "not loaded" };
    bool gpu = false, loaded = false;

    bool ok (OrtStatus* s, const char* what)
    {
        if (s == nullptr) return true;
        status = std::string (what) + ": " + api->GetErrorMessage (s);
        api->ReleaseStatus (s);
        return false;
    }

    ~Impl()
    {
        if (api == nullptr) return;
        if (session != nullptr) api->ReleaseSession (session);
        if (opts != nullptr) api->ReleaseSessionOptions (opts);
        if (mem != nullptr) api->ReleaseMemoryInfo (mem);
        if (env != nullptr) api->ReleaseEnv (env);
    }

    bool createSession (const std::wstring& model, bool useGpu, HMODULE ort)
    {
        if (opts != nullptr) { api->ReleaseSessionOptions (opts); opts = nullptr; }
        if (! ok (api->CreateSessionOptions (&opts), "options")) return false;
        api->SetSessionGraphOptimizationLevel (opts, ORT_ENABLE_ALL);
        const int threads = std::max (1, (int) std::thread::hardware_concurrency() / 2);
        api->SetIntraOpNumThreads (opts, threads);

        if (useGpu)
        {
            api->DisableMemPattern (opts);
            api->SetSessionExecutionMode (opts, ORT_SEQUENTIAL);
            auto appendDml = reinterpret_cast<AppendDmlFn> (GetProcAddress (ort, "OrtSessionOptionsAppendExecutionProvider_DML"));
            if (appendDml == nullptr) { status = "DirectML entry point missing"; return false; }
            if (! ok (appendDml (opts, 0), "DirectML")) return false;
        }
        return ok (api->CreateSession (env, model.c_str(), opts, &session), "session");
    }
};

OrtEngine::OrtEngine() : impl (std::make_unique<Impl>()) {}
OrtEngine::~OrtEngine() = default;

bool OrtEngine::compiledWithOrt() noexcept { return true; }
bool OrtEngine::isLoaded() const noexcept { return impl->loaded; }
bool OrtEngine::isGpu() const noexcept { return impl->gpu; }
const std::string& OrtEngine::status() const noexcept { return impl->status; }

bool OrtEngine::load (const std::wstring& dir, bool tryGpu)
{
    auto& d = *impl;
    if (d.loaded) return true;

    const std::wstring base = dir.empty() || dir.back() == L'\\' || dir.back() == L'/' ? dir : dir + L"\\";
    const std::wstring model = base + L"UVR-MDX-NET-Voc_FT.onnx";
    if (GetFileAttributesW (model.c_str()) == INVALID_FILE_ATTRIBUTES) { d.status = "model file not found"; return false; }

    // our DirectML first (newer than the copy in System32), then ONNX Runtime from the same folder
    LoadLibraryExW ((base + L"DirectML.dll").c_str(), nullptr, LOAD_WITH_ALTERED_SEARCH_PATH);
    HMODULE ort = LoadLibraryExW ((base + L"onnxruntime.dll").c_str(), nullptr, LOAD_WITH_ALTERED_SEARCH_PATH);
    if (ort == nullptr) { d.status = "onnxruntime.dll could not be loaded"; return false; }

    auto getBase = reinterpret_cast<Impl::GetApiBaseFn> (GetProcAddress (ort, "OrtGetApiBase"));
    if (getBase == nullptr) { d.status = "OrtGetApiBase missing"; return false; }
    d.api = getBase()->GetApi (ORT_API_VERSION);
    if (d.api == nullptr) { d.status = "ONNX Runtime version mismatch"; return false; }

    if (! d.ok (d.api->CreateEnv (ORT_LOGGING_LEVEL_WARNING, "ezvfx", &d.env), "env")) return false;
    if (! d.ok (d.api->CreateCpuMemoryInfo (OrtArenaAllocator, OrtMemTypeDefault, &d.mem), "memory")) return false;

    bool created = false;
    if (tryGpu)
    {
        created = d.createSession (model, true, ort);
        d.gpu = created;
    }
    if (! created)
    {
        const std::string gpuError = d.status;
        created = d.createSession (model, false, ort);
        if (created && tryGpu) d.status = "GPU unavailable (" + gpuError + "), using CPU";
    }
    if (! created) return false;

    // real tensor names
    OrtAllocator* alloc = nullptr;
    if (d.api->GetAllocatorWithDefaultOptions (&alloc) == nullptr && alloc != nullptr)
    {
        char* name = nullptr;
        if (d.api->SessionGetInputName (d.session, 0, alloc, &name) == nullptr && name != nullptr)
        {
            d.inName = name;
            d.api->AllocatorFree (alloc, name);
        }
        name = nullptr;
        if (d.api->SessionGetOutputName (d.session, 0, alloc, &name) == nullptr && name != nullptr)
        {
            d.outName = name;
            d.api->AllocatorFree (alloc, name);
        }
    }

    d.loaded = true;
    if (d.gpu) d.status = "GPU (DirectML)";
    else if (! tryGpu) d.status = "CPU";
    return true;
}

bool OrtEngine::run (const float* in, float* out)
{
    auto& d = *impl;
    if (! d.loaded) return false;

    const int64_t shape[4] = { 1, kPlanes, kBins, kFrames };
    OrtValue* input = nullptr;
    if (! d.ok (d.api->CreateTensorWithDataAsOrtValue (d.mem, const_cast<float*> (in), sizeof (float) * (size_t) kTensorSize,
                                                       shape, 4, ONNX_TENSOR_ELEMENT_DATA_TYPE_FLOAT, &input), "tensor"))
        return false;

    const char* inNames[] = { d.inName.c_str() };
    const char* outNames[] = { d.outName.c_str() };
    OrtValue* output = nullptr;
    const bool good = d.ok (d.api->Run (d.session, nullptr, inNames, &input, 1, outNames, 1, &output), "run");
    d.api->ReleaseValue (input);
    if (! good) return false;

    float* data = nullptr;
    const bool got = d.ok (d.api->GetTensorMutableData (output, reinterpret_cast<void**> (&data)), "output");
    if (got && data != nullptr) std::copy (data, data + kTensorSize, out);
    d.api->ReleaseValue (output);
    return got && data != nullptr;
}

#else   // ---------------------------------------------------------------- no ONNX Runtime

struct OrtEngine::Impl { std::string status { "vocal separation engine not included in this build" }; };

OrtEngine::OrtEngine() : impl (std::make_unique<Impl>()) {}
OrtEngine::~OrtEngine() = default;
bool OrtEngine::compiledWithOrt() noexcept { return false; }
bool OrtEngine::load (const std::wstring&, bool) { return false; }
bool OrtEngine::isLoaded() const noexcept { return false; }
bool OrtEngine::isGpu() const noexcept { return false; }
const std::string& OrtEngine::status() const noexcept { return impl->status; }
bool OrtEngine::run (const float*, float*) { return false; }

#endif
} // namespace ez
