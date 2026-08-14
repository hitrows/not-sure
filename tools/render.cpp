// notsure-render
//
// Runs LimiterCore over a wav file. Nothing here touches JUCE, so the build is
// a couple of seconds and the tuning loop is a single command.
//
//   notsure-render in.wav out.wav --crush 7 --crunch 5 --sag 9 --quality 4
//
// Sweep mode renders one file per value, which is the fastest way to hear what
// a control actually does:
//
//   notsure-render loop.wav sag.wav --sweep sag 0 10 6

#include "../Source/dsp/LimiterCore.h"
#include "WavFile.h"

#include <chrono>
#include <cstdlib>
#include <iostream>
#include <string>

namespace
{
    void printUsage()
    {
        std::cout
            << "usage: notsure-render <in.wav> <out.wav> [options]\n\n"
            << "  --crush  <0..10>      drive into the limiter   (default 0)\n"
            << "  --crunch <0..10>      drive into output stage  (default 0)\n"
            << "  --sag    <0..10>      release stretch          (default 0)\n"
            << "  --attack <ms>         0.3 / 1.3 / 4.0          (default 1.3)\n"
            << "  --mix    <0..100>     parallel dry/wet         (default 100)\n"
            << "  --trim   <dB>         output trim              (default 0)\n"
            << "  --darkness <0..10>   post-distortion tilt      (default 0)\n"
            << "  --autogain <0|1>     loudness compensation     (default 1)\n"
            << "  --quality <1|2|4>     oversampling; 1 aliases   (default 4)\n"
            << "  --offline            simulate an offline bounce (max factor)\n"
            << "  --rate <hz>          override the processing sample rate\n"
            << "  --sweep  <param> <from> <to> <steps>\n"
            << "                        render one file per value; output names\n"
            << "                        get a numeric suffix\n";
    }

    std::string suffixed (const std::string& path, int index, float value)
    {
        const auto dot = path.find_last_of ('.');
        const std::string stem = (dot == std::string::npos) ? path : path.substr (0, dot);
        const std::string ext  = (dot == std::string::npos) ? ".wav" : path.substr (dot);

        char buffer[32];
        std::snprintf (buffer, sizeof (buffer), "_%d_%.2f", index, value);

        return stem + buffer + ext;
    }

    bool renderOnce (const wavio::AudioFile& source,
                     const notsure::LimiterCore::Params& params,
                     const std::string& outPath,
                     double rateOverride = 0.0)
    {
        wavio::AudioFile result = source;

        // --rate overrides the sample rate the core runs at, so the sample-rate
        // oversampling cap can be exercised without needing files at 96/192 kHz.
        const double rate = rateOverride > 0.0 ? rateOverride
                                               : static_cast<double> (source.sampleRate);

        notsure::LimiterCore core;
        core.prepare (rate);
        core.setParams (params);

        // Process in blocks so the console path exercises the same code path a
        // host would. 512 is a common host buffer size.
        constexpr int blockSize = 512;
        const int total = result.numSamples();

        for (int start = 0; start < total; start += blockSize)
        {
            const int count = std::min (blockSize, total - start);
            core.process (result.left.data() + start,
                          result.right.data() + start,
                          count);
        }

        std::string error;

        if (! wavio::save (outPath, result, error))
        {
            std::cerr << "error: " << error << "\n";
            return false;
        }

        // Report the factor actually running (after cap + offline boost) and the
        // fixed latency, so Parts 1-6 of OVERSAMPLING-SPEC can be verified here.
        std::cout << "wrote " << outPath
                  << "  [rate " << (long) rate << " Hz"
                  << ", quality " << params.quality << "x"
                  << (params.offline ? ", offline" : "")
                  << " -> running " << core.getEffectiveFactor() << "x"
                  << ", latency " << core.getLatencySamples() << "]\n";
        return true;
    }
}

int main (int argc, char** argv)
{
    if (argc < 3)
    {
        printUsage();
        return 1;
    }

    const std::string inPath  = argv[1];
    const std::string outPath = argv[2];

    notsure::LimiterCore::Params params;

    std::string sweepParam;
    float sweepFrom = 0.0f, sweepTo = 0.0f;
    int sweepSteps = 0;
    double rateOverride = 0.0;

    for (int i = 3; i < argc; ++i)
    {
        const std::string arg = argv[i];

        auto nextFloat = [&] () -> float
        {
            return (i + 1 < argc) ? std::strtof (argv[++i], nullptr) : 0.0f;
        };

        if      (arg == "--crush")  params.crush    = nextFloat();
        else if (arg == "--crunch") params.crunch   = nextFloat();
        else if (arg == "--darkness") params.darkness = nextFloat();
        else if (arg == "--autogain") params.autoGain = (nextFloat() != 0.0f);
        else if (arg == "--quality") params.quality = static_cast<int> (nextFloat());
        else if (arg == "--offline") params.offline = true;
        else if (arg == "--rate")    rateOverride  = static_cast<double> (nextFloat());
        else if (arg == "--sag")    params.sag      = nextFloat();
        else if (arg == "--attack") params.attackMs = nextFloat();
        else if (arg == "--mix")    params.mix      = nextFloat();
        else if (arg == "--trim")   params.trimDb   = nextFloat();
        else if (arg == "--sweep")
        {
            if (i + 4 >= argc)
            {
                std::cerr << "error: --sweep needs <param> <from> <to> <steps>\n";
                return 1;
            }

            sweepParam = argv[++i];
            sweepFrom  = std::strtof (argv[++i], nullptr);
            sweepTo    = std::strtof (argv[++i], nullptr);
            sweepSteps = std::atoi   (argv[++i]);
        }
        else
        {
            std::cerr << "error: unknown option " << arg << "\n\n";
            printUsage();
            return 1;
        }
    }

    wavio::AudioFile source;
    std::string error;

    if (! wavio::load (inPath, source, error))
    {
        std::cerr << "error: " << error << "\n";
        return 1;
    }

    std::cout << "loaded " << inPath << " - "
              << source.numSamples() << " frames at "
              << source.sampleRate << " Hz"
              << (source.wasMono ? " (mono, duplicated to stereo)" : " (stereo)")
              << "\n";

    const auto started = std::chrono::steady_clock::now();
    bool ok = true;

    if (sweepSteps > 1)
    {
        for (int step = 0; step < sweepSteps; ++step)
        {
            const float t = static_cast<float> (step) / static_cast<float> (sweepSteps - 1);
            const float value = sweepFrom + (sweepTo - sweepFrom) * t;

            if      (sweepParam == "crush")  params.crush    = value;
            else if (sweepParam == "crunch") params.crunch   = value;
            else if (sweepParam == "darkness") params.darkness = value;
            else if (sweepParam == "sag")    params.sag      = value;
            else if (sweepParam == "attack") params.attackMs = value;
            else if (sweepParam == "mix")    params.mix      = value;
            else if (sweepParam == "trim")   params.trimDb   = value;
            else
            {
                std::cerr << "error: cannot sweep unknown parameter "
                          << sweepParam << "\n";
                return 1;
            }

            ok = renderOnce (source, params, suffixed (outPath, step, value), rateOverride) && ok;
        }
    }
    else
    {
        ok = renderOnce (source, params, outPath, rateOverride);
    }

    const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds> (
                             std::chrono::steady_clock::now() - started).count();

    std::cout << "done in " << elapsed << " ms\n";

    return ok ? 0 : 1;
}
