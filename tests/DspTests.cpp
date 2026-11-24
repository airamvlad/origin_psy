#include "dsp/Chain.h"
#include "dsp/DryWet.h"
#include "dsp/Fraza.h"
#include "dsp/Fft.h"
#include "dsp/Modulation.h"
#include "dsp/Scales.h"

#include <chrono>
#include <cmath>
#include <cstdio>
#include <vector>

using namespace originpsy;

namespace
{
int failures = 0;

void check (bool condition, const char* what)
{
    if (! condition)
    {
        std::printf ("FAIL: %s\n", what);
        ++failures;
    }
}

bool near (double a, double b, double tol) { return std::abs (a - b) <= tol; }

constexpr double sr = 48000.0;

Transport transportAt (double bpm, double ppq)
{
    Transport t;
    t.bpm = bpm;
    t.ppqAtBlockStart = ppq;
    t.playing = true;
    return t;
}

/** Runs a stereo buffer through a module in blocks, with a moving transport. */
template <typename Module>
void run (Module& m, std::vector<float>& l, std::vector<float>& r, double bpm, int block = 256)
{
    double ppq = 0.0;
    for (size_t start = 0; start < l.size(); start += (size_t) block)
    {
        const int n = (int) std::min ((size_t) block, l.size() - start);
        auto t = transportAt (bpm, ppq);
        m.process (l.data() + start, r.data() + start, n, t);
        ppq += n / t.samplesPerBeat (sr);
    }
}

bool allFinite (const std::vector<float>& v)
{
    for (float x : v) if (! std::isfinite (x) || std::abs (x) > 20.0f) return false;
    return true;
}

double rms (const std::vector<float>& v, size_t from, size_t to)
{
    double s = 0.0;
    for (size_t i = from; i < to; ++i) s += (double) v[i] * v[i];
    return std::sqrt (s / (double) std::max<size_t> (1, to - from));
}

std::vector<float> sine (double hz, size_t length, float amp = 0.5f)
{
    std::vector<float> v (length);
    for (size_t i = 0; i < length; ++i) v[i] = amp * (float) std::sin (twoPi * hz * (double) i / sr);
    return v;
}

std::vector<float> saw (double hz, size_t length, float amp = 0.5f)
{
    std::vector<float> v (length);
    double phase = 0.0;
    for (size_t i = 0; i < length; ++i)
    {
        v[i] = amp * (float) (2.0 * phase - 1.0);
        phase += hz / sr;
        phase -= std::floor (phase);
    }
    return v;
}

//==============================================================================
void scaleTests()
{
    check (degreesToSemitones (0, 0) == 0, "degree 0 is the root");
    check (degreesToSemitones (0, 1) == 1, "phrygian second is a semitone");
    check (degreesToSemitones (0, 7) == 12, "seven degrees is an octave");
    check (degreesToSemitones (0, -1) == -2, "one degree down in phrygian is a whole tone");
    check (degreesToSemitones (1, 2) == 4, "hijaz third is major");
    check (snapToScale (0, 0, 66) == 65 || snapToScale (0, 0, 66) == 67, "F# snaps into C phrygian");
}

void euclidTests()
{
    const uint32_t e = euclid (8, 3, 0);
    int count = 0;
    for (int i = 0; i < 8; ++i) count += (e >> i) & 1;
    check (count == 3, "euclid 3 of 8 has three hits");
    check ((e & 1u) != 0, "euclid starts on the first step");
    check (euclid (16, 4, 0) == 0x1111u, "euclid 4 of 16 is four on the floor");
    check (euclid (16, 4, 1) == 0x2222u, "rotate moves the pattern by one step");
    check (euclid (5, 0, 0) == 0u, "zero hits is silent");
    check (euclid (32, 32, 0) == 0xffffffffu, "32 of 32 fills every step");
}

void frazaTests()
{
    Fraza::Params p;
    p.bars = 16;
    p.curve = Fraza::rise;
    check (near (Fraza::phrasePosition (p, 0.0, 4.0), 0.0, 1e-9), "phrase starts at bar 1");
    check (near (Fraza::phrasePosition (p, 32.0, 4.0), 0.5, 1e-9), "bar 9 of 16 is half way");
    check (near (Fraza::phrasePosition (p, 64.0, 4.0), 0.0, 1e-9), "phrase wraps after 16 bars");
    check (near (Fraza::evaluate (p, 0.5), 0.5, 1e-6), "rise is linear");

    p.curve = Fraza::lastBar;
    check (Fraza::evaluate (p, 14.5 / 16.0) == 0.0f, "last bar curve is silent before the last bar");
    check (near (Fraza::evaluate (p, 15.5 / 16.0), 0.5, 1e-6), "last bar curve rises inside the last bar");

    p.offset = 2;
    check (near (Fraza::phrasePosition (p, 8.0, 4.0), 0.0, 1e-9), "offset moves the phrase start");

    // 7/8: a bar is 3.5 quarter notes.
    p.offset = 0;
    check (near (Fraza::phrasePosition (p, 3.5 * 8.0, 3.5), 0.5, 1e-9), "phrase follows odd meters");

    // Hand-drawn curve: points joined by straight lines.
    float pts[Fraza::numCustomPoints];
    for (auto& v : pts) v = 0.0f;
    pts[Fraza::numCustomPoints - 1] = 1.0f;
    p.curve = Fraza::custom;
    p.customPoints = pts;
    check (Fraza::evaluate (p, 0.5) == 0.0f, "custom curve follows the drawn points");
    check (near (Fraza::evaluate (p, 1.0 - 0.5 / (Fraza::numCustomPoints - 1)), 0.5, 1e-5),
           "custom curve joins points with lines");
    p.customPoints = nullptr;
    check (near (Fraza::evaluate (p, 0.25), 0.25, 1e-6), "custom curve without points falls back to a rise");

    // Starting a custom curve from a ready-made one keeps its shape.
    Fraza::Params shape;
    shape.bars = 16;
    shape.curve = Fraza::riseExp;
    Fraza::fillCustomFrom (shape, pts);
    p.customPoints = pts;
    for (double pos : { 0.1, 0.37, 0.5, 0.81 })
        check (near (Fraza::evaluate (p, pos), Fraza::evaluate (shape, pos), 0.01), "custom copy matches the shape");

    // The ready-made curves are unchanged.
    shape.curve = Fraza::rise;       check (near (Fraza::evaluate (shape, 0.3), 0.3, 1e-6), "rise unchanged");
    shape.curve = Fraza::fall;       check (near (Fraza::evaluate (shape, 0.3), 0.7, 1e-6), "fall unchanged");
    shape.curve = Fraza::last4Bars;  check (near (Fraza::evaluate (shape, 14.0 / 16.0), 0.5, 1e-6), "last 4 bars unchanged");
}

//==============================================================================
void echoTests()
{
    FraktalEcho::Params p;
    p.generations = 3;
    std::array<FraktalEcho::Tap, FraktalEcho::maxTaps> taps {};
    const int count = FraktalEcho::buildTree (p, taps);
    check (count == 7, "three generations make seven echoes");
    check (near (taps[0].delayBeats, 1.0, 1e-9), "first echo is one stride late");
    check (near (taps[1].delayBeats, 2.0, 1e-9), "continuing branch adds a full stride");
    check (near (taps[2].delayBeats, 1.0 + p.ratio, 1e-6), "sprout adds stride x ratio");
    check (taps[2].degrees == p.spread && taps[1].degrees == 0, "only sprouts move the pitch");

    // Impulse: first echo lands exactly one 1/8 note later at 120 BPM.
    FraktalEcho echo;
    echo.prepare (sr, 256);
    p.spread = 0;          // no pitch shift: clean impulse
    p.mix = 1.0f;
    p.feedback = 0.0f;
    p.width = 0.0f;
    p.damp = 0.0f;
    echo.setParams (p);
    echo.reset();
    std::vector<float> l (48000, 0.0f), r (48000, 0.0f);
    l[100] = r[100] = 1.0f;
    run (echo, l, r, 120.0);
    const size_t expected = 100 + 12000; // 1/8 at 120 BPM = 0.25 s
    size_t peak = 0;
    for (size_t i = 0; i < 20000; ++i) if (std::abs (l[i]) > std::abs (l[peak])) peak = i;
    check (peak >= expected - 2 && peak <= expected + 2, "first echo is one 1/8 note after the hit");
    check (allFinite (l) && allFinite (r), "echo output is finite");

    // Mix 0 passes the input untouched.
    FraktalEcho dry;
    dry.prepare (sr, 256);
    auto p0 = FraktalEcho::Params {};
    p0.mix = 0.0f;
    dry.setParams (p0);
    dry.reset();
    auto in = saw (110.0, 24000);
    auto a = in, b = in;
    run (dry, a, b, 145.0);
    double err = 0.0;
    for (size_t i = 0; i < in.size(); ++i) err = std::max (err, (double) std::abs (a[i] - in[i]));
    check (err < 1.0e-6, "echo with mix 0 is a null test");

    // Pitched tree with heavy feedback stays bounded.
    FraktalEcho wild;
    wild.prepare (sr, 256);
    auto pw = FraktalEcho::Params {};
    pw.generations = 4; pw.feedback = 0.95f; pw.spread = 3; pw.mix = 1.0f;
    wild.setParams (pw);
    wild.reset();
    auto wl = saw (220.0, (size_t) sr * 6), wr = wl;
    run (wild, wl, wr, 150.0);
    check (allFinite (wl) && allFinite (wr), "max feedback stays bounded");
}

void vokalTests()
{
    // Pitch tracker finds A2 (110 Hz) in a saw and A3 in a sine.
    for (double hz : { 110.0, 220.0, 55.0 })
    {
        PitchTracker t;
        t.prepare (sr);
        auto s = saw (hz, 24000);
        for (float x : s) t.push (x);
        check (t.getConfidence() > 0.5f, "tracker is confident on a saw");
        check (near (t.getFrequency(), hz, hz * 0.01), "tracker finds the saw's pitch");
    }
    {
        PitchTracker t;
        t.prepare (sr);
        auto s = sine (220.0, 24000);
        for (float x : s) t.push (x);
        check (near (t.getFrequency(), 220.0, 2.5), "tracker finds a sine's pitch");
    }

    // Each note picks its vowel from the map.
    VokalAcid v;
    v.prepare (sr, 256);
    VokalAcid::Params p;
    p.glideMs = 5.0f;
    p.vowelMap = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 4, 0, 0 }; // A (pitch class 9) -> U
    v.setParams (p);
    v.reset();
    auto l = saw (110.0, 24000), r = l;
    run (v, l, r, 140.0);
    check (v.getDetectedNote() == 45, "vokal detects A2");
    check (near (v.getVowelPosition(), 4.0, 0.05), "A maps to U and the vowel glides there");
    check (allFinite (l) && allFinite (r), "vokal output is finite");
    const double level = rms (l, 12000, 24000);
    check (level > 0.03 && level < 1.5, "vokal keeps a usable level");

