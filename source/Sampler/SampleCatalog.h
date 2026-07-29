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

// One entry in the sample browser. Category/Mode/Key/BPM only for now — Subtag/mood
// tagging is deliberately deferred (see project-flow-catalog-ux-design memory).
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

// Reads test_samples/catalog_manifest.json (gitignored — real Beastsamples content, never
// pushed to the public repo), produced by tools/catalog-build.js from a manual pack-by-pack
// review of the real catalog. Returns an empty catalog on a fresh clone/CI where
// test_samples/ doesn't exist — same graceful-degradation behavior as before.
inline juce::Array<SampleEntry> buildTestCatalog()
{
    auto testSamplesDir = juce::File (__FILE__)
                               .getParentDirectory()
                               .getParentDirectory()
                               .getSiblingFile ("test_samples");

    auto catalogDir = testSamplesDir.getChildFile ("catalog");
    auto manifestFile = testSamplesDir.getChildFile ("catalog_manifest.json");

    juce::Array<SampleEntry> entries;

    if (! manifestFile.existsAsFile())
        return entries;

    auto json = juce::JSON::parse (manifestFile);
    auto* items = json.getArray();
    if (items == nullptr)
        return entries;

    for (auto& item : *items)
    {
        SampleEntry entry;
        entry.file = catalogDir.getChildFile (item["file"].toString());
        entry.name = item["name"].toString();
        entry.category = item["category"].toString();
        entry.mode = item["mode"].toString() == "OneShot" ? SampleMode::OneShot : SampleMode::Loop;
        entry.key = item["key"].toString();
        entry.bpm = (int) item["bpm"];
        entry.locked = (bool) item["locked"];
        entries.add (entry);
    }

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
