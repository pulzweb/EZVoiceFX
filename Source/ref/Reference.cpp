#include "Reference.h"
#include "Bluestein.h"
#include "../dsp/Loudness.h"

namespace ez
{
static juce::String ru (const char* s) { return juce::String::fromUTF8 (s); }

//==============================================================================
juce::String ReferenceProfile::toString() const
{
    if (! valid) return {};
    juce::StringArray a;
    a.add (name.replaceCharacter ('|', '/'));
    a.add (juce::String ((int) balanceValid) + "," + juce::String ((int) separated) + "," + juce::String ((int) approximate)
           + "," + juce::String ((int) gpu));
    a.add (juce::String (balanceDb, 2)); a.add (juce::String (lra, 2));
    a.add (juce::String (sibilance, 4)); a.add (juce::String (seconds, 1));
    juce::StringArray b;
    for (float v : bands) b.add (juce::String (v, 2));
    a.add (b.joinIntoString (","));
    return a.joinIntoString ("|");
}

ReferenceProfile ReferenceProfile::fromString (const juce::String& s)
{
    ReferenceProfile p;
    const auto a = juce::StringArray::fromTokens (s, "|", "");
    if (a.size() < 7) return p;
    p.name = a[0];
    const auto f = juce::StringArray::fromTokens (a[1], ",", "");
    if (f.size() >= 4) { p.balanceValid = f[0].getIntValue() != 0; p.separated = f[1].getIntValue() != 0;
                         p.approximate = f[2].getIntValue() != 0; p.gpu = f[3].getIntValue() != 0; }
    p.balanceDb = a[2].getFloatValue(); p.lra = a[3].getFloatValue();
    p.sibilance = a[4].getFloatValue(); p.seconds = a[5].getFloatValue();
    const auto b = juce::StringArray::fromTokens (a[6], ",", "");
    if (b.size() != kMatchBands) return p;
    for (int i = 0; i < kMatchBands; ++i) p.bands[i] = b[i].getFloatValue();
    p.valid = true;
    return p;
}

//==============================================================================
ReferenceAnalyzer::ReferenceAnalyzer() : juce::Thread ("EZ Voice FX reference", 16 * 1024 * 1024) {}

ReferenceAnalyzer::~ReferenceAnalyzer() { stopThread (30000); }

void ReferenceAnalyzer::startAnalysis (const juce::File& f, Mode m, bool gpu)
{
    stopThread (10000);
    file = f; mode = m; allowGpu = gpu;
    progress.store (0.0f);
    resultReady.store (false);
    setStatus (ru ("Запуск анализа..."));
    startThread();
}

void ReferenceAnalyzer::cancel() { stopThread (10000); setStatus (ru ("Анализ отменён")); }

juce::String ReferenceAnalyzer::getStatus() const { const juce::ScopedLock sl (lock); return status; }
void ReferenceAnalyzer::setStatus (const juce::String& s) { const juce::ScopedLock sl (lock); status = s; }

bool ReferenceAnalyzer::takeResult (ReferenceProfile& out)
{
    if (! resultReady.exchange (false)) return false;
    const juce::ScopedLock sl (lock);
    out = result;
    return true;
}

//==============================================================================
// loads up to 60 s (the loudest part) as 44.1 kHz stereo
bool ReferenceAnalyzer::loadAudio (juce::AudioBuffer<float>& out, double& seconds)
{
    juce::AudioFormatManager fm;
    fm.registerBasicFormats();
    std::unique_ptr<juce::AudioFormatReader> reader (fm.createReaderFor (file));
    if (reader == nullptr) { setStatus (ru ("Не удалось открыть файл (поддерживаются WAV, AIFF, FLAC, OGG, MP3)")); return false; }

    const double sr = reader->sampleRate;
    const juce::int64 len = reader->lengthInSamples;
    if (sr <= 0.0 || len < (juce::int64) (sr * 5.0)) { setStatus (ru ("Файл слишком короткий (нужно от 5 секунд)")); return false; }

    const juce::int64 want = std::min (len, (juce::int64) (sr * 60.0));
    juce::int64 start = 0;
    if (len > want)
    {
        // energy per second, then the loudest 60 s window
        const int sec = (int) (len / (juce::int64) sr);
        std::vector<double> e ((size_t) sec, 0.0);
        juce::AudioBuffer<float> tmp ((int) reader->numChannels, (int) sr);
        for (int s = 0; s < sec && ! threadShouldExit(); ++s)
        {
            reader->read (&tmp, 0, (int) sr, (juce::int64) s * (juce::int64) sr, true, true);
            double acc = 0.0;
            for (int c = 0; c < tmp.getNumChannels(); ++c)
            {
                const float* d = tmp.getReadPointer (c);
                for (int i = 0; i < tmp.getNumSamples(); i += 4) acc += (double) d[i] * d[i];
            }
            e[(size_t) s] = acc;
            progress.store (0.05f * (float) s / (float) sec);
        }
        const int win = (int) (want / (juce::int64) sr);
        double best = -1.0, run = 0.0;
        int bestStart = 0;
        for (int s = 0; s < sec; ++s)
        {
            run += e[(size_t) s];
            if (s >= win) run -= e[(size_t) (s - win)];
            if (s >= win - 1 && run > best) { best = run; bestStart = s - win + 1; }
        }
        start = (juce::int64) bestStart * (juce::int64) sr;
    }

    juce::AudioBuffer<float> raw (2, (int) want);
    raw.clear();
    reader->read (&raw, 0, (int) want, start, true, true);
    if (reader->numChannels == 1) raw.copyFrom (1, 0, raw, 0, 0, (int) want);

    if (std::abs (sr - 44100.0) < 1.0)
    {
        out = raw;
    }
    else
    {
        const double ratio = sr / 44100.0;
        const int outLen = (int) ((double) want / ratio) - 8;
        out.setSize (2, outLen);
        for (int c = 0; c < 2; ++c)
        {
            juce::LagrangeInterpolator interp;
            interp.process (ratio, raw.getReadPointer (c), out.getWritePointer (c), outLen);
        }
    }
    seconds = (double) out.getNumSamples() / 44100.0;
    return true;
}

//==============================================================================
static float biquadPower (const Biquad& q, double w)
{
    const std::complex<double> z1 = std::polar (1.0, -w), z2 = std::polar (1.0, -2.0 * w);
    const auto num = q.b0 + q.b1 * z1 + q.b2 * z2;
    const auto den = 1.0 + q.a1 * z1 + q.a2 * z2;
    return (float) std::norm (num / den);
}

bool ReferenceAnalyzer::analyse (const juce::AudioBuffer<float>& audio, Mode m, std::vector<FrameStat>& frames, bool& usedGpu, Mode& usedMode)
{
    usedMode = m;
    constexpr int nfft = 7680, hop = 1024, dimF = OrtEngine::kBins, dimT = OrtEngine::kFrames;
    constexpr int nBins = nfft / 2 + 1;
    constexpr int trim = nfft / 2;
    constexpr int chunk = hop * (dimT - 1);
    constexpr int gen = chunk - 2 * trim;
    constexpr float compensate = 1.021f;
    constexpr double fsr = 44100.0;

    const int n = audio.getNumSamples();
    const bool useNet = (m == Auto || m == Precise);
    usedGpu = false;

    if (useNet)
    {
        if (engine == nullptr) engine = std::make_unique<OrtEngine>();
        if (! engine->isLoaded())
        {
            setStatus (ru ("Загрузка нейросети отделения вокала..."));
            const auto dir = juce::File::getSpecialLocation (juce::File::currentExecutableFile).getParentDirectory();
            if (! engine->load (dir.getFullPathName().toWideCharPointer(), allowGpu))
            {
                setStatus (ru ("Нейросеть недоступна: ") + juce::String (engine->status()) + ru (". Использую быстрый режим."));
                return analyse (audio, Fast, frames, usedGpu, usedMode);
            }
        }
        usedGpu = engine->isGpu();
    }

    // constant tables
    std::vector<float> window ((size_t) nfft), kw ((size_t) nBins);
    for (int i = 0; i < nfft; ++i) window[(size_t) i] = (float) (0.5 - 0.5 * std::cos (2.0 * kPi * i / nfft));
    {
        KWeighting k;
        k.prepare (fsr);
        for (int b = 0; b < nBins; ++b)
        {
            const double w = 2.0 * kPi * (double) b / nfft;
            kw[(size_t) b] = biquadPower (k.shelf, w) * biquadPower (k.hp, w);
        }
    }
    std::vector<int> bandOf ((size_t) nBins, -1);
    for (int b = 1; b < nBins; ++b)
    {
        const float f = (float) (b * fsr / nfft);
        for (int k = 0; k < kMatchBands; ++k)
        {
            const float lo = kMatchFreqs[k] / 1.33f, hi = kMatchFreqs[k] * 1.33f;
            if (f >= lo && f < hi) { bandOf[(size_t) b] = k; break; }
        }
    }
    const int hiBin = (int) (5000.0 * nfft / fsr);

    BluesteinFFT fft;
    fft.prepare (nfft);
    std::vector<std::complex<float>> zin ((size_t) nfft), Z ((size_t) nfft);
    std::vector<std::complex<float>> specL ((size_t) (nBins * dimT)), specR ((size_t) (nBins * dimT));
    std::vector<float> tensorIn ((size_t) OrtEngine::kTensorSize), tensorOut ((size_t) OrtEngine::kTensorSize);

    // padded mix like UVR: [trim zeros | audio | pad zeros | trim zeros]
    const int pad = gen - (n % gen);
    const int total = trim + n + pad + trim;
    const int numChunks = (n + pad) / gen;
    auto sampleAt = [&] (int c, int idx) -> float
    {
        const int j = idx - trim;
        return (j >= 0 && j < n) ? audio.getSample (c, j) : 0.0f;
    };
    juce::ignoreUnused (total);

    frames.clear();
    frames.reserve ((size_t) (numChunks * dimT));
    const double t0 = juce::Time::getMillisecondCounterHiRes();

    for (int ci = 0; ci < numChunks; ++ci)
    {
        if (threadShouldExit()) return false;
        const int base = ci * gen;

        // STFT of the chunk (center = true, reflect padding), both channels in one complex FFT
        for (int t = 0; t < dimT; ++t)
        {
            for (int i = 0; i < nfft; ++i)
            {
                int p = t * hop - nfft / 2 + i;                    // position inside the chunk
                if (p < 0) p = -p;
                if (p >= chunk) p = 2 * (chunk - 1) - p;
                const float l = sampleAt (0, base + p), r = sampleAt (1, base + p);
                zin[(size_t) i] = std::complex<float> (l * window[(size_t) i], r * window[(size_t) i]);
            }
            fft.forward (zin.data(), Z.data());
            for (int k = 0; k < nBins; ++k)
            {
                const auto zk = Z[(size_t) k];
                const auto zn = std::conj (Z[(size_t) ((nfft - k) % nfft)]);
                specL[(size_t) (k * dimT + t)] = 0.5f * (zk + zn);
                specR[(size_t) (k * dimT + t)] = std::complex<float> (0.0f, -0.5f) * (zk - zn);
            }
        }

        const bool netOk = useNet && [&]
        {
            for (int k = 0; k < dimF; ++k)
                for (int t = 0; t < dimT; ++t)
                {
                    const auto l = specL[(size_t) (k * dimT + t)], r = specR[(size_t) (k * dimT + t)];
                    tensorIn[(size_t) ((0 * dimF + k) * dimT + t)] = l.real();
                    tensorIn[(size_t) ((1 * dimF + k) * dimT + t)] = l.imag();
                    tensorIn[(size_t) ((2 * dimF + k) * dimT + t)] = r.real();
                    tensorIn[(size_t) ((3 * dimF + k) * dimT + t)] = r.imag();
                }
            return engine->run (tensorIn.data(), tensorOut.data());
        }();
        if (useNet && ! netOk)
        {
            setStatus (ru ("Ошибка нейросети: ") + juce::String (engine->status()) + ru (". Использую быстрый режим."));
            return analyse (audio, Fast, frames, usedGpu, usedMode);
        }

        // statistics for the frames that UVR keeps (centres inside [trim, chunk - trim))
        for (int t = 0; t < dimT; ++t)
        {
            const int centre = t * hop;
            if (centre < trim || centre >= chunk - trim) continue;
            if (base + centre - trim >= n) break;

            FrameStat fsx {};
            for (int k = 1; k < nBins; ++k)
            {
                const auto ml = specL[(size_t) (k * dimT + t)], mr = specR[(size_t) (k * dimT + t)];
                const float pm = std::norm (ml) + std::norm (mr);
                float pv;
                if (useNet && k < dimF)
                {
                    const std::complex<float> vl (tensorOut[(size_t) ((0 * dimF + k) * dimT + t)], tensorOut[(size_t) ((1 * dimF + k) * dimT + t)]);
                    const std::complex<float> vr (tensorOut[(size_t) ((2 * dimF + k) * dimT + t)], tensorOut[(size_t) ((3 * dimF + k) * dimT + t)]);
                    pv = std::norm (vl) + std::norm (vr);
                    const float pi = std::norm (ml - compensate * vl) + std::norm (mr - compensate * vr);
                    fsx.ki += kw[(size_t) k] * pi;
                }
                else if (m == Acapella)
                {
                    pv = pm;
                }
                else if (useNet)
                {
                    pv = 0.0f;                                         // above the model range: instrumental
                    fsx.ki += kw[(size_t) k] * pm;
                }
                else
                {
                    // fast approximation: centred (mid minus side) energy in the voice range
                    const float f = (float) (k * fsr / nfft);
                    const float pmid = 0.25f * std::norm (ml + mr), pside = 0.25f * std::norm (ml - mr);
                    float wgt = 1.0f;
                    if (f < 250.0f) wgt = juce::jlimit (0.0f, 1.0f, (f - 150.0f) / 100.0f);
                    if (f > 7000.0f) wgt = juce::jlimit (0.0f, 1.0f, (10000.0f - f) / 3000.0f);
                    pv = std::min (pm, 2.0f * std::max (0.0f, pmid - pside) * wgt * 0.6f);
                    fsx.ki += kw[(size_t) k] * (pm - pv);
                }
                fsx.kv += kw[(size_t) k] * pv;
                fsx.full += pv;
                if (k >= hiBin) fsx.hi += pv;
                const int bnd = bandOf[(size_t) k];
                if (bnd >= 0) fsx.bands[bnd] += pv;
            }
            frames.push_back (fsx);
        }

        progress.store (0.05f + 0.9f * (float) (ci + 1) / (float) numChunks);
        const double elapsed = (juce::Time::getMillisecondCounterHiRes() - t0) / 1000.0;
        const double eta = elapsed / (ci + 1) * (numChunks - ci - 1);
        setStatus ((useNet ? (usedGpu ? ru ("Отделяю вокал (видеокарта)... ") : ru ("Отделяю вокал (процессор)... "))
                           : ru ("Быстрый анализ... "))
                   + juce::String (ci + 1) + "/" + juce::String (numChunks) + ru (", осталось ~") + juce::String ((int) eta) + ru (" с"));

        // weak PC: in Auto mode do not make the user wait minutes
        if (m == Auto && ci == 0 && numChunks > 1 && ! usedGpu && elapsed * numChunks > 75.0)
        {
            setStatus (ru ("Компьютер не тянет нейросеть быстро (~") + juce::String ((int) (elapsed * numChunks)) +
                       ru (" с). Переключаюсь на быстрый режим. Точный анализ можно запустить режимом «Точно»."));
            return analyse (audio, Fast, frames, usedGpu, usedMode);
        }
    }
    return true;
}

//==============================================================================
void ReferenceAnalyzer::finish (const std::vector<FrameStat>& frames, Mode m, bool usedGpu, double seconds)
{
    ReferenceProfile p;
    p.name = file.getFileNameWithoutExtension();
    p.seconds = (float) seconds;
    p.separated = (m == Auto || m == Precise);
    p.approximate = (m == Fast);
    p.gpu = usedGpu;

    const int F = (int) frames.size(), blk = 17, step = 4;
    if (F < blk * 4) { setStatus (ru ("Слишком мало данных для анализа")); return; }

    std::vector<double> bv, bi;
    std::vector<int> bstart;
    for (int s = 0; s + blk <= F; s += step)
    {
        double v = 0.0, i = 0.0;
        for (int k = s; k < s + blk; ++k) { v += frames[(size_t) k].kv; i += frames[(size_t) k].ki; }
        bv.push_back (v); bi.push_back (i); bstart.push_back (s);
    }
    const double vmax = *std::max_element (bv.begin(), bv.end());
    if (vmax <= 0.0) { setStatus (ru ("В референсе не найден голос")); return; }

    std::vector<char> frameActive ((size_t) F, 0);
    double sv = 0.0, si = 0.0;
    std::vector<float> levels;
    for (size_t b = 0; b < bv.size(); ++b)
    {
        if (bv[b] < vmax * 0.003 || bv[b] < bi[b] * 0.001) continue;      // -25 dB vs loudest, not buried
        sv += bv[b]; si += bi[b];
        levels.push_back ((float) (10.0 * std::log10 (bv[b] + 1.0e-20)));
        for (int k = bstart[b]; k < bstart[b] + blk; ++k) frameActive[(size_t) k] = 1;
    }
    if (levels.size() < 8) { setStatus (ru ("В референсе слишком мало вокала")); return; }

    p.balanceValid = (m != Acapella) && si > 0.0;
    p.balanceDb = p.balanceValid ? (float) juce::jlimit (-20.0, 20.0, 10.0 * std::log10 (sv / si)) : 0.0f;
    std::sort (levels.begin(), levels.end());
    p.lra = levels[levels.size() * 95 / 100] - levels[levels.size() * 10 / 100];

    double bands[kMatchBands] {}, hi = 0.0, full = 0.0;
    for (int k = 0; k < F; ++k)
        if (frameActive[(size_t) k])
        {
            for (int b = 0; b < kMatchBands; ++b) bands[b] += frames[(size_t) k].bands[b];
            hi += frames[(size_t) k].hi; full += frames[(size_t) k].full;
        }
    float db[kMatchBands];
    double mean = 0.0;
    for (int b = 0; b < kMatchBands; ++b)
    {
        db[b] = (float) (10.0 * std::log10 (bands[b] + 1.0e-20));
        if (b >= 2 && b <= 7) mean += db[b];
    }
    mean /= 6.0;
    for (int b = 0; b < kMatchBands; ++b) p.bands[b] = juce::jlimit (-40.0f, 20.0f, db[b] - (float) mean);
    p.sibilance = full > 0.0 ? (float) (hi / full) : 0.1f;
    p.valid = true;

    {
        const juce::ScopedLock sl (lock);
        result = p;
    }
    juce::String s;
    s << ru ("Готово: ") << p.name << " (" << juce::String (p.seconds, 0) << ru (" с, ")
      << (p.separated ? (p.gpu ? ru ("нейросеть на видеокарте") : ru ("нейросеть на процессоре"))
                      : (p.approximate ? ru ("быстрый режим, приблизительно") : ru ("акапелла"))) << ")";
    setStatus (s);
    progress.store (1.0f);
    resultReady.store (true);
}

void ReferenceAnalyzer::run()
{
    juce::AudioBuffer<float> audio;
    double seconds = 0.0;
    setStatus (ru ("Чтение файла..."));
    if (! loadAudio (audio, seconds) || threadShouldExit()) return;

    std::vector<FrameStat> frames;
    bool gpu = false;
    Mode used = mode;                                  // analyse() may fall back to Fast
    if (! analyse (audio, mode, frames, gpu, used) || threadShouldExit()) return;
    finish (frames, used, gpu, seconds);
}
} // namespace ez