    // Robot character and full resonance stay bounded.
    p.character = VokalAcid::robot; p.resonance = 1.0f; p.drive = 1.0f; p.accent = 1.0f;
    v.setParams (p);
    auto l2 = saw (55.0, 48000, 0.9f), r2 = l2;
    run (v, l2, r2, 140.0);
    check (allFinite (l2) && allFinite (r2), "robot at full resonance stays bounded");
}

/** Like run(), with a mono sidechain key next to the audio. */
template <typename Module>
void runWithKey (Module& m, std::vector<float>& l, std::vector<float>& r, const std::vector<float>* key, double bpm,
                 int block = 256)
{
    double ppq = 0.0;
    for (size_t start = 0; start < l.size(); start += (size_t) block)
    {
        const int n = (int) std::min ((size_t) block, l.size() - start);
        auto t = transportAt (bpm, ppq);
        m.process (l.data() + start, r.data() + start, n, t, key != nullptr ? key->data() + start : nullptr);
        ppq += n / t.samplesPerBeat (sr);
    }
}

/** Kick-like key: a 60 Hz thump on every beat at 120 BPM (24000 samples). */
std::vector<float> kicks (size_t length, size_t first = 1000)
{
    std::vector<float> v (length, 0.0f);
    for (size_t i = first; i < length; i += 24000)
        for (size_t k = 0; k < 6000 && i + k < length; ++k)
            v[i + k] = 0.9f * (float) std::exp (-(double) k / 1500.0) * (float) std::sin (twoPi * 60.0 * (double) k / sr);
    return v;
}

