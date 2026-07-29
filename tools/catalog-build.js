// Builds Flow's real test catalog from the manually-reviewed pack list (see
// catalog-manual-review.md). Copies approved real files into test_samples/catalog/
// (gitignored, mirrors each pack's real folder structure to avoid name collisions) and
// writes a JSON manifest the plugin reads at runtime. Never touches the source drive.
//
// Usage: node tools/catalog-build.js "D:/[BEASTSAMPLES] COMPLETE COLLECTION"
//
// Scope, per user decision 2026-07-29: Category + Mode + Key + BPM only. No Subtag/Tag/
// mood yet — that's still deliberately deferred (see project-flow-catalog-ux-design memory).

const fs = require ('fs');
const path = require ('path');

const REPO_ROOT = path.join (__dirname, '..');
const CATALOG_DIR = path.join (REPO_ROOT, 'test_samples', 'catalog');
const MANIFEST_PATH = path.join (REPO_ROOT, 'test_samples', 'catalog_manifest.json');

// Same rules as tools/catalog-scan.js's SUBTAG_RULES, used here only to infer Mode
// (Loop/OneShot) for packs where mode varies by subfolder — Subtag itself isn't written
// to the manifest yet.
const ONE_SHOT_KEYWORDS = [ 'kick', 'snare', 'hi-hat', 'hihat', 'hi hat', 'open-hat', 'openhat', 'perc', 'shot' ];

function inferModeFromPath (relativePathLower)
{
    return ONE_SHOT_KEYWORDS.some (kw => relativePathLower.includes (kw)) ? 'OneShot' : 'Loop';
}

