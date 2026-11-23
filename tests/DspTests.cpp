#include "dsp/Chain.h"
#include "dsp/Fraza.h"
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
    echoTests();
    vokalTests();
    gateTests();
    chainTests();
    modulationTests();
    performanceTest();

    if (failures == 0) std::printf ("All DSP tests passed.\n");
    else std::printf ("%d DSP test(s) failed.\n", failures);
    return failures == 0 ? 0 : 1;
}
