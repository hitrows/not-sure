#pragma once

#include <juce_core/juce_core.h>
#include <juce_cryptography/juce_cryptography.h>

// See LICENCE-SPEC.md. GOG-style: a purchase gets a small signed text file,
// not copy protection. The plugin is fully functional with or without one -
// the only difference is what the version-screw tooltip reads.
namespace notsure
{

// Public half of the signing key pair, embedded in every build. The private
// half never ships and lives at ~/Private/notsure-signing.key on the signing
// machine, outside this repo - see tools/make-licence.cpp and
// LICENCE-SPEC.md. Generated once; regenerating it invalidates every licence
// already issued. If this were ever a default-constructed/malformed key
// instead, verifyLicence() below fails safe to "unregistered" rather than
// accepting anything - see notsure-licence-test's "invalid public key" case.
inline constexpr const char* kPublicKey =
    "5,66758233fc537567d03e7b6e1b39038607bdd0b1dc32d90da2b1ca8bc0c9223256da8f16504169f57387cd71bafb73dc5b90b6fd191875f419b3984abdef3365";

// Signs the concatenation of name and email with privateKey and returns the
// exact three-line text a licence.txt should contain. Pure - touches no disk,
// no global state - so both tools/make-licence (the real key) and the test
// tool (throwaway keys) can call it directly.
juce::String signLicence (const juce::String& name, const juce::String& email,
                          const juce::RSAKey& privateKey);

// Verifies fileText against publicKey. Never throws and never partially
// trusts a bad file: anything short of a fully valid, matching signature
// yields "unregistered" - empty input, wrong line count, unparseable hex, a
// signature that decodes to something other than name+email (wrong key,
// corruption, or a hand-edited file). A valid file yields
// "licensed to <name>". Pure, like signLicence.
juce::String verifyLicence (const juce::String& fileText, const juce::RSAKey& publicKey);

// App-lifetime singleton. Reads the fixed licence file once - on first
// access, from the processor constructor - and caches the result; never
// re-reads, never touches the audio thread. Wraps verifyLicence() with the
// real path and the embedded kPublicKey.
class LicenceChecker
{
public:
    static LicenceChecker& getInstance();

    const juce::String& getStatusText() const noexcept { return statusText; }

private:
    LicenceChecker();

    juce::String statusText;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LicenceChecker)
};

} // namespace notsure
