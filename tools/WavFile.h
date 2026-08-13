#pragma once

// Minimal wav reader and writer.
//
// Deliberately dependency-free: the console renderer must build without JUCE
// and without pulling anything from the network. Handles the formats that
// actually turn up in a music folder - 16, 24 and 32-bit PCM, and 32-bit
// float - mono or stereo. Anything else is rejected with a clear message.

#include <cstdint>
#include <cstring>
#include <fstream>
#include <string>
#include <vector>

namespace wavio
{

struct AudioFile
{
    std::vector<float> left;
    std::vector<float> right;
    int sampleRate = 44100;
    bool wasMono = false;

    int numSamples() const { return static_cast<int> (left.size()); }
};

namespace detail
{
    inline uint32_t readU32 (const unsigned char* p)
    {
        return static_cast<uint32_t> (p[0])
             | (static_cast<uint32_t> (p[1]) << 8)
             | (static_cast<uint32_t> (p[2]) << 16)
             | (static_cast<uint32_t> (p[3]) << 24);
    }

    inline uint16_t readU16 (const unsigned char* p)
    {
        return static_cast<uint16_t> (p[0] | (p[1] << 8));
    }

    inline void writeU32 (std::ofstream& out, uint32_t v)
    {
        unsigned char b[4] { static_cast<unsigned char> (v & 0xff),
                             static_cast<unsigned char> ((v >> 8) & 0xff),
                             static_cast<unsigned char> ((v >> 16) & 0xff),
                             static_cast<unsigned char> ((v >> 24) & 0xff) };
        out.write (reinterpret_cast<char*> (b), 4);
    }

