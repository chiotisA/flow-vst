#pragma once

#include <juce_core/juce_core.h>

// Loop vs One-Shot is a behavioral distinction, not a file-organization one — packs are
// sold/installed as intact units later, so mode lives as metadata per sample rather than
// as a folder split. Not derived from BPM presence (older packs carry loops with no BPM
// tag) — must be set deliberately per sample during a catalog audit.
enum class SampleMode
{
    Loop,
    OneShot
};

// One entry in the sample browser. Hand-written for now, pointing at a handful of real
// PHANTASIA files copied into test_samples/catalog/ (gitignored — real Beastsamples
// content, not for public repo history). The real per-pack metadata/parsing system that
// replaces this hardcoded list is task 7 in the Flow build order; this exists purely to
// prove the browser UI mechanics (search/filter/lock/favorite) against real content.
struct SampleEntry
{
    juce::File file;
    juce::String name;
    juce::String category;
    SampleMode mode = SampleMode::Loop;
    juce::String key;
    int bpm = 0;
    bool locked = false;
};

inline juce::Array<SampleEntry> buildTestCatalog()
{
    auto catalogDir = juce::File (__FILE__)
                          .getParentDirectory()
                          .getParentDirectory()
                          .getSiblingFile ("test_samples")
                          .getChildFile ("catalog");

    // All 14 test entries are confirmed Loop content (audited 2026-07-29) — no One-Shot
    // test samples exist yet. Pull some in from the D: catalog before building the
    // One-Shot tab or pitch-detection root mapping against real content.
    juce::Array<SampleEntry> entries;
    entries.add ({ catalogDir.getChildFile ("Bass_D_96bpm.wav"),    "Phantasia Bass #11",   "Bass",   SampleMode::Loop, "D", 96,  false });
    entries.add ({ catalogDir.getChildFile ("Bass_D_166bpm.wav"),   "Phantasia Bass #13",   "Bass",   SampleMode::Loop, "D", 166, true  });
    entries.add ({ catalogDir.getChildFile ("Guitar_B_145bpm.wav"), "Phantasia Guitar #11", "Guitar", SampleMode::Loop, "B", 145, false });
    entries.add ({ catalogDir.getChildFile ("Guitar_D_145bpm.wav"), "Phantasia Guitar #17", "Guitar", SampleMode::Loop, "D", 145, true  });
    entries.add ({ catalogDir.getChildFile ("Synth_B_119bpm.wav"),  "Phantasia Synth #12",  "Synth",  SampleMode::Loop, "B", 119, false });
    entries.add ({ catalogDir.getChildFile ("Synth_D_128bpm.wav"),  "Phantasia Synth #16",  "Synth",  SampleMode::Loop, "D", 128, true  });

    entries.add ({ catalogDir.getChildFile ("Riff_E_166bpm.wav"),        "Guitar Riff #1",        "Riff",      SampleMode::Loop, "E",     166, false });
    entries.add ({ catalogDir.getChildFile ("Riff_D_135bpm.wav"),        "Guitar Riff #12",       "Riff",      SampleMode::Loop, "D",     135, true  });
    entries.add ({ catalogDir.getChildFile ("Drums_125bpm.wav"),         "60's Drum Fill #1",     "Drums",     SampleMode::Loop, "-",     125, false });
    entries.add ({ catalogDir.getChildFile ("Drums_96bpm.wav"),          "60's Drum Fill #1 (96)", "Drums",    SampleMode::Loop, "-",     96,  true  });
    entries.add ({ catalogDir.getChildFile ("PsyGuitar_C_166bpm.wav"),   "Phantasia Psy Guitar #10", "PsyGuitar", SampleMode::Loop, "C",   166, false });
    entries.add ({ catalogDir.getChildFile ("PsyGuitar_D_96bpm.wav"),    "Phantasia Psy Guitar #14", "PsyGuitar", SampleMode::Loop, "D",   96,  true  });
    entries.add ({ catalogDir.getChildFile ("Brass_Dmin_130bpm.wav"),    "Phantasia Brass #10",   "Brass",     SampleMode::Loop, "Dmin",  130, false });
    entries.add ({ catalogDir.getChildFile ("Brass_Gsmin_160bpm.wav"),   "Phantasia Brass #12",   "Brass",     SampleMode::Loop, "G#min", 160, true  });

    // One-Shots (added 2026-07-29) — no BPM, and no key tag either since these packs were
    // never key-labelled at the source. Key stays "-" (unknown) until pitch-detection can
    // derive a real root from the actual audio; that's the concrete next feature to build.
    entries.add ({ catalogDir.getChildFile ("OneShot_SynthBass_01.wav"), "Synth Bass #1",  "Bass",  SampleMode::OneShot, "-", 0, false });
    entries.add ({ catalogDir.getChildFile ("OneShot_SynthBass_05.wav"), "Synth Bass #5",  "Bass",  SampleMode::OneShot, "-", 0, false });
    entries.add ({ catalogDir.getChildFile ("OneShot_SynthBass_12.wav"), "Synth Bass #12", "Bass",  SampleMode::OneShot, "-", 0, true  });
    entries.add ({ catalogDir.getChildFile ("OneShot_SynthBass_20.wav"), "Synth Bass #20", "Bass",  SampleMode::OneShot, "-", 0, true  });
    entries.add ({ catalogDir.getChildFile ("OneShot_Kick_01.wav"),      "Dragon Kick #1",  "Drums", SampleMode::OneShot, "-", 0, false });
    entries.add ({ catalogDir.getChildFile ("OneShot_Snare_01.wav"),     "Dragon Snare #1", "Drums", SampleMode::OneShot, "-", 0, false });

    return entries;
}

// Places the root note in a comfortable middle octave (C4 = MIDI 60); loops don't have a
// strict "root key" the way one-shot instrument samples do, but the engine needs a pitch
// reference to shift from, and this makes MIDI note 60 play the sample at its stated key.
inline int keyToRootMidiNote (const juce::String& key)
{
    static const juce::StringArray noteNames { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };

    // Strip a trailing "min"/"maj" (e.g. brass loops tagged "Dmin", "G#min") — we only
    // need the pitch class to place a root note, not the major/minor quality.
    auto noteOnly = key.upToFirstOccurrenceOf ("min", false, true)
                        .upToFirstOccurrenceOf ("maj", false, true)
                        .trim();

    const auto index = noteNames.indexOf (noteOnly);
    return 60 + juce::jmax (0, index);
}
