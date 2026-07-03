#pragma once

#include "MdxUiSceneRenderer.h"

#include <NsCore/Aero.h>
#include <NsCore/Ptr.h>
#include <NsGui/DynamicTextureSource.h>
#include <NsGui/FrameworkElement.h>
#include <NsGui/Enums.h>

#include <string>

class MpqManager;

namespace War3UI {

class MdxSceneControl final : public Aero::FrameworkElement {
public:
    MdxSceneControl();
    ~MdxSceneControl();

    static void SetSharedContext(MpqManager* mpq, int gameVersion);
    static void RenderActiveScenes(float dt);

    const char* GetSceneKey() const;
    void SetSceneKey(const char* value);

    const char* GetModelPath() const;
    void SetModelPath(const char* value);

    const char* GetSequence() const;
    void SetSequence(const char* value);

    bool GetAutoPlay() const;
    void SetAutoPlay(bool value);

    bool GetUseModelCamera() const;
    void SetUseModelCamera(bool value);

    Aero::Stretch GetStretch() const;
    void SetStretch(Aero::Stretch value);

    bool GetIsActive() const;
    void SetIsActive(bool value);

    void RenderFrame(float dt);

    static const Aero::DependencyProperty* SceneKeyProperty;
    static const Aero::DependencyProperty* ModelPathProperty;
    static const Aero::DependencyProperty* SequenceProperty;
    static const Aero::DependencyProperty* AutoPlayProperty;
    static const Aero::DependencyProperty* UseModelCameraProperty;
    static const Aero::DependencyProperty* StretchProperty;
    static const Aero::DependencyProperty* IsActiveProperty;

protected:
    void OnRender(Aero::DrawingContext* context) override;
    Aero::Size MeasureOverride(const Aero::Size& availableSize) override;
    Aero::Size ArrangeOverride(const Aero::Size& finalSize) override;

private:
    void MarkRendererDirty();
    void EnsureRendererConfigured();
    void EnsureTextureSource(int width, int height);
    Aero::Rect ComputeImageRect(const Aero::Size& arrangeSize) const;

    static Aero::Texture* TextureRenderCallback(Aero::RenderDevice* device, void* user);
    static void OnScenePropertyChanged(Aero::DependencyObject* d,
        const Aero::DependencyPropertyChangedEventArgs& e);

    MdxUiSceneRenderer renderer_;
    Aero::Ptr<Aero::DynamicTextureSource> textureSource_;
    std::string appliedSceneKey_;
    std::string appliedModelPath_;
    std::string appliedSequence_;
    bool appliedAutoPlay_ = true;
    bool appliedUseModelCamera_ = true;
    int textureWidth_ = 0;
    int textureHeight_ = 0;
    bool rendererDirty_ = true;

    NS_DECLARE_REFLECTION(MdxSceneControl, FrameworkElement)
};

} // namespace War3UI
