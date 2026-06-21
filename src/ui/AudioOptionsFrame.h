#pragma once
#include "UIFrame.h"
#include "SliderFrame.h"
#include "ButtonFrame.h"
#include "TextRenderer.h"

class AudioSystem;

class AudioOptionsFrame : public UIFrame {
public:
    AudioOptionsFrame(const std::string& name, UIFrame* parent = nullptr);

    void SetTextRenderer(TextRenderer* tr);
    void SetAudioSystem(AudioSystem* audio) { audio_ = audio; }

    void Open();
    void Close();
    bool IsOpen() const { return visible_; }
    void Toggle() { if (visible_) Close(); else Open(); }

    void Render(UiRenderer& r) override;
    UIFrame* OnMouseDown(float sx, float sy, int btn) override;
    UIFrame* OnMouseUp(float sx, float sy, int btn) override;
    UIFrame* HitTest(float sx, float sy) override;

private:
    AudioSystem* audio_ = nullptr;
    SliderFrame masterVol_;
    SliderFrame musicVol_;
    SliderFrame sfxVol_;
    ButtonFrame closeBtn_;
    TextRenderer* textRenderer_ = nullptr;

    void SyncVolumes();
};