    inline void writeU16 (std::ofstream& out, uint16_t v)
    {
        unsigned char b[2] { static_cast<unsigned char> (v & 0xff),
                             static_cast<unsigned char> ((v >> 8) & 0xff) };
        out.write (reinterpret_cast<char*> (b), 2);
    }
}

// Returns false and fills 'error' on failure.
inline bool load (const std::string& path, AudioFile& file, std::string& error)
{
    std::ifstream in (path, std::ios::binary);

    if (! in)
    {
        error = "cannot open " + path;
        return false;
    }

    std::vector<unsigned char> bytes ((std::istreambuf_iterator<char> (in)),
                                       std::istreambuf_iterator<char>());

    if (bytes.size() < 44
        || std::memcmp (bytes.data(), "RIFF", 4) != 0
        || std::memcmp (bytes.data() + 8, "WAVE", 4) != 0)
    {
        error = path + " is not a RIFF/WAVE file";
        return false;
    }

    uint16_t formatTag = 0, numChannels = 0, bitsPerSample = 0;
    size_t dataOffset = 0, dataSize = 0;
    bool haveFmt = false;

    // Walk the chunk list rather than assuming a 44-byte header - real files
    // carry LIST, bext and other chunks before the data.
    size_t pos = 12;

    while (pos + 8 <= bytes.size())
    {
        const char* id = reinterpret_cast<const char*> (bytes.data() + pos);
        const uint32_t chunkSize = detail::readU32 (bytes.data() + pos + 4);
        const size_t body = pos + 8;

        if (std::memcmp (id, "fmt ", 4) == 0 && body + 16 <= bytes.size())
        {
            formatTag     = detail::readU16 (bytes.data() + body);
            numChannels   = detail::readU16 (bytes.data() + body + 2);
            file.sampleRate = static_cast<int> (detail::readU32 (bytes.data() + body + 4));
            bitsPerSample = detail::readU16 (bytes.data() + body + 14);
            haveFmt = true;
        }
        else if (std::memcmp (id, "data", 4) == 0)
        {
            dataOffset = body;
            dataSize = std::min (static_cast<size_t> (chunkSize), bytes.size() - body);
        }

        pos = body + chunkSize + (chunkSize & 1);   // chunks are word-aligned
    }

    if (! haveFmt || dataSize == 0)
    {
        error = path + ": missing fmt or data chunk";
        return false;
    }

    if (numChannels < 1 || numChannels > 2)
    {
        error = path + ": only mono or stereo supported, found "
              + std::to_string (numChannels) + " channels";
        return false;
    }

    // 1 = PCM, 3 = IEEE float, 0xFFFE = extensible (subformat assumed PCM).
    const bool isFloat = (formatTag == 3);

    if (formatTag != 1 && formatTag != 3 && formatTag != 0xFFFE)
    {
        error = path + ": unsupported wav format tag "
              + std::to_string (formatTag) + " (compressed files not supported)";
        return false;
    }

    const int bytesPerSample = bitsPerSample / 8;

    if (bytesPerSample < 2 || bytesPerSample > 4)
    {
        error = path + ": unsupported bit depth " + std::to_string (bitsPerSample);
        return false;
    }

    const size_t frameSize = static_cast<size_t> (bytesPerSample) * numChannels;
    const size_t numFrames = dataSize / frameSize;

    file.left.resize (numFrames);
    file.right.resize (numFrames);
    file.wasMono = (numChannels == 1);

    for (size_t frame = 0; frame < numFrames; ++frame)
    {
        for (int ch = 0; ch < numChannels; ++ch)
        {
            const unsigned char* p = bytes.data() + dataOffset
                                   + frame * frameSize
                                   + static_cast<size_t> (ch) * bytesPerSample;
            float value = 0.0f;

            if (isFloat && bytesPerSample == 4)
            {
                std::memcpy (&value, p, 4);
            }
            else if (bytesPerSample == 2)
            {
                const int16_t s = static_cast<int16_t> (detail::readU16 (p));
                value = static_cast<float> (s) / 32768.0f;
            }
            else if (bytesPerSample == 3)
            {
                int32_t s = (static_cast<int32_t> (p[0]))
                          | (static_cast<int32_t> (p[1]) << 8)
                          | (static_cast<int32_t> (p[2]) << 16);

                if (s & 0x800000)         // sign-extend 24-bit
                    s |= ~0xffffff;

                value = static_cast<float> (s) / 8388608.0f;
            }
            else
            {
                const int32_t s = static_cast<int32_t> (detail::readU32 (p));
                value = static_cast<float> (s) / 2147483648.0f;
            }

            (ch == 0 ? file.left : file.right)[frame] = value;
        }

        if (numChannels == 1)
            file.right[frame] = file.left[frame];
    }

    return true;
}

// Always writes 32-bit float. Keeps headroom during tuning so nothing clips
// on the way out and comparisons stay honest.
inline bool save (const std::string& path, const AudioFile& file, std::string& error)
{
    std::ofstream out (path, std::ios::binary);

    if (! out)
    {
        error = "cannot write " + path;
        return false;
    }

    const uint16_t channels = 2;
    const uint16_t bits = 32;
    const uint32_t frames = static_cast<uint32_t> (file.left.size());
    const uint32_t byteRate = static_cast<uint32_t> (file.sampleRate) * channels * (bits / 8);
    const uint32_t dataSize = frames * channels * (bits / 8);

    out.write ("RIFF", 4);
    detail::writeU32 (out, 36 + dataSize);
    out.write ("WAVE", 4);

    out.write ("fmt ", 4);
    detail::writeU32 (out, 16);
    detail::writeU16 (out, 3);                                  // IEEE float
    detail::writeU16 (out, channels);
    detail::writeU32 (out, static_cast<uint32_t> (file.sampleRate));
    detail::writeU32 (out, byteRate);
    detail::writeU16 (out, channels * (bits / 8));
    detail::writeU16 (out, bits);

    out.write ("data", 4);
    detail::writeU32 (out, dataSize);

    for (uint32_t n = 0; n < frames; ++n)
    {
        const float l = file.left[n];
        const float r = file.right[n];
        out.write (reinterpret_cast<const char*> (&l), 4);
        out.write (reinterpret_cast<const char*> (&r), 4);
    }

    return out.good();
}

} // namespace wavio
