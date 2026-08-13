#pragma once

#include <juce_data_structures/juce_data_structures.h>

namespace notsure
{

// UpdateChecker
//
// Reports whether a newer version exists. It never downloads, installs, or
// updates anything - that split is deliberate (see UPDATE-CHECK-SPEC.md).
//
// One app-lifetime singleton shared across every plugin instance, so a session
// with twenty instances makes at most one request. The result is cached in a
// PropertiesFile with a timestamp; a check under 24h old is reused and no
// request is made. Editors observe it as ChangeListeners and query it - nothing
// here holds an editor pointer, so an editor can close while a request is in
// flight without any dangling reference.
//
// On any failure (offline, DNS, 404, malformed JSON, non-numeric version) it
// does nothing at all: no dialog, no log, no retry loop.
class UpdateChecker : public juce::ChangeBroadcaster
{
public:
    static UpdateChecker& getInstance();

    // Idempotent: the first call (on the first editor open) runs the check;
    // later calls are no-ops. localVersion is this build's "major.minor.patch".
    // Never call this from the processor constructor - a host scanning plugins
    // must not cause network traffic.
    void start (const juce::String& localVersion);

    bool isUpdateAvailable() const noexcept { return updateAvailable; }

    // Where the notice's click should go - the manifest's "url" if it gave one,
    // otherwise the Releases page.
    juce::String getDownloadUrl() const { return downloadUrl; }

private:
    UpdateChecker();
    ~UpdateChecker() override;

    struct Fetcher;
    friend struct Fetcher;

    // Called on the message thread once the background fetch has parsed a valid
    // manifest. Writes the cache, updates state, notifies editors.
    void onManifest (juce::String latestVersion, juce::String latestUrl);

    static bool parseVersion (const juce::String&, int& major, int& minor, int& patch);
    static bool isNewer (const juce::String& candidate, const juce::String& current);
    static juce::PropertiesFile::Options settingsOptions();

    juce::PropertiesFile settings;
    std::unique_ptr<juce::Thread> fetchThread;

    bool         started        = false;   // message thread only
    juce::String localVersion;
    juce::String downloadUrl { "https://github.com/hitrows/not-sure/releases/latest" };
    std::atomic<bool> updateAvailable { false };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (UpdateChecker)
};

} // namespace notsure
