#pragma once
#include "UIFrame.h"
#include "TextRenderer.h"
#include "../third_party/sokol/sokol_app.h"
#include <string>
#include <vector>
#include <deque>
#include <functional>

class ChatConsoleFrame : public UIFrame {
public:
    ChatConsoleFrame(const std::string& name, UIFrame* parent = nullptr);

    void SetTextRenderer(TextRenderer* tr) { textRenderer_ = tr; }

    void AddMessage(const std::string& msg, uint32_t color = 0xFFFFFFFF);
    void Open();
    void Close();
    bool IsOpen() const { return open_; }
    void Toggle() { if (open_) Close(); else Open(); }

    // Input
    void HandleKeyDown(int key);
    void HandleCharInput(char c);

    const std::string& GetInputBuffer() const { return inputBuffer_; }
    void ClearInput();
    void SetSendCallback(std::function<void(const std::string&)> cb);

    void Render(UiRenderer& r) override;

    // Position
    void SetDimensions(float x, float y, float w, float h);

private:
    struct ChatMessage {
        std::string text;
        uint32_t color;
    };

    std::deque<ChatMessage> messages_;
    std::string inputBuffer_;
    std::vector<std::string> history_;
    int historyPos_ = -1;
    bool open_ = false;
    TextRenderer* textRenderer_ = nullptr;
    float fontSize_ = 13.0f;
    float pad_ = 6.0f;
    int maxMessages_ = 100;
    std::function<void(const std::string&)> sendCallback_;
};