void sidechainTests()
{
    // Gate band in sidechain mode ducks on each kick and opens again between them.
    PolimetarGate::Params p;
    for (auto& band : p.bands) { band.sidechain = true; band.depth = 1.0f; band.attackMs = 1.0f; band.releaseMs = 40.0f; }
    PolimetarGate g;
    g.prepare (sr, 256);
    g.setParams (p);
    g.reset();
    const size_t len = (size_t) sr * 2;
    const auto key = kicks (len);
    auto l = sine (1000.0, len), r = l;
    runWithKey (g, l, r, &key, 120.0);
    check (allFinite (l), "sidechain gate output is finite");
    check (rms (l, 25000, 26500) < 0.1, "sidechain gate ducks on the kick");
    check (rms (l, 40000, 48000) > 0.3, "sidechain gate opens between kicks");

    // Without a sidechain the band stays open.
    g.reset();
    auto l2 = sine (1000.0, len), r2 = l2;
    runWithKey (g, l2, r2, nullptr, 120.0);
    check (rms (l2, 25000, 26500) > 0.3, "sidechain gate with no key stays open");

    // Zap listening to the key: fires on the kicks although the input has no hits.
    ZapResonator z;
    z.prepare (sr, 256);
    ZapResonator::Params zp;
    zp.mix = 1.0f;
    zp.listenToKey = true;
    z.setParams (zp);
    z.reset();
    auto zl = saw (110.0, len, 0.3f), zr = zl;
    runWithKey (z, zl, zr, &key, 120.0);
    check (z.getHitCount() == 4, "zap listening to the sidechain fires on each kick");
    z.reset();
    auto zl2 = saw (110.0, len, 0.3f), zr2 = zl2;
    zp.listenToKey = false;
    z.setParams (zp);
    runWithKey (z, zl2, zr2, &key, 120.0);
    check (z.getHitCount() <= 1, "zap listening to the input ignores the sidechain");
}

void gateTests()
{
    PolimetarGate::Band b;
    b.euclidMode = true; b.steps = 4; b.hits = 1; b.division = 2; b.length = 0.5f; // 1/16 steps
    int step = -1;
    check (PolimetarGate::isOpen (b, 0.0, &step) && step == 0, "first step opens");
    check (! PolimetarGate::isOpen (b, 0.2, &step), "gate closes after its length");
    check (! PolimetarGate::isOpen (b, 0.3, &step) && step == 1, "step two is a rest");
    check (PolimetarGate::isOpen (b, 1.0, &step) && step == 0, "pattern repeats every 4 sixteenths");

    // Polymeter: 7 and 16 step bands meet again only after 112 sixteenths.
    PolimetarGate::Band b7 = b, b16 = b;
    b7.steps = 7; b16.steps = 16;
    int s7 = 0, s16 = 0, firstRealign = -1;
    for (int i = 1; i < 200; ++i)
    {
        PolimetarGate::isOpen (b7, i * 0.25 + 0.01, &s7);
        PolimetarGate::isOpen (b16, i * 0.25 + 0.01, &s16);
        if (s7 == 0 && s16 == 0) { firstRealign = i; break; }
    }
    check (firstRealign == 112, "7 against 16 lines up after 112 steps");

    // All bands open: output equals the (all-pass) split of the input, level kept.
    PolimetarGate g;
    g.prepare (sr, 256);
    PolimetarGate::Params p;
    for (auto& band : p.bands) { band.euclidMode = true; band.steps = 1; band.hits = 1; band.length = 1.0f; }
    g.setParams (p);
    g.reset();
    auto l = sine (1000.0, 24000), r = l;
    run (g, l, r, 140.0);
    check (near (rms (l, 6000, 24000), rms (sine (1000.0, 24000), 6000, 24000), 0.01), "open gate keeps the level");

    // Depth 1 with no hits silences the band.
    PolimetarGate g2;
    g2.prepare (sr, 256);
    for (auto& band : p.bands) { band.hits = 0; band.depth = 1.0f; }
    g2.setParams (p);
    g2.reset();
    auto l2 = sine (1000.0, 24000), r2 = l2;
    run (g2, l2, r2, 140.0);
    check (rms (l2, 6000, 24000) < 0.001, "closed gate at full depth is silent");
}

