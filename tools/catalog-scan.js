// Scans the real Beastsamples drive and produces a CSV manifest for manual review in
// Google Sheets. Never touches/moves/copies any real audio — read-only, metadata only.
//
// Usage: node tools/catalog-scan.js "D:/[BEASTSAMPLES] COMPLETE COLLECTION" output.csv
//
// Output columns: PackFolder, RelativePath, Category, Subtag, Mode, Key, BPM, NeedsReview, Notes
//
// Classification is a hand-built lookup table (see PACK_CLASSIFICATION below), not a
// generic heuristic guess — each pack was individually reviewed in conversation before
// being placed in this table. Packs not in the table are treated as NeedsReview by default
// (safe fallback: never silently miscategorize something we haven't actually looked at).

const fs = require ('fs');
const path = require ('path');

// WAV only — packs ship AIFF/WAV pairs of the same content, Flow only needs one, and
// this sidesteps a whole class of false "duplicate" flags between format pairs.
const AUDIO_EXTENSIONS = new Set (['.wav']);

// Packs deliberately excluded from this pass entirely (not written to the CSV at all):
// - "Beastsamples Collection": confirmed to be plugin installers, not sample audio.
// - "Experience Bundle Pack-Complete": old content, no BPM info, deprioritized by user.
// - "Supreme Samples Bundle by Beastsamples": zip-only, unique content reserved as a
//   future paid expansion for testing the licensing/unlock system later, not part of the
//   free base catalog scan.
const SKIP_PACKS = new Set ([
    'Beastsamples Collection',
    'Experience Bundle Pack-Complete',
    'Supreme Samples Bundle by Beastsamples',
]);

// Single-category packs — Category applies to every file in the whole pack.
const SINGLE_CATEGORY_PACKS = {
    // Guitar
    'Classic Guitars Chapter II (BETA) by Beastsamples': 'Guitar',
    'Classical Guitar by Beastsamples': 'Guitar',
    'Clean Guitars - Beastsamples': 'Guitar',
    'Dark vibes guitar by Beastsamples': 'Guitar',
    'Guitar Riffs by Beastsamples': 'Guitar',
    'Guitar Tools by Beastsamples': 'Guitar',
    'Liquid Guitars by Beastsamples': 'Guitar',
    'Mood Guitar Samples  FREE by Beastsamples': 'Guitar',
    'Moonshine Guitar Pack by Beastsamples': 'Guitar',
    'Neat Guitars by Beastsamples.com': 'Guitar',
    'PSY R Guitars - FREE by beastsamples': 'Guitar',
    'Psychedelic Guitar Pack by Beastsamples': 'Guitar',
    'Radiant Guitars by Beastsamples': 'Guitar',
    'Raw Guitars - Supreme Edition by Beastsamples': 'Guitar',
    'Sick Guitars by Beastsamples': 'Guitar',
    'Space Guitar Loops': 'Guitar',
    'Tale Guitars FREE by Beastsamples': 'Guitar',

    // Synth
    'Analog Synths Collection FIDIA': 'Synth',
    'Analog Synths by Beastsamples': 'Synth',
    'Analogia - Synth Collection': 'Synth',
    'Free Arpegiators Kit by Beastsamples': 'Synth',
    'Fuse Synths by Beastsamples': 'Synth',
    'Lines Synths by Beastsamples': 'Synth',
    'Mastermind Synth Bundle': 'Synth',
    'Native Synths by Beastsamples': 'Synth',
    'Psy Synth Loops by Beastsamples': 'Synth',
    'Rabbit Hole Synth Pack': 'Synth',
    'Riddle Synths ny Beastsamples': 'Synth',
    'Trippy Synth Samples by Beastsamples': 'Synth',

    // Bass
    '432hz Bass Grooves by Beastsamples': 'Bass',
    'BASS ONE SHOTS SYNTHS by Beastsamples': 'Bass',
    'Bass Mellow Grooves by Beastsamples': 'Bass',
    'Drone Bass by Beastsamples': 'Bass',
    'Electric Rap Basslines - BETA by Beastsamples': 'Bass',
    'ROGUE BASSLINES by Beastsamples': 'Bass',

    // Drums
    "60's Drum Fills - Vol 2 by Beastsamples": 'Drums',
    "60's Drum Fills - Vol I by Beastsamples": 'Drums',
    "60's Drum Hits & Rolls by Beastsamples": 'Drums',
    'Analog Drums by Beatsamples': 'Drums',
    'Dragon One-Shot Kit': 'Drums',
    'Drum sample toolkit by Beastsamples': 'Drums',
    'Drums': 'Drums',
    'Ground Drumkit by Beastsamples': 'Drums',
    'Hi-Hats by Beastsamples': 'Drums',
    'MADPACK - DRUMS by Beastsamples': 'Drums',
    'Phonk Drumloops by Beastsamples': 'Drums',
    'Primal Drumkit by Beastsamples': 'Drums',

    // Keys (new 6th category — Guitar/Synth/Bass/Drums/SFX didn't have anywhere for this)
    'Hermetic Keys by Beastsamples': 'Keys',
    'Lo-fi Keys by Beastsamples': 'Keys',
    'Rap Keys by Beastsamples': 'Keys',
    'Vibey Keys by Beastsamples': 'Keys',
};

