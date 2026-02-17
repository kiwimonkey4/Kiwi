#pragma once
#include <JuceHeader.h>
#include "ChatEntry.h"
#include <functional>

class ChatHistoryComponent : public juce::Component
{
public:
    ChatHistoryComponent();
    ~ChatHistoryComponent();
    
    void addChatEntry(const ChatEntry& entry);
    void loadFromHistory(const std::vector<ChatEntry>& history);
    void setOnMidiDragged(std::function<void(const ChatEntry&)> callback);
    void paint(juce::Graphics& g) override;
    void resized() override;
    
private:
    void rebuildChatView();
    void updateContainerSize();
    
    class ChatEntryComponent : public juce::Component
    {
    public:
        ChatEntryComponent(const ChatEntry& e, std::function<void(const ChatEntry&)> onDragged = nullptr);
        
        void paint(juce::Graphics& g) override;
        void mouseDrag(const juce::MouseEvent& e) override;
        int getIdealHeight(int width) const;
        
    private:
        int getTextHeight(const juce::String& text, int width) const;
        ChatEntry entry;
        std::function<void(const ChatEntry&)> onMidiDragged;
    };
    
    std::vector<ChatEntry> entries;
    juce::Viewport viewport;
    juce::Component container;

    std::function<void(const ChatEntry&)> onMidiDraggedCallback;
};