void chainTests()
{
    Chain::Layout layout;
    layout.slots = { ModuleType::fraktalEcho, ModuleType::fraktalEcho, ModuleType::empty, ModuleType::vokalAcid };
    layout.bypass = { false, false, false, true };
    const auto e = layout.effective();
    check (e[0] == ModuleType::fraktalEcho && e[1] == ModuleType::empty, "a module sits in one slot only");
    check (e[3] == ModuleType::empty, "bypassed slot is skipped");

    // Layout change fades without a jump bigger than the signal.
    Chain c;
    c.prepare (sr, 256);
    Chain::Layout none;
    none.slots = { ModuleType::empty, ModuleType::empty, ModuleType::empty, ModuleType::empty };
    c.setLayout (none);
    c.reset();
    auto l = sine (440.0, 4800), r = l;
    run (c, l, r, 140.0);
    double maxErr = 0.0;
    auto ref = sine (440.0, 4800);
    for (size_t i = 0; i < l.size(); ++i) maxErr = std::max (maxErr, (double) std::abs (l[i] - ref[i]));
    check (maxErr < 1.0e-6, "empty chain passes audio untouched");

    c.setLayout (layout);
    auto l2 = sine (440.0, 9600), r2 = l2;
    run (c, l2, r2, 140.0, 128);
    double maxStep = 0.0;
    for (size_t i = 1; i < l2.size(); ++i) maxStep = std::max (maxStep, (double) std::abs (l2[i] - l2[i - 1]));
    check (maxStep < 0.1, "layout change has no click");
    check (c.activeLayout() == layout, "new layout is active after the fade");
}

void performanceTest()
{
    Chain c;
    c.prepare (sr, 256);
    Chain::Layout all;
    all.slots = { ModuleType::vokalAcid, ModuleType::polimetarGate, ModuleType::fraktalEcho, ModuleType::empty };
    c.setLayout (all);
    c.reset();
    FraktalEcho::Params ep;
    ep.generations = 4; ep.spread = 2;
    c.echo.setParams (ep);

    const size_t seconds = 10;
    auto l = saw (110.0, (size_t) sr * seconds), r = l;
    const auto start = std::chrono::steady_clock::now();
    run (c, l, r, 145.0);
    const double elapsed = std::chrono::duration<double> (std::chrono::steady_clock::now() - start).count();
    const double load = elapsed / (double) seconds * 100.0;
    std::printf ("CPU: all three modules = %.2f %% of one core at 48 kHz\n", load);
    check (allFinite (l), "full chain output is finite");

    Chain c2;
    c2.prepare (sr, 256);
    Chain::Layout two;
    two.slots = { ModuleType::zapResonator, ModuleType::orbitDoppler, ModuleType::spectralRatchet, ModuleType::empty };
    c2.setLayout (two);
    c2.reset();
    auto l2 = saw (110.0, (size_t) sr * seconds), r2 = l2;
    for (size_t i = 0; i < l2.size(); i += 6000) l2[i] = r2[i] = 1.0f; // 1/16 hits at 120 BPM
    const auto start2 = std::chrono::steady_clock::now();
    run (c2, l2, r2, 145.0);
    const double elapsed2 = std::chrono::duration<double> (std::chrono::steady_clock::now() - start2).count();
    std::printf ("CPU: Zap Resonator + Orbit Doppler + Spectral Ratchet = %.2f %% of one core at 48 kHz\n", elapsed2 / (double) seconds * 100.0);
    check (allFinite (l2) && allFinite (r2), "zap and orbit chain output is finite");
}

