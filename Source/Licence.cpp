#include "Licence.h"

namespace notsure
{

juce::String signLicence (const juce::String& name, const juce::String& email,
                          const juce::RSAKey& privateKey)
{
    juce::MemoryOutputStream mo;
    mo << name << email;

    juce::BigInteger value;
    value.loadFromMemoryBlock (mo.getMemoryBlock());
    privateKey.applyToValue (value);

    return name + "\n" + email + "\n" + value.toString (16);
}

juce::String verifyLicence (const juce::String& fileText, const juce::RSAKey& publicKey)
{
    static const juce::String unregistered ("unregistered");

    if (! publicKey.isValid())
        return unregistered;

    auto lines = juce::StringArray::fromLines (fileText);
    lines.trim();
    lines.removeEmptyStrings();

    // Three fields: name, email, signature. Anything else - empty file,
    // random bytes that happen to split into fewer or more lines, a
    // truncated file - fails here rather than being partially parsed.
    if (lines.size() < 3)
        return unregistered;

    const auto name  = lines[0];
    const auto email = lines[1];
    const auto sigHex = lines[2];

    juce::BigInteger value;
    value.parseString (sigHex, 16);
    if (value.isZero())
        return unregistered;

    if (! publicKey.applyToValue (value))
        return unregistered;

    auto decoded = value.toMemoryBlock();

    // RSAKey::applyToValue() "dumbly" applies the key regardless of whether it
    // matches the one that signed the value (see its own header comment) - a
    // corrupted signature or a different key both land here as garbage bytes
    // rather than as a thrown error, so validate before turning them into a
    // String and comparing.
    if (! juce::CharPointer_UTF8::isValidString (static_cast<const char*> (decoded.getData()),
                                                  (int) decoded.getSize()))
        return unregistered;

    if (decoded.toString() != name + email)
        return unregistered;

    return "licensed to " + name;
}

LicenceChecker& LicenceChecker::getInstance()
{
    static LicenceChecker instance;
    return instance;
}

LicenceChecker::LicenceChecker()
{
    const auto file = juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
                          .getChildFile ("Application Support/Hitrows/Not Sure/licence.txt");

    statusText = file.existsAsFile()
                     ? verifyLicence (file.loadFileAsString(), juce::RSAKey (kPublicKey))
                     : juce::String ("unregistered");
}

} // namespace notsure
