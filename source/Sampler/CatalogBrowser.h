#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "SampleCatalog.h"

// Search + list of the test catalog. Locked entries are visible but not selectable —
// matches the business model (browse/audition everything, unlock via purchase later).
class CatalogBrowser : public juce::Component,
                       private juce::ListBoxModel,
                       private juce::TextEditor::Listener
{
public:
    CatalogBrowser();

    std::function<void (const SampleEntry&)> onSampleSelected;

    void resized() override;

private:
    int getNumRows() override;
    void paintListBoxItem (int rowNumber, juce::Graphics&, int width, int height, bool rowIsSelected) override;
    void listBoxItemClicked (int row, const juce::MouseEvent&) override;
    void textEditorTextChanged (juce::TextEditor&) override { updateFilteredList(); }

    void updateFilteredList();
    void setActiveMode (SampleMode newMode);

    juce::Array<SampleEntry> allEntries;
    juce::Array<int> filteredIndices;
    juce::StringArray favoriteFilePaths;

    SampleMode activeMode = SampleMode::Loop;

    juce::TextButton loopsTabButton { "Loops" };
    juce::TextButton oneShotsTabButton { "One-Shots" };
    juce::TextEditor searchBox;
    juce::ListBox listBox { "Catalog", this };

    static constexpr int starColumnWidth = 24;
    static constexpr int tabRowHeight = 26;
};