void zapTests()
{
    ZapResonator z;
    z.prepare (sr, 256);
    ZapResonator::Params p;
    p.mix = 1.0f;   // zaps only
    p.order = ZapResonator::up;
    z.setParams (p);

    // Clicks every 1/8 at 120 BPM (12000 samples apart).
    const size_t len = (size_t) sr * 2;
    std::vector<float> l (len, 0.0f);
    for (size_t i = 1000; i < len; i += 12000)
        for (size_t k = 0; k < 40 && i + k < len; ++k) l[i + k] = 0.8f * (float) std::exp (-(double) k / 8.0);
    auto r = l;
    std::vector<float> silenceL (len, 0.0f), silenceR (len, 0.0f);
    run (z, l, r, 120.0);
    check (allFinite (l) && allFinite (r), "zap output is finite");
    check (z.getHitCount() == 8, "every click fires one zap");
    check (rms (l, 13000 + 2400, 13000 + 4800) > 0.01, "a zap rings after the hit");
    const int last = z.getLastNote();
    bool inScale = false;
    for (int i = 0; i < ZapResonator::notesInRange (p); ++i) inScale = inScale || ZapResonator::noteForStep (p, i) == last;
    check (inScale, "zaps land on scale notes");
    check (ZapResonator::noteForStep (p, 7) == ZapResonator::noteForStep (p, 0) + 12, "seven steps up is an octave");

    // Fast hats: 1/16 at 145 BPM, every one fires.
    z.reset();
    std::vector<float> hl (len, 0.0f);
    int expected = 0;
    for (size_t i = 500; i < len; i += 4966, ++expected)
        for (size_t k = 0; k < 120 && i + k < len; ++k)
            hl[i + k] = 0.5f * (float) std::exp (-(double) k / 30.0) * (k % 2 == 0 ? 1.0f : -1.0f);
    auto hr = hl;
    run (z, hl, hr, 145.0);
    check ((int) z.getHitCount() == expected, "fast 1/16 hats all fire");

    z.reset();
    run (z, silenceL, silenceR, 120.0);
    check (z.getHitCount() == 0 && rms (silenceL, 0, len) < 1.0e-6, "silence fires no zaps");

    // Sustained saw with no hits: at most the first edge triggers.
    z.reset();
    auto sl = saw (110.0, len), sr2 = sl;
    run (z, sl, sr2, 120.0);
    check (z.getHitCount() <= 1, "a steady sound does not keep firing");

    // Mix 0 is a null test.
    ZapResonator n;
    n.prepare (sr, 256);
    p.mix = 0.0f;
    n.setParams (p);
    n.reset();
    auto a = saw (220.0, len), b = a, ref = a;
    for (size_t i = 0; i < len; i += 9000) a[i] = b[i] = ref[i] = 1.0f;
    run (n, a, b, 140.0);
    double err = 0.0;
    for (size_t i = 0; i < len; ++i) err = std::max (err, (double) std::abs (a[i] - ref[i]));
    check (err < 1.0e-6, "zap with mix 0 is a null test");

    // Extreme settings stay bounded.
    ZapResonator x;
    x.prepare (sr, 256);
    p = {};
    p.sweep = 48.0f; p.sweepMs = 5.0f; p.decayMs = 2000.0f; p.resonance = 0.0f; p.thresholdDb = -60.0f; p.mix = 1.0f;
    x.setParams (p);
    x.reset();
    auto nl = saw (3000.0, len, 1.0f), nr = nl;
    for (size_t i = 0; i < len; i += 2000) nl[i] = nr[i] = 1.0f;
    run (x, nl, nr, 175.0, 64);
    check (allFinite (nl) && allFinite (nr), "zap stays bounded at extreme settings");
}

/** Zero crossings per second in part of a signal. */
double crossingRate (const std::vector<float>& v, size_t from, size_t to)
{
    int count = 0;
    for (size_t i = from + 1; i < to; ++i)
        if ((v[i - 1] < 0.0f) != (v[i] < 0.0f)) ++count;
    return count * 0.5 * sr / (double) (to - from);
}

void orbitTests()
{
    // Circle path: phase 0..0.5 comes closer, 0.5..1 moves away.
    OrbitDoppler o;
    o.prepare (sr, 256);
    OrbitDoppler::Params p;
    p.path = OrbitDoppler::circle;
    p.rate = 6;          // 1 bar = 2 s at 120 BPM
    p.radius = 20.0f;
    p.doppler = 1.0f;
    p.mix = 1.0f;
    o.setParams (p);
    o.reset();
    const size_t lap = (size_t) sr * 2;
    auto l = sine (1000.0, lap), r = l;
    run (o, l, r, 120.0);
    check (allFinite (l) && allFinite (r), "orbit output is finite");
    const double closer = crossingRate (l, lap * 20 / 100, lap * 30 / 100);
    const double away = crossingRate (l, lap * 70 / 100, lap * 80 / 100);
    check (closer > 1010.0, "pitch rises while the sound comes closer");
    check (away < 990.0, "pitch falls while the sound moves away");
    check (rms (r, lap * 22 / 100, lap * 28 / 100) > rms (l, lap * 22 / 100, lap * 28 / 100) * 1.5,
           "a quarter of the way round the sound is on the right");
    check (rms (l, lap * 72 / 100, lap * 78 / 100) > rms (r, lap * 72 / 100, lap * 78 / 100) * 1.5,
           "three quarters of the way round it is on the left");

    // No Doppler: the pitch stays put.
    OrbitDoppler still;
    still.prepare (sr, 256);
    p.doppler = 0.0f;
    still.setParams (p);
    still.reset();
    auto sl = sine (1000.0, lap), sr2 = sl;
    run (still, sl, sr2, 120.0);
    check (std::abs (crossingRate (sl, lap * 20 / 100, lap * 30 / 100) - 1000.0) < 8.0, "Doppler 0 keeps the pitch");

    // Mix 0 is a null test; every path stays bounded.
    for (int path = 0; path < OrbitDoppler::numPaths; ++path)
    {
        OrbitDoppler n;
        n.prepare (sr, 256);
        p.path = path;
        p.radius = 30.0f;
        p.doppler = 1.0f;
        p.rate = 0;      // 1/16: as fast as it goes
        p.mix = 1.0f;
        n.setParams (p);
        n.reset();
        auto a = saw (220.0, lap), b = a;
        run (n, a, b, 180.0, 37);
        check (allFinite (a) && allFinite (b), "every orbit path stays bounded");
    }
    OrbitDoppler n;
    n.prepare (sr, 256);
    p.mix = 0.0f;
    n.setParams (p);
    n.reset();
    auto a = saw (220.0, lap), b = a, ref = a;
    run (n, a, b, 140.0);
    double err = 0.0;
    for (size_t i = 0; i < lap; ++i) err = std::max (err, (double) std::abs (a[i] - ref[i]));
    check (err < 1.0e-6, "orbit with mix 0 is a null test");
}