// Multi-category packs where Category is instead determined per-file from a known
// subfolder-name mapping (checked case-insensitively against every path segment).
const SUBFOLDER_CATEGORY_PACKS = {
    'PHANTASIA by Beastsamples': {
        'bass wavs': 'Bass',
        'guitar wavs': 'Guitar',
        'psy guitar wavs': 'Guitar',
        'synth wavs': 'Synth',
        // Brass doesn't fit any of our 6 categories (Guitar/Synth/Bass/Drums/SFX/Keys) —
        // real gap found while building this table, flagged in Notes rather than guessed.
        'brass melodies': 'Brass (UNMAPPED — needs a category decision)',
    },
    'Percs & Folleys by Beastsamples': {
        'bass': 'Bass',
        'guitar': 'Guitar',
        'synth': 'Synth',
        'hits-hats-percs': 'Drums',
        'kicks': 'Drums',
        'snares-percs': 'Drums',
    },
};

// Packs needing a human ear — genre-themed bundles that mix categories internally, or
// packs with no instrument named at all. Every file still gets listed (nothing silently
// dropped), just with blank Category/Subtag/Mode/Key/BPM and NeedsReview=YES.
const MANUAL_REVIEW_PACKS = new Set ([
    'Amapiano Essence Preview Bundle',
    'Dark Phonk by Beastsamples',
    'Dream Melodies by Beastsamples',
    'Free Samples 2022',
    'Phonk Melodies by Beastsamples',
    'Phonk Samples Vol_I by Beastsamples',
    'Phonk Samples Vol_II by Beastsamples',
    'Progressions  by Beastsamples',
    'Psychedelic Complete Collection - Folders',
    'Psychic Textures Vol I by Beastsamples',
    'Secret Content by Beastsamples',
    'Sonic Visions by Beastsamples',
    'The MasterMind Melodies',
    'Trap Moodscape by Beastsamples',
    'URBAN SAMPLES - PRERELEASE by Beastsamples',
    'Xmas Edition By Beastsamples',
]);

// Subtag/Mode inference from path+filename tokens (case-insensitive substring match,
// checked in this priority order). Not exhaustive — anything unmatched is left blank
// rather than guessed, so a human can fill it in instead of trusting a wrong default.
const SUBTAG_RULES = [
    [ 'kick',              'Kick',               'OneShot' ],
    [ 'snare',              'Snare',              'OneShot' ],
    [ 'hi-hat',             'Hi-Hat',             'OneShot' ],
    [ 'hihat',              'Hi-Hat',             'OneShot' ],
    [ 'hi hat',             'Hi-Hat',             'OneShot' ],
    [ 'open-hat',           'Hi-Hat',             'OneShot' ],
    [ 'openhat',            'Hi-Hat',             'OneShot' ],
    [ 'perc',               'Perc',               'OneShot' ],
    [ 'riser',              'Riser',              'OneShot' ],
    [ 'impact',             'Impact',             'OneShot' ],
    [ 'arp',                'Arp',                'Loop' ],
    [ 'pad',                'Pad',                'Loop' ],
    [ 'lead',               'Lead',               'Loop' ],
    [ 'pluck',              'Pluck',              'Loop' ],
    [ 'wobble',             'Wobble',             'Loop' ],
    [ 'riff',               'Riff',               'Loop' ],
    [ 'bassline',           'Groove/Bassline',    'Loop' ],
    [ 'groove',             'Groove/Bassline',    'Loop' ],
    [ 'fill',               'Fill',               'Loop' ],
    [ 'melody',             'Melody',             'Loop' ],
    [ 'melodies',           'Melody',             'Loop' ],
    [ 'chord',              'Chord/Progression',  'Loop' ],
    [ 'progression',        'Chord/Progression',  'Loop' ],
    [ 'texture',            'Texture/Drone',       'Loop' ],
    [ 'drone',              'Texture/Drone',       'Loop' ],
    [ 'ambience',           'Texture/Drone',       'Loop' ],
    [ 'ambient',            'Texture/Drone',       'Loop' ],
    [ 'one-shot',           null,                 'OneShot' ],
    [ 'oneshot',            null,                 'OneShot' ],
    [ 'one shot',           null,                 'OneShot' ],
    [ 'shot',               null,                 'OneShot' ],
];

