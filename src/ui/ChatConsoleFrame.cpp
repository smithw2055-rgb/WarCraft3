#include "ChatConsoleFrame.h"
#include <cstdio>
#include <algorithm>

ChatConsoleFrame::ChatConsoleFrame(const std::string& name, UIFrame* parent)
    : UIFrame(name, parent) {
    // Add welcome message
    AddMessage("Chat console. Press Enter to send, Esc to close.", 0xFF888888);
}

void ChatConsoleFrame::SetSendCallback(std::function<void(const std::string&)> cb) {
    sendCallback_ = std::move(cb);
}

void ChatConsoleFrame::AddMessage(const std::string& msg, uint32_t color) {
    messages_.push_back({ msg, color });
    if ((int)messages_.size() > maxMessages_)
        messages_.pop_front();
}

void ChatConsoleFrame::Open() {
    open_ = true;
    inputBuffer_.clear();
    historyPos_ = (int)history_.size();
}

void ChatConsoleFrame::Close() {
    open_ = false;
    inputBuffer_.clear();
}

void ChatConsoleFrame::ClearInput() {
    inputBuffer_.clear();
}

void ChatConsoleFrame::HandleKeyDown(int key) {
    if (!open_) return;

    switch (key) {
    case SAPP_KEYCODE_ESCAPE:
        Close();
        break;
    case SAPP_KEYCODE_ENTER:
        if (!inputBuffer_.empty()) {
            history_.push_back(inputBuffer_);
            if (sendCallback_) sendCallback_(inputBuffer_);
            inputBuffer_.clear();
            historyPos_ = (int)history_.size();
        }
        break;
    case SAPP_KEYCODE_BACKSPACE:
        if (!inputBuffer_.empty())
            inputBuffer_.pop_back();
        break;
    case SAPP_KEYCODE_UP:
        if (historyPos_ > 0) {
            historyPos_--;
            inputBuffer_ = history_[historyPos_];
        }
        break;
    case SAPP_KEYCODE_DOWN:
        if (historyPos_ < (int)history_.size() - 1) {
            historyPos_++;
            inputBuffer_ = history_[historyPos_];
        } else {
            historyPos_ = (int)history_.size();
            inputBuffer_.clear();
        }
        break;
    default:
        break;
    }
}

void ChatConsoleFrame::HandleCharInput(char c) {
    if (!open_) return;
    if (c >= 32 && c <= 126)
        inputBuffer_ += c;
}

void ChatConsoleFrame::SetDimensions(float x, float y, float w, float h) {
    SetPosition(x, y);
    SetSize(w, h);
}

void ChatConsoleFrame::Render(UiRenderer& r) {
    if (!visible_ || !textRenderer_) return;

    if (open_) {
        // Input area at bottom
        float inputH = fontSize_ + pad_ * 2;
        float inputY = bounds_.y;
        float inputW = bounds_.w;

        // Input background
        r.DrawQuad(bounds_.x, inputY, inputW, inputH, 0xCC000000);
        r.DrawQuad(bounds_.x, inputY + inputH - 1, inputW, 1, 0xFF4488FF);

        std::string display = "> " + inputBuffer_ + (open_ ? "_" : "");
        textRenderer_->RenderText(r, bounds_.x + pad_, inputY + pad_,
                                 display.c_str(), 0xFFFFFFFF, fontSize_);
    }

    // Message log (above input area)
    if (open_) {
        float msgAreaH = bounds_.h - (fontSize_ + pad_ * 2) - 4;
        float msgY = bounds_.y + (fontSize_ + pad_ * 2) + 4;
        float msgX = bounds_.x + pad_;

        // Background for message area
        r.DrawQuad(bounds_.x, msgY, bounds_.w, msgAreaH, 0xAA000000);

        // Show last N messages that fit
        int maxLines = (int)(msgAreaH / (fontSize_ + 2));
        int start = std::max(0, (int)messages_.size() - maxLines);

        float curY = msgY + msgAreaH - (fontSize_ + 2);
        for (int i = start; i < (int)messages_.size() && curY >= msgY; i++) {
            textRenderer_->RenderText(r, msgX, curY,
                                     messages_[i].text.c_str(),
                                     messages_[i].color, fontSize_);
            curY -= fontSize_ + 2;
        }
    } else {
        // When closed, show only the last message as a small overlay
        // Skip for now to keep it clean
    }
}
