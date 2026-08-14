// Exercises signLicence/verifyLicence for real (LICENCE-SPEC.md acceptance
// 1-5): no file, empty file, random bytes, a validly-signed licence, a
// corrupted signature, and a licence signed with a different key. Uses
// throwaway key pairs generated on the spot - never the real embedded
// kPublicKey and never the real private key or the real licence.txt path.
// Prints PASS/FAIL per case and exits non-zero if anything fails.

#include "../Source/Licence.h"

#include <iostream>
#include <random>
#include <vector>

namespace
{
    int failures = 0;

    void check (const juce::String& label, bool ok)
    {
        std::cout << (ok ? "PASS  " : "FAIL  ") << label << "\n";
        if (! ok)
            ++failures;
    }

    juce::String randomBytesAsString (int count, unsigned seed)
    {
        std::mt19937 rng (seed);
        std::vector<char> bytes ((size_t) count);
        for (auto& b : bytes)
            b = (char) (rng() & 0xff);
        return juce::String::createStringFromData (bytes.data(), (int) bytes.size());
    }
}

int main()
{
    juce::RSAKey publicA, privateA;
    juce::RSAKey::createKeyPair (publicA, privateA, 512);

    juce::RSAKey publicB, privateB;
    juce::RSAKey::createKeyPair (publicB, privateB, 512);

    // 1: no licence text at all (what LicenceChecker sees with no file).
    check ("no file: unregistered",
           notsure::verifyLicence ({}, publicA) == "unregistered");

    // 2: an empty file.
    check ("empty file: unregistered",
           notsure::verifyLicence ("", publicA) == "unregistered");

    // 3: random bytes.
    for (int seed = 0; seed < 5; ++seed)
        check ("random bytes (seed " + juce::String (seed) + "): unregistered",
               notsure::verifyLicence (randomBytesAsString (256, (unsigned) seed), publicA) == "unregistered");

    // 4: a validly-signed licence.
    {
        const auto text = notsure::signLicence ("Ivan Petrov", "ivan@example.com", privateA);
        check ("valid licence: shows the buyer's name",
               notsure::verifyLicence (text, publicA) == "licensed to Ivan Petrov");
    }

    // 5: a corrupted signature - flip one hex character in an otherwise-real file.
    {
        auto text = notsure::signLicence ("Ivan Petrov", "ivan@example.com", privateA);
        auto lastHexDigit = text.getLastCharacter();
        const juce::String flippedDigit = (lastHexDigit == '0') ? "1" : "0";
        text = text.dropLastCharacters (1) + flippedDigit;
        check ("corrupted signature: unregistered, not an error",
               notsure::verifyLicence (text, publicA) == "unregistered");
    }

    // 6: signed with a different key pair than the one checking it.
    {
        const auto text = notsure::signLicence ("Ivan Petrov", "ivan@example.com", privateB);
        check ("wrong key: unregistered",
               notsure::verifyLicence (text, publicA) == "unregistered");
    }

    // 7: a licence text with only two lines (truncated mid-field).
    {
        const auto text = notsure::signLicence ("Ivan Petrov", "ivan@example.com", privateA);
        const auto truncated = text.upToLastOccurrenceOf ("\n", false, false);
        check ("truncated (missing signature line): unregistered",
               notsure::verifyLicence (truncated, publicA) == "unregistered");
    }

    // 8: an invalid (default-constructed) public key never validates anything,
    // matching Source/Licence.h's placeholder kPublicKey before a real key is
    // embedded - it must fail closed, not open.
    {
        const auto text = notsure::signLicence ("Ivan Petrov", "ivan@example.com", privateA);
        check ("invalid public key: fails closed (unregistered)",
               notsure::verifyLicence (text, juce::RSAKey()) == "unregistered");
    }

    std::cout << "\n" << failures << " failure(s)\n";
    return failures == 0 ? 0 : 1;
}
