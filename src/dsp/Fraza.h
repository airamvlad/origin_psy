#pragma once

#include "Common.h"

namespace originpsy
{

/** Fraza: a modulation source that knows where the song is inside an 8, 16
    or 32 bar phrase. Hook its curve onto parameters and the build-up plays
    itself, with no automation lanes. */
class Fraza
{
public:
    enum Curve { rise = 0, riseExp, lastBar, last2Bars, last4Bars, fall, pulse };
    static constexpr int numCurves = 7;

    struct Params
    {
        int bars = 16;     // 8, 16 or 32
        int offset = 0;    // bars the phrase is shifted by, 0..31
        int curve = rise;
    };

    /** Position inside the phrase, 0..1. */
    static double phrasePosition (const Params& p, double ppq, double beatsPerBar) noexcept
    {
        const double bars = std::max (1, p.bars);
        const double bar = ppq / std::max (0.5, beatsPerBar) - p.offset;
        const double pos = std::fmod (bar, bars) / bars;
        return pos < 0.0 ? pos + 1.0 : pos;
    }

    /** Curve value 0..1 at a phrase position. */
    static float evaluate (const Params& p, double position) noexcept
    {
        const double bars = std::max (1, p.bars);
        const double barsIn = position * bars;
        auto tail = [&] (double lastBars) {
            const double start = bars - lastBars;
            return barsIn < start ? 0.0 : (barsIn - start) / lastBars;
        };

        double v = 0.0;
        switch (p.curve)
        {
            case rise:      v = position; break;
            case riseExp:   v = position * position * position; break;
            case lastBar:   v = tail (1.0); break;
            case last2Bars: v = tail (2.0); break;
            case last4Bars: v = tail (4.0); break;
            case fall:      v = 1.0 - position; break;
            case pulse:     v = std::fmod (barsIn, 4.0) >= 3.0 ? 1.0 : 0.0; break; // last bar of every 4
            default:        v = position; break;
        }
        return (float) std::clamp (v, 0.0, 1.0);
    }

    static float valueAt (const Params& p, const Transport& t) noexcept
    {
        return evaluate (p, phrasePosition (p, t.ppqAtBlockStart, t.beatsPerBar));
    }
};

} // namespace originpsy
