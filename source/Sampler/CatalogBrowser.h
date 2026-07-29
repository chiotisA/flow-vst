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
    void textEditorTextChanged (juce::TextEditor&) override;
    void textEditorReturnKeyPressed (juce::TextEditor&) override { hideSuggestions(); }
    void textEditorEscapeKeyPressed (juce::TextEditor&) override { hideSuggestions(); }

    void updateFilteredList();
    void setActiveMode (SampleMode newMode);

    // Search-suggestions overlay: as you type, shows matches from the WHOLE catalog
    // regardless of the current Mode tab / Category / Key / BPM filters — the point is to
    // help you find something even when your current filters would otherwise hide it with
    // zero explanation (e.g. searching "arp" while on the One-Shots tab, when every Arp is
    // Loop content). Clicking a suggestion switches Mode/Category to match it and loads it.
    struct SuggestionsModel : public juce::ListBoxModel
    {
        explicit SuggestionsModel (CatalogBrowser& ownerIn) : owner (ownerIn) {}
        int getNumRows() override { return owner.suggestionIndices.size(); }
        void paintListBoxItem (int row, juce::Graphics& g, int w, int h, bool selected) override;
        void listBoxItemClicked (int row, const juce::MouseEvent&) override { owner.selectSuggestion (row); }
        CatalogBrowser& owner;
    };

    void updateSuggestions();
    void selectSuggestion (int suggestionRow);
    void hideSuggestions();

    juce::Array<SampleEntry> allEntries;
    juce::Array<int> filteredIndices;
    juce::StringArray favoriteFilePaths;

    SampleMode activeMode = SampleMode::Loop;

    juce::TextButton loopsTabButton { "Loops" };
    juce::TextButton oneShotsTabButton { "One-Shots" };

    // Category/Key/BPM are separate structured filters, deliberately pulled out of the
    // free-text search box — searching a key like "D" as loose substring text would match
    // almost any name/category containing that letter (e.g. "Drums"), not just samples
    // actually in the key of D.
    juce::ComboBox categoryFilterBox;
    juce::ComboBox keyFilterBox;
    juce::TextEditor bpmMinBox, bpmMaxBox;

    juce::TextEditor searchBox;
    juce::ListBox listBox { "Catalog", this };

    SuggestionsModel suggestionsModel { *this };
    juce::ListBox suggestionsListBox { "Suggestions", &suggestionsModel };
    juce::Array<int> suggestionIndices; // indices into allEntries

    static constexpr int starColumnWidth = 24;
    static constexpr int tabRowHeight = 26;
    static constexpr int filterRowHeight = 26;
    static constexpr int maxSuggestions = 8;
    static constexpr int suggestionRowHeight = 22;
};