void tapeTests()
{
    // Manual brake, Stop mode, 1/2 note (1 s at 120 BPM).
    TapeStop t;
    t.prepare (sr, 256);
    TapeStop::Params p;
    p.mode = TapeStop::stop;
    p.length = 3;
    t.setParams (p);
    t.reset();
    const size_t sec = (size_t) sr;
    auto l = sine (1000.0, sec * 4), r = l;
    const auto ref = l;
    auto runSpan = [&] (size_t from, size_t to) {
        for (size_t s = from; s < to; s += 256)
        {
            const int n = (int) std::min<size_t> (256, to - s);
            t.process (l.data() + s, r.data() + s, n, transportAt (120.0, 0.0));
        }
    };
    runSpan (0, sec / 2);
    p.brake = true;
    t.setParams (p);
    runSpan (sec / 2, sec * 2);
    const double slowing = crossingRate (l, sec * 6 / 10, sec * 9 / 10);
    check (slowing < 900.0 && slowing > 200.0, "braking lowers the pitch");
    check (rms (l, sec * 16 / 10, sec * 2) < 1.0e-3, "after the slow down the tape is stopped");
    check (t.getSpeed() < 0.01f, "speed reaches zero");
    p.brake = false;
    t.setParams (p);
    runSpan (sec * 2, sec * 4);
    double err = 0.0;
    for (size_t i = sec * 2 + (size_t) (0.03 * sr); i < sec * 4; ++i) err = std::max (err, (double) std::abs (l[i] - ref[i]));
    check (err < 1.0e-6, "letting go of the brake goes straight back to the live sound");

    // Slow Down holds at half speed: one octave down.
    TapeStop h;
    h.prepare (sr, 256);
    p = {};
    p.mode = TapeStop::slowDown;
    p.length = 2;   // 1/4 = 0.5 s
    p.brake = true;
    h.setParams (p);
    h.reset();
    auto hl = sine (1000.0, sec * 2), hr = hl;
    run (h, hl, hr, 120.0);
    check (std::abs (crossingRate (hl, sec, sec * 2) - 500.0) < 10.0, "Slow Down holds an octave lower");
    check (allFinite (hl), "slow down output is finite");

    // Every 2 bars: stopped just before the bar line, live again after it.
    TapeStop a;
    a.prepare (sr, 256);
    p = {};
    p.mode = TapeStop::stop;
    p.trigger = TapeStop::every2;
    p.length = 3;   // 1/2 note
    a.setParams (p);
    a.reset();
    check (! TapeStop::autoEngaged (p, 5.0, 4.0) && TapeStop::autoEngaged (p, 7.0, 4.0), "auto trigger fires in the last half note of 2 bars");
    check (! TapeStop::autoEngaged (p, 8.1, 4.0), "auto trigger lets go on the bar line");
    auto al = sine (1000.0, sec * 5), ar = al;   // 2 bars = 4 s at 120 BPM
    const auto aref = al;
    run (a, al, ar, 120.0);
    check (crossingRate (al, sec * 38 / 10, sec * 4) < 450.0, "the tape has nearly stopped at the end of bar 2");
    double aerr = 0.0;
    for (size_t i = sec * 4 + (size_t) (0.03 * sr); i < sec * 5; ++i) aerr = std::max (aerr, (double) std::abs (al[i] - aref[i]));
    check (aerr < 1.0e-6, "the next bar starts live");

    // Start mode spins up from a stop.
    TapeStop st;
    st.prepare (sr, 256);
    p = {};
    p.mode = TapeStop::start;
    p.length = 3;
    st.setParams (p);
    st.reset();
    auto sl = sine (1000.0, sec * 2), sr2 = sl;
    for (size_t s = 0; s < sec * 2; s += 256)
    {
        if (! p.brake && s >= sec / 2) { p.brake = true; st.setParams (p); }
        st.process (sl.data() + s, sr2.data() + s, 256, transportAt (120.0, 0.0));
    }
    check (crossingRate (sl, sec * 6 / 10, sec * 8 / 10) < 500.0, "Start begins at a low pitch");
    check (crossingRate (sl, sec * 12 / 10, sec * 14 / 10) > crossingRate (sl, sec * 6 / 10, sec * 8 / 10) + 200.0, "Start spins up");
    check (std::abs (crossingRate (sl, sec * 17 / 10, sec * 2) - 1000.0) < 10.0, "after spinning up the pitch is normal");
    check (allFinite (sl), "start output is finite");

    // Mix 0 is a null test.
    TapeStop n;
    n.prepare (sr, 256);
    p = {};
    p.mix = 0.0f;
    p.brake = true;
    n.setParams (p);
    n.reset();
    auto nl = saw (220.0, sec), nr = nl, nref = nl;
    run (n, nl, nr, 140.0);
    double nerr = 0.0;
    for (size_t i = 0; i < sec; ++i) nerr = std::max (nerr, (double) std::abs (nl[i] - nref[i]));
    check (nerr < 1.0e-6, "tape stop with mix 0 is a null test");
}

