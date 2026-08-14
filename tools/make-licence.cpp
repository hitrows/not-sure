// make-licence - issues signed licence.txt files, and (once) generates the
// signing key pair itself. See LICENCE-SPEC.md.
//
// Two modes:
//
//   make-licence --genkey
//       Generates a fresh key pair and prints both halves, clearly labelled.
//       Does not write either half to disk itself - the public half gets
//       pasted into Source/Licence.h, the private half gets redirected by
//       hand to wherever it is meant to live (outside the repo; never a path
//       this tool picks for you). Run this exactly once; running it again
//       invalidates every licence issued under the old key.
//
//   make-licence "Full Name" email@example.com --key <path-to-private-key>
//       Signs a licence for that name/email with the private key at the
//       given path (or NOTSURE_PRIVATE_KEY in the environment if --key is
//       omitted) and prints the three-line licence.txt content to stdout.
//       Issuing by hand: make-licence "Ivan Petrov" ivan@example.com --key
//       ~/Private/notsure-signing.key > licence.txt

#include "../Source/Licence.h"

#include <iostream>

namespace
{
    void printUsage()
    {
        std::cerr <<
            "usage:\n"
            "  make-licence --genkey\n"
            "  make-licence <name> <email> [--key <path>]\n"
            "                             (or set NOTSURE_PRIVATE_KEY instead of --key)\n";
    }
}

int main (int argc, char* argv[])
{
    juce::StringArray args;
    for (int i = 1; i < argc; ++i)
        args.add (argv[i]);

    if (args.contains ("--genkey"))
    {
        juce::RSAKey publicKey, privateKey;

        // 512 bits: this project's own words on the security bar here are
        // "deliberately low" - this matches JUCE's own historical licensing
        // examples and keeps applyToValue() (pure BigInteger arithmetic, no
        // hardware acceleration) fast on every plugin construction.
        juce::RSAKey::createKeyPair (publicKey, privateKey, 512);

        std::cout << "Public key  (paste into Source/Licence.h as kPublicKey):\n"
                  << publicKey.toString() << "\n\n"
                  << "Private key (redirect this yourself to wherever it will live outside\n"
                  << "the repo - this tool never writes it anywhere):\n"
                  << privateKey.toString() << "\n";
        return 0;
    }

    if (args.size() < 2)
    {
        printUsage();
        return 1;
    }

    const juce::String name  = args[0];
    const juce::String email = args[1];

    juce::String keyPath;
    const int keyFlag = args.indexOf ("--key");
    if (keyFlag >= 0 && keyFlag + 1 < args.size())
        keyPath = args[keyFlag + 1];
    else
        keyPath = juce::SystemStats::getEnvironmentVariable ("NOTSURE_PRIVATE_KEY", {});

    if (keyPath.isEmpty())
    {
        std::cerr << "error: no private key - pass --key <path> or set NOTSURE_PRIVATE_KEY\n";
        printUsage();
        return 1;
    }

    const juce::File keyFile (keyPath);
    if (! keyFile.existsAsFile())
    {
        std::cerr << "error: private key file not found: " << keyPath << "\n";
        return 1;
    }

    const juce::RSAKey privateKey (keyFile.loadFileAsString().trim());
    if (! privateKey.isValid())
    {
        std::cerr << "error: could not parse a private key from " << keyPath << "\n";
        return 1;
    }

    std::cout << notsure::signLicence (name, email, privateKey) << "\n";
    return 0;
}
