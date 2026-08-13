#include "UpdateChecker.h"

namespace notsure
{

// The one place the manifest URL lives. A plain GET of this static file is the
// entire network footprint - no query parameters, no identifying information.
static const char* const kManifestUrl =
    "https://raw.githubusercontent.com/hitrows/not-sure/main/version.json";

static constexpr juce::int64 kOneDayMs = 24LL * 60LL * 60LL * 1000LL;

// ---------------------------------------------------------------------------
// Background fetch. Reads the manifest with a short timeout, parses it, and
// hands a valid result to the singleton on the message thread. On any failure
// it simply returns.
// ---------------------------------------------------------------------------
struct UpdateChecker::Fetcher : public juce::Thread
{
    explicit Fetcher (UpdateChecker& o) : juce::Thread ("NotSure update check"), owner (o) {}
    ~Fetcher() override { stopThread (6000); }

    void run() override
    {
        juce::String text;

        if (auto stream = juce::URL (kManifestUrl).createInputStream (
                juce::URL::InputStreamOptions (juce::URL::ParameterHandling::inAddress)
                    .withConnectionTimeoutMs (5000)))
        {
            text = stream->readEntireStreamAsString();
        }

        if (threadShouldExit() || text.isEmpty())
            return;

        const auto json = juce::JSON::parse (text);
        const auto version = json.getProperty ("version", juce::var()).toString();
        const auto url     = json.getProperty ("url",     juce::var()).toString();

        int a, b, c;
        if (! parseVersion (version, a, b, c))
            return;   // malformed / non-numeric -> treat as no update

        if (threadShouldExit())
            return;

        // The singleton outlives every fetch, so capturing it is safe.
        auto& o = owner;
        juce::MessageManager::callAsync ([&o, version, url] { o.onManifest (version, url); });
    }

    UpdateChecker& owner;
};

// ---------------------------------------------------------------------------

UpdateChecker& UpdateChecker::getInstance()
{
    static UpdateChecker instance;
    return instance;
}

UpdateChecker::UpdateChecker()
    : settings (settingsOptions())
{
}

UpdateChecker::~UpdateChecker()
{
    fetchThread.reset();   // joins the fetch thread if still running
}

juce::PropertiesFile::Options UpdateChecker::settingsOptions()
{
    juce::PropertiesFile::Options o;
    o.applicationName     = "settings";
    o.folderName          = "Hitrows/Not Sure";
    o.filenameSuffix      = "xml";
    o.osxLibrarySubFolder = "Application Support";
    return o;   // ~/Library/Application Support/Hitrows/Not Sure/settings.xml
}

void UpdateChecker::start (const juce::String& lv)
{
    // Message thread only (called from an editor constructor).
    if (started)
        return;

    started      = true;
    localVersion = lv;

    // Reuse a check under 24h old rather than hitting the network again. The
    // cached version is what the notice is drawn from, so it keeps showing
    // offline and across restarts once a check has found an update.
    const auto lastCheck = settings.getValue ("lastCheckTime").getLargeIntValue();
    const auto lastSeen  = settings.getValue ("lastSeenVersion");
    const auto now       = juce::Time::getCurrentTime().toMilliseconds();

    if (lastSeen.isNotEmpty() && (now - lastCheck) < kOneDayMs)
    {
        updateAvailable = isNewer (lastSeen, localVersion);
        sendChangeMessage();
        return;
    }

    // The request itself is switchable off (privacy). No UI; documented in the
    // README. Default is on.
    if (! settings.getBoolValue ("checkForUpdates", true))
        return;

    fetchThread = std::make_unique<Fetcher> (*this);
    fetchThread->startThread();
}

void UpdateChecker::onManifest (juce::String latestVersion, juce::String latestUrl)
{
    // Message thread.
    settings.setValue ("lastCheckTime", juce::String (juce::Time::getCurrentTime().toMilliseconds()));
    settings.setValue ("lastSeenVersion", latestVersion);
    settings.saveIfNeeded();

    if (latestUrl.isNotEmpty())
        downloadUrl = latestUrl;

    updateAvailable = isNewer (latestVersion, localVersion);
    sendChangeMessage();
}

bool UpdateChecker::parseVersion (const juce::String& v, int& major, int& minor, int& patch)
{
    auto parts = juce::StringArray::fromTokens (v.trim(), ".", "");
    if (parts.size() != 3)
        return false;

    for (auto& p : parts)
        if (p.isEmpty() || ! p.containsOnly ("0123456789"))
            return false;

    major = parts[0].getIntValue();
    minor = parts[1].getIntValue();
    patch = parts[2].getIntValue();
    return true;
}

bool UpdateChecker::isNewer (const juce::String& candidate, const juce::String& current)
{
    int ca, cb, cc, la, lb, lc;
    if (! parseVersion (candidate, ca, cb, cc) || ! parseVersion (current, la, lb, lc))
        return false;

    // Numeric compare, so 0.10.0 is correctly newer than 0.9.0.
    if (ca != la) return ca > la;
    if (cb != lb) return cb > lb;
    return cc > lc;
}

} // namespace notsure
