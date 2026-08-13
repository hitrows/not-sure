#pragma once

#include <array>

// ---------------------------------------------------------------------------
// Preset table, in one place so it stays easy to edit.
//
// Presets are NOT exposed as AU factory programs (that produced a redundant flat
// "AU Presets" list in Logic). They ship instead as .aupreset files in category
// folders under ~/Library/Audio/Presets/Hitrows/Not Sure/<category>/, which
// Logic groups in its Settings menu. tools/make-presets.mm reads this table and
// writes those files. Nothing in the plugin itself includes this header.
//
// autogain is on and bypass off for every preset (applied as constants by the
// generator), so they are not columns here.
//
// attack: 0 = 0.3 ms, 1 = 1.3 ms, 2 = 4.0 ms.
// quality: 0 = 1x, 1 = 2x, 2 = 4x.  Loop Destroy runs 1x on purpose.
// ---------------------------------------------------------------------------
namespace notsure
{

struct Preset
{
    const char* category;   // "" = top level; otherwise a folder in the menu
    const char* name;
    float crush, crunch, sag, darkness, mix, trim;
    int   attack;   // choice index 0..2
    int   quality;  // choice index 0..2
};

inline constexpr std::array<Preset, 9> presets { {
    //  category  name             crush crunch  sag  dark   mix  trim  atk qual
    { "",      "Default",          0.0f,  0.0f, 0.0f, 0.0f, 100.f, 0.f,  1,  2 },
    { "Drums", "Room Crush",       3.5f,  2.0f, 8.0f, 3.0f,  45.f, 0.f,  1,  2 },
    { "Drums", "Snare Fatten",     5.0f,  4.0f, 4.0f, 2.0f,  70.f, 0.f,  1,  2 },
    { "Drums", "Bus Glue",         2.0f,  1.5f, 3.0f, 1.5f,  35.f, 0.f,  2,  2 },
    { "Drums", "Loop Destroy",     8.0f,  7.0f, 9.0f, 5.0f, 100.f, 0.f,  0,  0 },
    { "Bass",  "Weight",           4.5f,  3.0f, 2.0f, 4.5f,  60.f, 0.f,  2,  2 },
    { "Vocal", "Grit",             6.0f,  5.5f, 3.5f, 3.0f,  28.f, 0.f,  1,  2 },
    { "Vocal", "sila",            10.0f, 10.0f, 0.0f, 0.0f, 100.f, 0.f,  0,  2 },
    { "Keys",  "Synth Thicken",    7.0f,  2.5f, 5.0f, 5.5f,  55.f, 0.f,  1,  2 },
} };

} // namespace notsure
