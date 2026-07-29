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

    searchBox.setTextToShowWhenEmpty ("Search name, category, key...", juce::Colours::grey);
    searchBox.addListener (this);
    addAndMakeVisible (searchBox);

    listBox.setRowHeight (28);
    addAndMakeVisible (listBox);

    setActiveMode (activeMode);
}

void CatalogBrowser::resized()
{
    auto area = getLocalBounds();
    auto tabRow = area.removeFromTop (tabRowHeight);
    loopsTabButton.setBounds (tabRow.removeFromLeft (tabRow.getWidth() / 2));
    oneShotsTabButton.setBounds (tabRow);
    searchBox.setBounds (area.removeFromTop (28));
    listBox.setBounds (area);
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

    for (int i = 0; i < allEntries.size(); ++i)
    {
        const auto& e = allEntries.getReference (i);

        if (e.mode != activeMode)
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