function inferSubtagAndMode (relativePath)
{
    const haystack = relativePath.toLowerCase();
    for (const [ keyword, subtag, mode ] of SUBTAG_RULES)
    {
        if (haystack.includes (keyword))
            return { subtag: subtag || '', mode };
    }
    return { subtag: '', mode: '' };
}

// Extracts Key/BPM from the filename by splitting on common separators and checking each
// whole token — NOT a substring search, so "D" never matches inside "Drums" the way the
// live in-plugin search currently does. This is the same bug class found in
// CatalogBrowser::updateFilteredList, avoided here on purpose.
const NOTE_TOKEN = /^([A-G](?:#|b)?)(min|maj)?$/i;
const BPM_TOKEN = /^(\d{2,3})bpm$/i;

function extractKeyAndBpm (filename)
{
    const base = path.basename (filename, path.extname (filename));
    const tokens = base.split (/[\s_\-.]+/).filter (Boolean);

    let key = '';
    let bpm = '';

    for (let i = 0; i < tokens.length; ++i)
    {
        const t = tokens[i];

        const bpmMatch = t.match (BPM_TOKEN);
        if (bpmMatch)
        {
            bpm = bpmMatch[1];
            continue;
        }
        // Two-token BPM form: "125" "BPM" as separate tokens.
        if (/^\d{2,3}$/.test (t) && tokens[i + 1] && /^bpm$/i.test (tokens[i + 1]))
        {
            bpm = t;
            continue;
        }

        const noteMatch = t.match (NOTE_TOKEN);
        if (noteMatch && ! key)
            key = noteMatch[1] + (noteMatch[2] ? noteMatch[2].toLowerCase() : '');
    }

    return { key, bpm };
}

function csvEscape (value)
{
    const s = String (value ?? '');
    if (s.includes (',') || s.includes ('"') || s.includes ('\n'))
        return '"' + s.replace (/"/g, '""') + '"';
    return s;
}

function walk (dir, fileList)
{
    for (const entry of fs.readdirSync (dir, { withFileTypes: true }))
    {
        const fullPath = path.join (dir, entry.name);
        if (entry.isDirectory())
            walk (fullPath, fileList);
        else if (AUDIO_EXTENSIONS.has (path.extname (entry.name).toLowerCase()))
            fileList.push (fullPath);
    }
}

function main()
{
    const [ , , rootArg, outArg ] = process.argv;
    if (! rootArg || ! outArg)
    {
        console.error ('Usage: node tools/catalog-scan.js "<drive root>" <output.csv>');
        process.exit (1);
    }

    const root = rootArg;
    const rows = [];

    const topLevelEntries = fs.readdirSync (root, { withFileTypes: true })
        .filter (e => e.isDirectory());

    for (const packEntry of topLevelEntries)
    {
        const packName = packEntry.name;
        if (SKIP_PACKS.has (packName))
            continue;

        const packPath = path.join (root, packName);
        const files = [];
        walk (packPath, files);

        const isManual = MANUAL_REVIEW_PACKS.has (packName);
        const singleCategory = SINGLE_CATEGORY_PACKS[packName];
        const subfolderMap = SUBFOLDER_CATEGORY_PACKS[packName];

        if (! isManual && ! singleCategory && ! subfolderMap)
            console.error (`WARNING: "${packName}" is not in any classification table — treating as NeedsReview.`);

        // Pass 1: find genuine accidental duplicates only. A numbered series like
        // "Kick (1).wav" through "Kick (9).wav" with NO bare "Kick.wav" ever present is a
        // legitimate numbered set (a kit of 9 different kicks), not a duplicate — only
        // flag when a bare (unnumbered) version of the same name also exists alongside a
        // numbered one, which is the actual signature of a Windows auto-rename artifact
        // (e.g. the real "BASS - A - 144 bpm.wav" + "...bpm(2).wav" case).
        const bareNamesSeen = new Set();
        const exactNameCount = new Map();
        for (const filePath of files)
        {
            const originalBasename = path.basename (filePath);
            const strippedBasename = originalBasename.replace (/\s*\(\d+\)(?=\.[^.]+$)/, '');
            if (originalBasename === strippedBasename)
                bareNamesSeen.add (strippedBasename.toLowerCase());
            exactNameCount.set (originalBasename.toLowerCase(), (exactNameCount.get (originalBasename.toLowerCase()) || 0) + 1);
        }

        for (const filePath of files)
        {
            const relativePath = path.relative (packPath, filePath);
            let category = '';
            let notes = '';
            let needsReview = isManual || (! singleCategory && ! subfolderMap);

            if (singleCategory)
            {
                category = singleCategory;
            }
            else if (subfolderMap)
            {
                const segments = relativePath.toLowerCase().split (path.sep);
                const matchedKey = Object.keys (subfolderMap).find (k => segments.includes (k));
                if (matchedKey)
                {
                    category = subfolderMap[matchedKey];
                    if (category.includes ('UNMAPPED'))
                        needsReview = true;
                }
                else
                {
                    needsReview = true;
                    notes = 'No known subfolder matched — category undetermined';
                }
            }

            let subtag = '', mode = '', key = '', bpm = '';
            if (! needsReview || subfolderMap)
            {
                const inferred = inferSubtagAndMode (relativePath);
                subtag = inferred.subtag;
                mode = inferred.mode;
                const kb = extractKeyAndBpm (path.basename (filePath));
                key = kb.key;
                bpm = kb.bpm;
                if (! mode)
                    notes = (notes ? notes + '; ' : '') + 'Mode could not be inferred — needs manual Loop/OneShot tag';
            }

            // Duplicate-file detection, two independent signals:
            // (a) this file is a numbered "(N)" variant AND a bare unnumbered version of
            //     the same name also exists in this pack — the actual Windows auto-rename
            //     signature (NOT just being part of a numbered series, which is normal).
            // (b) the exact same filename (any name) appears more than once in this pack,
            //     e.g. present in two different subfolders — a real structural duplicate
            //     independent of any numbering.
            const originalBasename = path.basename (filePath);
            const strippedBasename = originalBasename.replace (/\s*\(\d+\)(?=\.[^.]+$)/, '');
            const isNumberedVariant = originalBasename !== strippedBasename;

            if (isNumberedVariant && bareNamesSeen.has (strippedBasename.toLowerCase()))
                notes = (notes ? notes + '; ' : '') + `Possible duplicate of bare file "${strippedBasename}"`;
            else if (exactNameCount.get (originalBasename.toLowerCase()) > 1)
                notes = (notes ? notes + '; ' : '') + `Same filename appears ${exactNameCount.get (originalBasename.toLowerCase())}x in this pack (different folders)`;

            rows.push ([
                packName,
                relativePath,
                category,
                subtag,
                mode,
                key,
                bpm,
                needsReview ? 'YES' : '',
                notes,
            ]);
        }
    }

    const header = [ 'PackFolder', 'RelativePath', 'Category', 'Subtag', 'Mode', 'Key', 'BPM', 'NeedsReview', 'Notes' ];
    const csvLines = [ header.join (',') ];
    for (const row of rows)
        csvLines.push (row.map (csvEscape).join (','));

    fs.writeFileSync (outArg, csvLines.join ('\n'), 'utf8');

    const reviewCount = rows.filter (r => r[7] === 'YES').length;
    console.log (`Scanned ${topLevelEntries.length - [...SKIP_PACKS].length} packs (${SKIP_PACKS.size} skipped).`);
    console.log (`Wrote ${rows.length} rows to ${outArg}`);
    console.log (`${reviewCount} rows flagged NeedsReview (${((reviewCount / rows.length) * 100).toFixed (1)}%).`);
}

main();
