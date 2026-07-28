#pragma once

#include <juce_core/juce_core.h>

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

    juce::Array<SampleEntry> entries;
    entries.add ({ catalogDir.getChildFile ("Bass_D_96bpm.wav"),    "Phantasia Bass #11",   "Bass",   "D", 96,  false });
    entries.add ({ catalogDir.getChildFile ("Bass_D_166bpm.wav"),   "Phantasia Bass #13",   "Bass",   "D", 166, true  });
    entries.add ({ catalogDir.getChildFile ("Guitar_B_145bpm.wav"), "Phantasia Guitar #11", "Guitar", "B", 145, false });
    entries.add ({ catalogDir.getChildFile ("Guitar_D_145bpm.wav"), "Phantasia Guitar #17", "Guitar", "D", 145, true  });
    entries.add ({ catalogDir.getChildFile ("Synth_B_119bpm.wav"),  "Phantasia Synth #12",  "Synth",  "B", 119, false });
    entries.add ({ catalogDir.getChildFile ("Synth_D_128bpm.wav"),  "Phantasia Synth #16",  "Synth",  "D", 128, true  });

    entries.add ({ catalogDir.getChildFile ("Riff_E_166bpm.wav"),        "Guitar Riff #1",        "Riff",      "E",     166, false });
    entries.add ({ catalogDir.getChildFile ("Riff_D_135bpm.wav"),        "Guitar Riff #12",       "Riff",      "D",     135, true  });
    entries.add ({ catalogDir.getChildFile ("Drums_125bpm.wav"),         "60's Drum Fill #1",     "Drums",     "-",     125, false });
    entries.add ({ catalogDir.getChildFile ("Drums_96bpm.wav"),          "60's Drum Fill #1 (96)", "Drums",    "-",     96,  true  });
    entries.add ({ catalogDir.getChildFile ("PsyGuitar_C_166bpm.wav"),   "Phantasia Psy Guitar #10", "PsyGuitar", "C",   166, false });
    entries.add ({ catalogDir.getChildFile ("PsyGuitar_D_96bpm.wav"),    "Phantasia Psy Guitar #14", "PsyGuitar", "D",   96,  true  });
    entries.add ({ catalogDir.getChildFile ("Brass_Dmin_130bpm.wav"),    "Phantasia Brass #10",   "Brass",     "Dmin",  130, false });
    entries.add ({ catalogDir.getChildFile ("Brass_Gsmin_160bpm.wav"),   "Phantasia Brass #12",   "Brass",     "G#min", 160, true  });

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