const NOTE_TOKEN = /^([A-G](?:#|b)?)(min|maj)?$/i;
const BPM_TOKEN = /^(\d{2,3})bpm$/i;

function extractKeyAndBpm (filename)
{
    const base = path.basename (filename, path.extname (filename));
    const tokens = base.split (/[\s_\-.]+/).filter (Boolean);
    let key = '', bpm = '';

    for (let i = 0; i < tokens.length; ++i)
    {
        const t = tokens[i];
        const bpmMatch = t.match (BPM_TOKEN);
        if (bpmMatch) { bpm = bpmMatch[1]; continue; }
        if (/^\d{2,3}$/.test (t) && tokens[i + 1] && /^bpm$/i.test (tokens[i + 1])) { bpm = t; continue; }
        const noteMatch = t.match (NOTE_TOKEN);
        if (noteMatch && ! key)
            key = noteMatch[1] + (noteMatch[2] ? noteMatch[2].toLowerCase() : '');
    }
    return { key, bpm };
}

function walkWav (dir, fileList)
{
    for (const entry of fs.readdirSync (dir, { withFileTypes: true }))
    {
        const fullPath = path.join (dir, entry.name);
        if (entry.isDirectory())
            walkWav (fullPath, fileList);
        else if (path.extname (entry.name).toLowerCase() === '.wav')
            fileList.push (fullPath);
    }
}

// Approved packs from the manual review pass (catalog-manual-review.md). Each entry:
// - category: fixed Category for every file in the pack
// - mode: 'Loop' | 'OneShot' | 'inferFromPath' (subfolder names indicate mode per-file)
// - excludeIfPathContains: lowercase substrings — any file whose relative path contains
//   one of these is skipped entirely (Guitar Tools' Tools/Wah-Wah subfolders)
// - onlyIfPathContains: if set, ONLY files whose relative path contains this substring
//   are included (Liquid Guitars — just the Wet stems)
// - categoryOverrides: [{ contains, category }] — a pack's default category isn't always
//   right for every subfolder (e.g. a "Bass" subfolder bundled inside a drum kit is real
//   Bass one-shot content, not a Drums subtype) — checked before falling back to `category`
// - regions: [{ contains, category, mode, exclude }] — for packs that are genuinely
//   multi-category with no single fallback (PHANTASIA, Percs & Folleys). Checked in order,
//   first match wins; `exclude: true` drops the file entirely (e.g. PHANTASIA's Brass).
//   A pack with `regions` has no top-level category/mode of its own.
const APPROVED_PACKS = {
    'Analog Drums by Beatsamples':                    { category: 'Drums', mode: 'OneShot', categoryOverrides: [ { contains: 'bass', category: 'Bass' } ] },
    'Analog Synths by Beastsamples':                  { category: 'Synth', mode: 'Loop' },
    'Analog Synths Collection FIDIA':                 { category: 'Synth', mode: 'inferFromPath' },
    'Bass Mellow Grooves by Beastsamples':            { category: 'Bass',  mode: 'Loop' },
    'Classic Guitars Chapter II (BETA) by Beastsamples': { category: 'Guitar', mode: 'Loop' },
    'Classical Guitar by Beastsamples':               { category: 'Guitar', mode: 'Loop' },
    'Clean Guitars - Beastsamples':                   { category: 'Guitar', mode: 'Loop' },
    'Dark Phonk by Beastsamples':                     { category: 'Synth', mode: 'Loop' },
    'Dragon One-Shot Kit':                            { category: 'Drums', mode: 'OneShot' },
    'Dream Melodies by Beastsamples':                 { category: 'Synth', mode: 'Loop' },
    'Drone Bass by Beastsamples':                     { category: 'Bass',  mode: 'Loop' },
    'Drum sample toolkit by Beastsamples':             { category: 'Drums', mode: 'OneShot' },
    'Electric Rap Basslines - BETA by Beastsamples':  { category: 'Bass',  mode: 'Loop' },
    'Free Arpegiators Kit by Beastsamples':            { category: 'Synth', mode: 'Loop' },
    'Fuse Synths by Beastsamples':                    { category: 'Synth', mode: 'Loop' },
    'Ground Drumkit by Beastsamples':                 { category: 'Drums', mode: 'OneShot' },
    'Guitar Riffs by Beastsamples':                   { category: 'Guitar', mode: 'Loop' },
    'Guitar Tools by Beastsamples':                   { category: 'Guitar', mode: 'OneShot', excludeIfPathContains: [ 'tools', 'wah-wah' ] },
    'Hermetic Keys by Beastsamples':                  { category: 'Keys',  mode: 'Loop' },
    'Hi-Hats by Beastsamples':                        { category: 'Drums', mode: 'Loop' },
    'Lines Synths by Beastsamples':                   { category: 'Synth', mode: 'Loop' },
    'Liquid Guitars by Beastsamples':                 { category: 'Guitar', mode: 'Loop', onlyIfPathContains: 'wet' },
    'Lo-fi Keys by Beastsamples':                     { category: 'Keys',  mode: 'Loop' },
    'Mood Guitar Samples  FREE by Beastsamples':       { category: 'Guitar', mode: 'Loop' },
    'Native Synths by Beastsamples':                  { category: 'Synth', mode: 'Loop' },
    'Phonk Drumloops by Beastsamples':                 { category: 'Drums', mode: 'Loop' },
    'Phonk Melodies by Beastsamples':                 { category: 'Synth', mode: 'Loop' },
    'Phonk Samples Vol_I by Beastsamples':            { category: 'Synth', mode: 'Loop' },
    'Phonk Samples Vol_II by Beastsamples':           { category: 'Synth', mode: 'Loop' },

    // PHANTASIA — genuinely multi-category, no single fallback. Brass excluded per user
    // (no home in Guitar/Synth/Bass/Drums/Keys yet — see project-flow-catalog-ux-design
    // memory's "third category gap"). "guitar wavs" also matches inside "psy guitar wavs"
    // paths, but both map to the same category so match order doesn't matter here.
    'PHANTASIA by Beastsamples': {
        regions: [
            { contains: 'bass wavs',        category: 'Bass',   mode: 'Loop' },
            { contains: 'guitar wavs',      category: 'Guitar', mode: 'Loop' },
            { contains: 'synth wavs',       category: 'Synth',  mode: 'Loop' },
            { contains: 'brass melodies',   exclude: true },
        ],
    },

    // Percs & Folleys — Loops/One-Shots are explicit top-level folders (the actual Mode
    // signal, not guessed from subfolder keywords), with Category subfolders underneath.
    'Percs & Folleys by Beastsamples': {
        regions: [
            { contains: 'loops/aiffs/bass',              category: 'Bass',   mode: 'Loop' },
            { contains: 'loops/wavs/bass',                category: 'Bass',   mode: 'Loop' },
            { contains: 'loops/aiffs/guitar',            category: 'Guitar', mode: 'Loop' },
            { contains: 'loops/wavs/guitar',              category: 'Guitar', mode: 'Loop' },
            { contains: 'loops/aiffs/synth',              category: 'Synth',  mode: 'Loop' },
            { contains: 'loops/wavs/synth',                category: 'Synth',  mode: 'Loop' },
            { contains: 'loops/aiffs/hits-hats-percs',    category: 'Drums',  mode: 'Loop' },
            { contains: 'loops/wavs/hits-hats-percs',      category: 'Drums',  mode: 'Loop' },
            { contains: 'loops/aiffs/kicks',              category: 'Drums',  mode: 'Loop' },
            { contains: 'loops/wavs/kicks',                category: 'Drums',  mode: 'Loop' },
            { contains: 'loops/aiffs/snares-percs',       category: 'Drums',  mode: 'Loop' },
            { contains: 'loops/wavs/snares-percs',         category: 'Drums',  mode: 'Loop' },
            { contains: 'one-shots',                      category: 'Drums',  mode: 'OneShot' },
            { contains: 'bonus',                          category: 'Drums',  mode: 'Loop' },
        ],
    },
};

function main()
{
    const [ , , rootArg ] = process.argv;
    if (! rootArg)
    {
        console.error ('Usage: node tools/catalog-build.js "<drive root>"');
        process.exit (1);
    }

    fs.mkdirSync (CATALOG_DIR, { recursive: true });

    const manifest = [];
    let copiedCount = 0, skippedByRuleCount = 0;

    for (const [ packName, rules ] of Object.entries (APPROVED_PACKS))
    {
        const packPath = path.join (rootArg, packName);
        if (! fs.existsSync (packPath))
        {
            console.error (`WARNING: pack not found on disk, skipping: "${packName}"`);
            continue;
        }

        const files = [];
        walkWav (packPath, files);

        for (const filePath of files)
        {
            const relativePath = path.relative (packPath, filePath);
            // Normalize to forward slashes so region "contains" patterns like
            // 'loops/wavs/bass' match regardless of platform path separator.
            const relativeLower = relativePath.toLowerCase().split (path.sep).join ('/');

            let category, mode;

            if (rules.regions)
            {
                const region = rules.regions.find (r => relativeLower.includes (r.contains));
                if (! region)
                {
                    console.error (`WARNING: no region matched for "${packName}" file: ${relativePath} — skipping.`);
                    skippedByRuleCount++;
                    continue;
                }
                if (region.exclude)
                {
                    skippedByRuleCount++;
                    continue;
                }
                category = region.category;
                mode = region.mode;
            }
            else
            {
                if (rules.excludeIfPathContains && rules.excludeIfPathContains.some (s => relativeLower.includes (s)))
                {
                    skippedByRuleCount++;
                    continue;
                }
                if (rules.onlyIfPathContains && ! relativeLower.includes (rules.onlyIfPathContains))
                {
                    skippedByRuleCount++;
                    continue;
                }

                mode = rules.mode === 'inferFromPath' ? inferModeFromPath (relativeLower) : rules.mode;

                const override = rules.categoryOverrides && rules.categoryOverrides.find (o => relativeLower.includes (o.contains));
                category = override ? override.category : rules.category;
            }

            const { key, bpm } = extractKeyAndBpm (path.basename (filePath));

            const destRelative = path.join (packName, relativePath);
            const destPath = path.join (CATALOG_DIR, destRelative);
            fs.mkdirSync (path.dirname (destPath), { recursive: true });
            fs.copyFileSync (filePath, destPath);
            copiedCount++;

            manifest.push ({
                file: destRelative.split (path.sep).join ('/'),
                name: path.basename (filePath, '.wav'),
                pack: packName,
                category,
                mode,
                key,
                bpm: bpm ? parseInt (bpm, 10) : 0,
                locked: false,
            });
        }
    }

    fs.writeFileSync (MANIFEST_PATH, JSON.stringify (manifest, null, 2), 'utf8');

    console.log (`Copied ${copiedCount} files from ${Object.keys (APPROVED_PACKS).length} approved packs.`);
    console.log (`Skipped ${skippedByRuleCount} files via per-pack include/exclude rules.`);
    console.log (`Manifest written to ${MANIFEST_PATH} (${manifest.length} entries).`);

    const byCategory = {};
    for (const m of manifest)
        byCategory[m.category] = (byCategory[m.category] || 0) + 1;
    console.log ('By category:', byCategory);
}

main();
