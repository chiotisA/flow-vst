#include "CatalogBrowser.h"

CatalogBrowser::CatalogBrowser()
{
    allEntries = buildTestCatalog();

    loopsTabButton.setClickingTogglesState (false);
    oneShotsTabButton.setClickingTogglesState (false);
    loopsTabButton.onClick = [this] { setActiveMode (SampleMode::Loop); };
    oneShotsTabButton.onClick = [this] { setActiveMode (SampleMode::OneShot); };
    addAndMakeVisible (loopsTabButton);
    addAndMakeVisible (oneShotsTabButton);

    categoryFilterBox.addItem ("All Categories", 1);
    categoryFilterBox.addItem ("Guitar", 2);
    categoryFilterBox.addItem ("Synth", 3);
    categoryFilterBox.addItem ("Bass", 4);
    categoryFilterBox.addItem ("Drums", 5);
    categoryFilterBox.addItem ("Keys", 6);
    categoryFilterBox.setSelectedId (1, juce::dontSendNotification);
    categoryFilterBox.onChange = [this] { updateFilteredList(); };
    addAndMakeVisible (categoryFilterBox);

    keyFilterBox.addItem ("Any Key", 1);
    {
        static const char* noteNames[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
        for (int i = 0; i < 12; ++i)
            keyFilterBox.addItem (noteNames[i], i + 2);
    }
    keyFilterBox.setSelectedId (1, juce::dontSendNotification);
    keyFilterBox.onChange = [this] { updateFilteredList(); };
    addAndMakeVisible (keyFilterBox);

    bpmMinBox.setTextToShowWhenEmpty ("Min BPM", juce::Colours::grey);
    bpmMinBox.setInputRestrictions (3, "0123456789");
    bpmMinBox.addListener (this);
    addAndMakeVisible (bpmMinBox);

    bpmMaxBox.setTextToShowWhenEmpty ("Max BPM", juce::Colours::grey);
    bpmMaxBox.setInputRestrictions (3, "0123456789");
    bpmMaxBox.addListener (this);
    addAndMakeVisible (bpmMaxBox);

    searchBox.setTextToShowWhenEmpty ("Search name, category, key...", juce::Colours::grey);
    searchBox.addListener (this);
    addAndMakeVisible (searchBox);

    listBox.setRowHeight (28);
    addAndMakeVisible (listBox);

    // Starts hidden — addChildComponent (not addAndMakeVisible) so it exists in the
    // hierarchy but doesn't show until updateSuggestions() has real matches to display.
    suggestionsListBox.setRowHeight (suggestionRowHeight);
    addChildComponent (suggestionsListBox);

    setActiveMode (activeMode);
}

void CatalogBrowser::resized()
{
    auto area = getLocalBounds();
    auto tabRow = area.removeFromTop (tabRowHeight);
    loopsTabButton.setBounds (tabRow.removeFromLeft (tabRow.getWidth() / 2));
    oneShotsTabButton.setBounds (tabRow);

    categoryFilterBox.setBounds (area.removeFromTop (filterRowHeight));

    auto keyBpmRow = area.removeFromTop (filterRowHeight);
    keyFilterBox.setBounds (keyBpmRow.removeFromLeft (keyBpmRow.getWidth() / 3));
    bpmMinBox.setBounds (keyBpmRow.removeFromLeft (keyBpmRow.getWidth() / 2));
    bpmMaxBox.setBounds (keyBpmRow);

    auto searchArea = area.removeFromTop (28);
    searchBox.setBounds (searchArea);
    listBox.setBounds (area);

    // Overlays the top of the main list, directly under the search box — bring to front
    // only when actually shown (see updateSuggestions()).
    const auto suggestionsHeight = juce::jmin (maxSuggestions, suggestionIndices.size()) * suggestionRowHeight;
    suggestionsListBox.setBounds (area.withHeight (juce::jmax (suggestionRowHeight, suggestionsHeight)));
}

void CatalogBrowser::setActiveMode (SampleMode newMode)
{
    activeMode = newMode;

    // Tab acts as a single-active toggle, not multi-select — searching within Loops only
    // ever returns Loops, same search under One-Shots only ever returns One-Shots.
    loopsTabButton.setColour (juce::TextButton::buttonColourId,
                               activeMode == SampleMode::Loop ? juce::Colours::darkslategrey : juce::Colours::black);
    oneShotsTabButton.setColour (juce::TextButton::buttonColourId,
                                  activeMode == SampleMode::OneShot ? juce::Colours::darkslategrey : juce::Colours::black);

    updateFilteredList();
}

void CatalogBrowser::updateFilteredList()
{
    filteredIndices.clear();
    const auto query = searchBox.getText().toLowerCase();

    const bool categoryFilterActive = categoryFilterBox.getSelectedId() > 1;
    const auto categoryFilter = categoryFilterBox.getText();

    const bool keyFilterActive = keyFilterBox.getSelectedId() > 1;
    const auto keyFilter = keyFilterBox.getText();

    const bool bpmMinActive = bpmMinBox.getText().isNotEmpty();
    const bool bpmMaxActive = bpmMaxBox.getText().isNotEmpty();
    const auto bpmMin = bpmMinBox.getText().getIntValue();
    const auto bpmMax = bpmMaxBox.getText().getIntValue();

    for (int i = 0; i < allEntries.size(); ++i)
    {
        const auto& e = allEntries.getReference (i);

        if (e.mode != activeMode)
            continue;

        if (categoryFilterActive && ! e.category.equalsIgnoreCase (categoryFilter))
            continue;

        if (keyFilterActive && ! e.key.equalsIgnoreCase (keyFilter))
            continue;

        // A sample with no BPM data can't match a BPM range — exclude it rather than
        // treating "unknown" as "anything goes."
        if (bpmMinActive && (e.bpm == 0 || e.bpm < bpmMin))
            continue;
        if (bpmMaxActive && (e.bpm == 0 || e.bpm > bpmMax))
            continue;

        if (query.isEmpty()
            || e.name.toLowerCase().contains (query)
            || e.category.toLowerCase().contains (query)
            || e.key.toLowerCase().contains (query))
        {
            filteredIndices.add (i);
        }
    }

    listBox.updateContent();
    listBox.repaint();
}

void CatalogBrowser::textEditorTextChanged (juce::TextEditor&)
{
    updateFilteredList();
    updateSuggestions();
}

void CatalogBrowser::updateSuggestions()
{
    const auto query = searchBox.getText().toLowerCase();
    suggestionIndices.clear();

    // Deliberately searches the WHOLE catalog, ignoring Mode/Category/Key/BPM — the point
    // is to surface something even when the current filters would otherwise hide it with
    // zero explanation (e.g. searching "arp" while on One-Shots, when every Arp is Loop).
    if (query.isNotEmpty())
    {
        for (int i = 0; i < allEntries.size() && suggestionIndices.size() < maxSuggestions; ++i)
        {
            const auto& e = allEntries.getReference (i);
            if (e.name.toLowerCase().contains (query)
                || e.category.toLowerCase().contains (query)
                || e.key.toLowerCase().contains (query))
            {
                suggestionIndices.add (i);
            }
        }
    }

    suggestionsListBox.setVisible (! suggestionIndices.isEmpty());
    suggestionsListBox.toFront (false);
    suggestionsListBox.updateContent();
    resized();
}

void CatalogBrowser::selectSuggestion (int suggestionRow)
{
    if (! juce::isPositiveAndBelow (suggestionRow, suggestionIndices.size()))
        return;

    const auto& entry = allEntries.getReference (suggestionIndices[suggestionRow]);

    if (entry.locked)
        return; // Not purchased — task 6 (licensing) wires up the real unlock flow.

    // Switch the browsing context to match, so the user can see where this result
    // actually lives rather than it just appearing to teleport in from nowhere.
    setActiveMode (entry.mode);
    categoryFilterBox.setText (entry.category, juce::dontSendNotification);

    hideSuggestions();

    if (onSampleSelected)
        onSampleSelected (entry);
}

void CatalogBrowser::hideSuggestions()
{
    suggestionsListBox.setVisible (false);
}

void CatalogBrowser::SuggestionsModel::paintListBoxItem (int row, juce::Graphics& g, int w, int h, bool selected)
{
    if (! juce::isPositiveAndBelow (row, owner.suggestionIndices.size()))
        return;

    const auto& entry = owner.allEntries.getReference (owner.suggestionIndices[row]);

    g.fillAll (selected ? juce::Colours::darkslategrey : juce::Colours::black.withAlpha (0.95f));
    g.setColour (entry.locked ? juce::Colours::grey : juce::Colours::white);
    g.setFont (12.0f);

    auto label = entry.name + "  ["
               + (entry.mode == SampleMode::Loop ? juce::String ("Loop") : juce::String ("One-Shot"))
               + " - " + entry.category + "]";
    g.drawText (label, juce::Rectangle<int> (0, 0, w, h).reduced (4, 0), juce::Justification::centredLeft, true);
}

int CatalogBrowser::getNumRows()
{
    return filteredIndices.size();
}

void CatalogBrowser::paintListBoxItem (int rowNumber, juce::Graphics& g, int width, int height, bool rowIsSelected)
{
    if (! juce::isPositiveAndBelow (rowNumber, filteredIndices.size()))
        return;

    const auto& entry = allEntries.getReference (filteredIndices[rowNumber]);

    g.fillAll (rowIsSelected ? juce::Colours::darkslategrey : juce::Colours::black);

    const bool isFavorite = favoriteFilePaths.contains (entry.file.getFullPathName());
    g.setColour (juce::Colours::white);
    g.setFont (14.0f);
    g.drawText (isFavorite ? "*" : "o", juce::Rectangle<int> (0, 0, starColumnWidth, height), juce::Justification::centred, false);

    auto textArea = juce::Rectangle<int> (starColumnWidth, 0, width - starColumnWidth, height);
    g.setColour (entry.locked ? juce::Colours::grey : juce::Colours::white);
    g.setFont (13.0f);

    auto label = entry.name + "  [" + entry.category + " - " + entry.key
               + (entry.bpm > 0 ? " - " + juce::String (entry.bpm) + " bpm]" : "]")
               + (entry.locked ? "  (locked)" : "");
    g.drawText (label, textArea.reduced (4, 0), juce::Justification::centredLeft, true);
}

void CatalogBrowser::listBoxItemClicked (int row, const juce::MouseEvent& e)
{
    if (! juce::isPositiveAndBelow (row, filteredIndices.size()))
        return;

    const auto& entry = allEntries.getReference (filteredIndices[row]);

    if (e.x < starColumnWidth)
    {
        const auto path = entry.file.getFullPathName();
        if (favoriteFilePaths.contains (path))
            favoriteFilePaths.removeString (path);
        else
            favoriteFilePaths.add (path);

        listBox.repaintRow (row);
        return;
    }

    if (entry.locked)
        return; // Not purchased — task 6 (licensing) wires up the real unlock flow.

    if (onSampleSelected)
        onSampleSelected (entry);
}