void ratchetTests()
{
    // FFT round trip.
    Fft f;
    f.prepare (8);
    std::vector<std::complex<float>> x (256), y;
    for (int i = 0; i < 256; ++i) x[(size_t) i] = { (float) std::sin (i * 0.37) + 0.2f * (float) (i % 7), 0.0f };
    y = x;
    f.perform (y.data(), false);
    check (std::abs (y[0].real() - [&] { float s = 0; for (auto& v : x) s += v.real(); return s; }()) < 1e-3f, "FFT bin 0 is the sum");
    f.perform (y.data(), true);
    float ferr = 0.0f;
    for (int i = 0; i < 256; ++i) ferr = std::max (ferr, std::abs (y[(size_t) i] - x[(size_t) i]));
    check (ferr < 1e-4f, "FFT forward and inverse give the input back");

    const int L = SpectralRatchet::latency;
    auto delayedError = [&] (const std::vector<float>& out, const std::vector<float>& in, size_t from) {
        double e = 0.0;
        for (size_t i = from; i < out.size(); ++i) e = std::max (e, (double) std::abs (out[i] - in[i - (size_t) L]));
        return e;
    };

    // All pass steps: the sound comes through unchanged, 2048 samples late.
    SpectralRatchet r;
    r.prepare (sr, 256);
    SpectralRatchet::Params p;
    p.pattern = 0u;
    r.setParams (p);
    r.reset();
    auto l = saw (220.0, (size_t) sr), rr = l;
    const auto in = l;
    run (r, l, rr, 140.0);
    check (delayedError (l, in, (size_t) L * 3) < 2.0e-3, "pass steps are transparent apart from the latency");

    // Mix 0: exactly the dry signal, delayed by the same latency.
    SpectralRatchet d;
    d.prepare (sr, 256);
    p.pattern = 0xffffu;
    p.mix = 0.0f;
    d.setParams (p);
    d.reset();
    auto dl = saw (220.0, (size_t) sr), dr = dl;
    run (d, dl, dr, 140.0);
    check (delayedError (dl, in, (size_t) L) < 1.0e-6, "mix 0 is the dry signal delayed by the latency");

    // Freeze holds the sound after the input stops.
    SpectralRatchet fr;
    fr.prepare (sr, 256);
    p = {};
    p.pattern = 0xffffu;   // every step freezes
    p.steps = 16;
    p.division = 2;        // 1/16 at 120 BPM: the pattern is 2 s, one capture at its start
    p.interval = SpectralRatchet::same;
    p.smear = 0.0f;
    fr.setParams (p);
    fr.reset();
    auto fl = sine (440.0, (size_t) sr * 2), frr = fl;
    for (size_t i = (size_t) (sr * 0.5); i < fl.size(); ++i) fl[i] = frr[i] = 0.0f;
    run (fr, fl, frr, 120.0);
    check (rms (fl, (size_t) (sr * 1.0), (size_t) (sr * 1.5)) > 0.05, "a frozen spectrum keeps sounding after the input stops");
    check (std::abs (crossingRate (fl, (size_t) (sr * 1.0), (size_t) (sr * 1.5)) - 440.0) < 15.0, "the frozen sound keeps its pitch");
    check (allFinite (fl) && allFinite (frr), "freeze output is finite");

    // Octaves up: the second repeat is an octave higher.
    check (SpectralRatchet::intervalFor (p, 0) == 0, "the first repeat is as captured");
    p.interval = SpectralRatchet::octaveUp;
    check (SpectralRatchet::intervalFor (p, 1) == 12 && SpectralRatchet::intervalFor (p, 2) == 24, "octaves up climbs by octaves");
    p.interval = SpectralRatchet::scaleUp;
    check (SpectralRatchet::intervalFor (p, 1) == 1, "scale up in phrygian starts with a semitone");

    SpectralRatchet oc;
    oc.prepare (sr, 256);
    p = {};
    p.pattern = 0x0006u;   // steps 2 and 3 freeze
    p.steps = 4;
    p.division = 8;        // 1/4 at 120 BPM = 0.5 s per step
    p.interval = SpectralRatchet::octaveUp;
    p.smear = 0.0f;
    oc.setParams (p);
    oc.reset();
    auto ol = sine (500.0, (size_t) sr * 2), orr = ol;
    run (oc, ol, orr, 120.0);
    // Output is 2048 samples late: step 3 (repeat 1) plays from 1.0 s + latency.
    const size_t s3 = (size_t) (sr * 1.0) + (size_t) L;
    check (std::abs (crossingRate (ol, s3 + 4000, s3 + 20000) - 1000.0) < 25.0, "the second repeat is an octave up");

    // Extreme settings stay bounded.
    SpectralRatchet x2;
    x2.prepare (sr, 256);
    p = {};
    p.pattern = 0xA5A5u; p.steps = 16; p.division = 0; p.interval = SpectralRatchet::randomScale;
    p.mask = SpectralRatchet::walk; p.smear = 1.0f; p.tilt = 1.0f;
    x2.setParams (p);
    x2.reset();
    auto xl = saw (3000.0, (size_t) sr * 2, 1.0f), xr = xl;
    run (x2, xl, xr, 180.0, 61);
    check (allFinite (xl) && allFinite (xr), "spectral ratchet stays bounded at extreme settings");
}

void dryWetTests()
{
    DryWet dw;
    dw.prepare (48000.0, 8, 64);
    dw.setMix (0.0f);
    dw.reset();
    dw.setDelay (3);
    std::vector<float> in (200), l (64), r (64), out;
    for (size_t i = 0; i < in.size(); ++i) in[i] = (float) i + 1.0f;
    for (size_t start = 0; start < in.size(); start += 50)
    {
        const int n = (int) std::min<size_t> (50, in.size() - start);
        std::copy (in.begin() + (long) start, in.begin() + (long) start + n, l.begin());
        std::copy (l.begin(), l.begin() + n, r.begin());
        dw.pushDry (l.data(), r.data(), n);
        for (int i = 0; i < n; ++i) l[(size_t) i] = r[(size_t) i] = -100.0f; // the "effect"
        dw.blend (l.data(), r.data(), n);
        out.insert (out.end(), l.begin(), l.begin() + n);
    }
    bool delayed = out[0] == 0.0f && out[2] == 0.0f;
    for (size_t i = 3; i < out.size(); ++i) delayed = delayed && out[i] == in[i - 3];
    check (delayed, "dry is delayed by the chain latency at mix 0");

    dw.setMix (1.0f);
    dw.reset();
    l.assign (64, 0.25f);
    dw.pushDry (in.data(), nullptr, 64);
    dw.blend (l.data(), nullptr, 64);
    check (l[10] == 0.25f, "mix 1 is only the effect");
}

