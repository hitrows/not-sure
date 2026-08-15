#pragma once

#include <juce_data_structures/juce_data_structures.h>

// Single source of truth for where this plugin's per-user files live -
// settings.xml (UpdateChecker) and licence.txt (Licence) both sit in the same
// folder. Never build this path by string concatenation: "~/Library" only
// exists on macOS, and osxLibrarySubFolder is a macOS-only concept that
// juce::PropertiesFile::Options already resolves correctly per platform
// (Application Support on macOS, %APPDATA% on Windows) - see
// WINDOWS-SPEC.md.
namespace notsure
{

inline juce::PropertiesFile::Options baseAppDataOptions()
{
    juce::PropertiesFile::Options o;
    o.folderName          = "Hitrows/Not Sure";
    o.osxLibrarySubFolder = "Application Support";
    return o;
}

// The folder itself, with no particular file in mind - getDefaultFile()
// always appends applicationName + filenameSuffix last, after every
// platform-specific branch, so its parent is the resolved folder on any
// platform.
inline juce::File getAppDataFolder()
{
    auto o = baseAppDataOptions();
    o.applicationName = "x";
    o.filenameSuffix  = "tmp";
    return o.getDefaultFile().getParentDirectory();
}

} // namespace notsure
