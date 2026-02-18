#include "ChatHistoryComponent.h"

ChatHistoryComponent::ChatHistoryComponent()
{
    viewport.setViewedComponent(&container, false);
    viewport.setScrollBarsShown(true, false);
    addAndMakeVisible(viewport);
}

ChatHistoryComponent::~ChatHistoryComponent()
{
    container.deleteAllChildren();
}

void ChatHistoryComponent::addChatEntry(const ChatEntry& entry)
{
    // Ensure we're on the message thread when modifying the component hierarchy
    jassert(juce::MessageManager::getInstance()->isThisTheMessageThread());
    
    // Limit to 10 entries
    if (entries.size() >= 10)
        entries.erase(entries.begin());
    
    entries.push_back(entry);
    rebuildChatView();
}

void ChatHistoryComponent::loadFromHistory(const std::vector<ChatEntry>& history)
{
    entries = history;
    rebuildChatView();
}

void ChatHistoryComponent::setOnMidiDragged(std::function<void(const ChatEntry&)> callback)
{
    onMidiDraggedCallback = std::move(callback);
}

void ChatHistoryComponent::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xFF9CCC65)); // #9CCC65 - same as text entry
}

void ChatHistoryComponent::resized()
{
    viewport.setBounds(getLocalBounds());
    rebuildChatView(); // Rebuild with correct width
}

void ChatHistoryComponent::rebuildChatView()
{
    // Must be called from message thread since we're modifying component hierarchy
    jassert(juce::MessageManager::getInstance()->isThisTheMessageThread());
    
    container.deleteAllChildren();
    
    // Don't rebuild if we don't have a valid size yet
    if (getWidth() <= 0)
        return;
    
    // Calculate total height needed for all entries
    int totalEntriesHeight = 5;
    std::vector<int> entryHeights;
    int entryWidth = getWidth() - 25;
    
    for (const auto& entry : entries)
    {
        auto tempComponent = ChatEntryComponent(entry, nullptr);
        int height = tempComponent.getIdealHeight(entryWidth);
        entryHeights.push_back(height);
        totalEntriesHeight += height + 10;
    }
    
    // If content is less than viewport height, start from bottom
    int startY = juce::jmax(5, viewport.getHeight() - totalEntriesHeight);
    int yPos = startY;
    
    for (size_t i = 0; i < entries.size(); ++i)
    {
        auto* entryComponent = new ChatEntryComponent(entries[i], onMidiDraggedCallback);
        entryComponent->setBounds(5, yPos, entryWidth, entryHeights[i]);
        container.addAndMakeVisible(entryComponent);
        yPos += entryHeights[i] + 10;
    }
    
    updateContainerSize();
    // Scroll to bottom to show most recent
    viewport.setViewPosition(0, juce::jmax(0, container.getHeight() - viewport.getHeight()));
}

void ChatHistoryComponent::updateContainerSize()
{
    int totalHeight = 10;
    for (auto* child : container.getChildren())
        totalHeight += child->getHeight() + 10;
    
    container.setSize(getWidth() - 20, juce::jmax(totalHeight, getHeight()));
}

ChatHistoryComponent::ChatEntryComponent::ChatEntryComponent(const ChatEntry& e, std::function<void(const ChatEntry&)> onDragged)
    : entry(e), onMidiDragged(std::move(onDragged))
{
}

void ChatHistoryComponent::ChatEntryComponent::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colours::transparentBlack);
    
    int y = 10;
    int padding = 10;
    
    // Calculate prompt text height
    int promptHeight = getTextHeight(entry.prompt, getWidth());
    
    // Draw prompt bubble
    juce::Rectangle<int> promptBubble(padding, y, getWidth() - 2 * padding, promptHeight + 10);
    g.setColour(juce::Colour(0xFF795C34)); // #795C34 brown bubble
    g.fillRoundedRectangle(promptBubble.toFloat(), 8.0f);
    
    g.setColour(juce::Colours::black);
    juce::Font promptFont (getLookAndFeel().getTypefaceForFont (juce::Font()));
    promptFont.setHeight(12.0f);
    g.setFont(promptFont);
    g.drawMultiLineText(entry.prompt, padding + 8, y + 18, getWidth() - 2 * padding - 16);
    
    y += promptHeight + 20;
    
    // Draw MIDI file info below the bubble
    if (entry.midiFile.existsAsFile())
    {
        g.setColour(juce::Colours::black);
        juce::Font midiFont (getLookAndFeel().getTypefaceForFont (juce::Font()));
        midiFont.setHeight(10.0f);
        midiFont.setItalic(true);
        g.setFont(midiFont);
        g.drawText("MIDI: " + entry.midiFile.getFileName(), padding + 5, y, getWidth() - 2 * padding, 15, juce::Justification::left);
    }
}

void ChatHistoryComponent::ChatEntryComponent::mouseDrag(const juce::MouseEvent& e)
{
    if (entry.midiFile.existsAsFile() && e.mouseWasDraggedSinceMouseDown())
    {
        juce::StringArray files;
        files.add(entry.midiFile.getFullPathName());
        juce::DragAndDropContainer::performExternalDragDropOfFiles(files, true);

        if (onMidiDragged)
            onMidiDragged(entry);
    }
}

int ChatHistoryComponent::ChatEntryComponent::getIdealHeight(int width) const
{
    int height = 20; // Top padding
    height += getTextHeight(entry.prompt, width) + 10; // Prompt bubble
    if (entry.midiFile.existsAsFile())
        height += 25; // MIDI file info
    height += 10; // Bottom padding
    return height;
}

int ChatHistoryComponent::ChatEntryComponent::getTextHeight(const juce::String& text, int width) const
{
    // Count newlines manually
    int lines = 1;
    for (int i = 0; i < text.length(); i++)
    {
        if (text[i] == '\n')
            lines++;
    }
    
    // Use provided width, with fallback to reasonable default
    int usableWidth = juce::jmax(100, width - 40); // Account for padding
    int charsPerLine = usableWidth / 7; // ~7 pixels per character at font size 11
    if (text.length() > charsPerLine)
        lines += (text.length() / charsPerLine);
    return lines * 16; // 16 pixels per line
}
