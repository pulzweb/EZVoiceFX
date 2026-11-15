// EZ Voice FX - vocal separation network (MDX-Net Voc FT) through ONNX Runtime.
// GPU via DirectML when available, CPU otherwise. Everything is loaded at run time
// from the plugin folder: onnxruntime.dll, DirectML.dll, UVR-MDX-NET-Voc_FT.onnx.
// This header has no JUCE / Windows dependencies on purpose.
#pragma once

#include <memory>
#include <string>

namespace ez
{
class OrtEngine
{
public:
    static constexpr int kPlanes = 4, kBins = 3072, kFrames = 256;
    static constexpr int kTensorSize = kPlanes * kBins * kFrames;

    OrtEngine();
    ~OrtEngine();

    // dir: folder with the dlls and the model (wide string path). Returns true on success.
    bool load (const std::wstring& dir, bool tryGpu);
    bool isLoaded() const noexcept;
    bool isGpu() const noexcept;
    const std::string& status() const noexcept;

    // in/out: float [1, 4, 3072, 256]  (L re, L im, R re, R im planes)
    bool run (const float* in, float* out);

    static bool compiledWithOrt() noexcept;

private:
    struct Impl;
    std::unique_ptr<Impl> impl;
};
} // namespace ez