void modulationTests()
{
    // LFO 1/4 rate: one cycle per quarter note, locked to the song position.
    check (near (Lfo::value (Lfo::sine, 4, 0.25), 1.0, 1e-5), "sine LFO peaks a quarter of the way through");
    check (near (Lfo::value (Lfo::sine, 4, 3.25), 1.0, 1e-5), "LFO repeats every cycle");
    check (near (Lfo::value (Lfo::triangle, 4, 0.5), 1.0, 1e-5), "triangle peaks half way");
    check (near (Lfo::value (Lfo::sawUp, 6, 2.0), 0.0, 1e-5), "1 bar saw is at zero half way through the bar");
    check (Lfo::value (Lfo::square, 4, 0.1) > 0.0f && Lfo::value (Lfo::square, 4, 0.6) < 0.0f, "square flips half way");
    bool same = true, varies = false, inRange = true;
    for (int c = 0; c < 64; ++c)
    {
        const float a = Lfo::value (Lfo::sampleHold, 4, c + 0.1), b = Lfo::value (Lfo::sampleHold, 4, c + 0.9);
        same = same && near (a, b, 1e-9);
        varies = varies || ! near (a, Lfo::value (Lfo::sampleHold, 4, 0.5), 1e-3);
        inRange = inRange && a >= -1.0f && a <= 1.0f;
    }
    check (same, "random LFO holds its value for a whole cycle");
    check (varies, "random LFO changes between cycles");
    check (inRange, "random LFO stays in -1..1");

    ModMatrix m;
    check (m.connect (ModSource::macro1, 3, 0.5f) == 0, "first connection takes slot 0");
    check (m.connect (ModSource::macro1, 3, 0.9f) == 0, "connecting the same pair again reuses its slot");
    m.connect (ModSource::lfo1, 3, -0.25f);
    std::array<float, numModSources> values {};
    values[(size_t) ModSource::macro1] = 1.0f;
    values[(size_t) ModSource::lfo1] = 0.5f;
    check (near (m.offsetFor (3, values), 0.5 - 0.125, 1e-6), "offset sums depth x source value");
    check (near (m.offsetFor (4, values), 0.0, 1e-9), "other parameters are untouched");
    m.setDepth (0, 3.0f);
    check (near (m.get (0).depth, 1.0, 1e-9), "depth is clamped to 1");
    for (int i = 0; i < ModMatrix::numSlots; ++i) m.connect (ModSource::envelope, 10 + i, 0.1f);
    check (m.connect (ModSource::fraza, 99, 0.1f) == -1, "a full matrix refuses new connections");
    m.clearAll();
    check (! m.get (0).used() && near (m.offsetFor (3, values), 0.0, 1e-9), "clear all removes every connection");

    // Direction: LFOs start bipolar, the other sources one way.
    ModMatrix d;
    const int lfoSlot = d.connect (ModSource::lfo1, 1, 0.5f);
    const int envSlot = d.connect (ModSource::envelope, 2, 0.5f);
    check (d.get (lfoSlot).bipolar && ! d.get (envSlot).bipolar, "LFOs start bipolar, other sources one way");
    std::array<float, numModSources> v {};
    v[(size_t) ModSource::lfo1] = -1.0f;
    v[(size_t) ModSource::envelope] = 0.0f;
    check (near (d.offsetFor (1, v), -0.5, 1e-6), "bipolar LFO swings below the knob");
    d.setBipolar (lfoSlot, false);
    check (near (d.offsetFor (1, v), 0.0, 1e-6), "one-way LFO stays at the knob at its low point");
    v[(size_t) ModSource::lfo1] = 1.0f;
    check (near (d.offsetFor (1, v), 0.5, 1e-6), "one-way LFO reaches the full depth at its high point");
    d.setBipolar (envSlot, true);
    check (near (d.offsetFor (2, v), -0.5, 1e-6), "bipolar envelope swings below the knob when quiet");
    d.setDepth (envSlot, -0.5f);
    d.setBipolar (envSlot, false);
    v[(size_t) ModSource::envelope] = 1.0f;
    check (near (d.offsetFor (2, v), -0.5, 1e-6), "one-way down goes below the knob");

    EnvelopeFollower env;
    env.prepare (sr);
    std::vector<float> loud ((size_t) sr / 10, 1.0f), quiet ((size_t) sr, 0.0f);
    env.process (loud.data(), loud.data(), (int) loud.size());
    check (env.value() > 0.95f, "envelope follows a full scale signal");
    env.process (quiet.data(), nullptr, (int) quiet.size());
    check (env.value() < 0.05f, "envelope falls back in silence");
}
} // namespace

int main()
{
    scaleTests();
    euclidTests();
    frazaTests();
    dryWetTests();
    echoTests();
    vokalTests();
    gateTests();
    chainTests();
    modulationTests();
    zapTests();
    orbitTests();
    tapeTests();
    sidechainTests();
    ratchetTests();
    performanceTest();

    if (failures == 0) std::printf ("All DSP tests passed.\n");
    else std::printf ("%d DSP test(s) failed.\n", failures);
    return failures == 0 ? 0 : 1;
}
