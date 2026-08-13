// make-presets — writes the preset table as .aupreset files in category folders.
//
// Logic groups presets by folder in its Settings menu when they live under
//   ~/Library/Audio/Presets/<Manufacturer>/<Plugin>/<Category>/
//
// The plugin exposes no AU factory programs, so we do not go through them. We
// take one ClassInfo dictionary from the AU as a template (it carries the right
// type/subtype/manufacturer/version) and swap in a state blob we build here.
//
// The state blob format is AudioProcessor::copyXmlToBinary, read straight from
// JUCE 8.0.15 source (juce_AudioProcessor.cpp), not from memory:
//   [uint32 LE magic 0x21324356][uint32 LE xmlByteLength][XML utf-8][0x00]
// The XML is the APVTS state - root <PARAMETERS>, one <PARAM id value/> each,
// values are the actual (denormalised) parameter values. Writing them directly
// sidesteps the AU parameter layer (which is normalised 0..1 and skewed).
//
// One source of truth: Source/Presets.h.
//
// Build + run:
//   clang++ -std=c++17 -fobjc-arc -ObjC++ tools/make-presets.mm \
//     -framework AudioToolbox -framework AudioUnit \
//     -framework Foundation -framework CoreFoundation -o /tmp/make-presets
//   /tmp/make-presets

#import <AudioToolbox/AudioToolbox.h>
#import <AudioUnit/AudioUnit.h>
#import <Foundation/Foundation.h>

#include "../Source/Presets.h"

namespace
{
    NSString* num (double v) { return [NSString stringWithFormat: @"%g", v]; }

    NSString* stateXml (const notsure::Preset& p)
    {
        // autogain on, bypass off for every preset.
        return [NSString stringWithFormat:
            @"<PARAMETERS>"
             "<PARAM id=\"crush\" value=\"%@\"/>"
             "<PARAM id=\"crunch\" value=\"%@\"/>"
             "<PARAM id=\"sag\" value=\"%@\"/>"
             "<PARAM id=\"darkness\" value=\"%@\"/>"
             "<PARAM id=\"mix\" value=\"%@\"/>"
             "<PARAM id=\"trim\" value=\"%@\"/>"
             "<PARAM id=\"autogain\" value=\"1\"/>"
             "<PARAM id=\"attack\" value=\"%d\"/>"
             "<PARAM id=\"oversampling\" value=\"%d\"/>"
             "<PARAM id=\"bypass\" value=\"0\"/>"
             "</PARAMETERS>",
            num (p.crush), num (p.crunch), num (p.sag), num (p.darkness),
            num (p.mix), num (p.trim), p.attack, p.quality];
    }

    NSData* stateBlob (const notsure::Preset& p)
    {
        NSData* xml = [stateXml (p) dataUsingEncoding: NSUTF8StringEncoding];

        const uint32_t magic = 0x21324356;               // little-endian host
        const uint32_t len   = (uint32_t) xml.length;
        const uint8_t  zero  = 0;

        NSMutableData* blob = [NSMutableData data];
        [blob appendBytes: &magic length: 4];
        [blob appendBytes: &len   length: 4];
        [blob appendData: xml];
        [blob appendBytes: &zero  length: 1];
        return blob;
    }
}

int main (int argc, char** argv)
{
    @autoreleasepool
    {
        AudioComponentDescription desc {};
        desc.componentType    = kAudioUnitType_Effect;
        desc.componentSubType = 'Nsur';
        desc.componentManufacturer = 'Htrw';

        AudioComponent comp = AudioComponentFindNext (nullptr, &desc);
        if (comp == nullptr) { fprintf (stderr, "error: Not Sure AU not found\n"); return 1; }

        AudioComponentInstance au = nullptr;
        if (AudioComponentInstanceNew (comp, &au) != noErr || au == nullptr)
        { fprintf (stderr, "error: could not instantiate the AU\n"); return 1; }
        AudioUnitInitialize (au);

        // One ClassInfo as a template - it carries type/subtype/manufacturer etc.
        CFPropertyListRef templateInfo = nullptr;
        UInt32 size = sizeof (templateInfo);
        if (AudioUnitGetProperty (au, kAudioUnitProperty_ClassInfo,
                                  kAudioUnitScope_Global, 0, &templateInfo, &size) != noErr
            || templateInfo == nullptr)
        { fprintf (stderr, "error: could not read template ClassInfo\n"); return 1; }
        NSDictionary* templateDict = (__bridge_transfer NSDictionary*) templateInfo;

        // Output base: argv[1] if given (used by the installer to stage into a
        // temp dir), otherwise the user's own preset folder.
        NSString* base = (argc > 1)
            ? [NSString stringWithUTF8String: argv[1]]
            : [NSHomeDirectory() stringByAppendingPathComponent:
                 @"Library/Audio/Presets/Hitrows/Not Sure"];
        NSFileManager* fm = [NSFileManager defaultManager];

        int written = 0;
        for (const auto& preset : notsure::presets)
        {
            NSMutableDictionary* dict = [templateDict mutableCopy];
            dict[@"jucePluginState"] = stateBlob (preset);
            dict[@"name"] = [NSString stringWithUTF8String: preset.name];

            NSString* dir = strlen (preset.category) > 0
                ? [base stringByAppendingPathComponent: [NSString stringWithUTF8String: preset.category]]
                : base;
            [fm createDirectoryAtPath: dir withIntermediateDirectories: YES attributes: nil error: nil];

            NSString* path = [[dir stringByAppendingPathComponent:
                                [NSString stringWithUTF8String: preset.name]]
                               stringByAppendingPathExtension: @"aupreset"];

            NSData* data = [NSPropertyListSerialization dataWithPropertyList: dict
                              format: NSPropertyListXMLFormat_v1_0 options: 0 error: nil];
            if (data != nil && [data writeToFile: path atomically: YES])
            { printf ("wrote %s\n", path.UTF8String); ++written; }
            else
                fprintf (stderr, "error: could not write %s\n", path.UTF8String);
        }

        AudioUnitUninitialize (au);
        AudioComponentInstanceDispose (au);

        printf ("done: %d/%d presets\n", written, (int) notsure::presets.size());
        return written == (int) notsure::presets.size() ? 0 : 2;
    }
}
