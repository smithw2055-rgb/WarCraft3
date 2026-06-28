#pragma once

#include "MdxUiSceneRenderer.h"

#include <NsCore/Noesis.h>
#include <NsCore/Ptr.h>
#include <NsGui/DynamicTextureSource.h>
#include <NsGui/FrameworkElement.h>
#include <NsGui/Enums.h>

#include <string>

class MpqManager;

namespace War3UI {

class MdxSceneControl final : public Noesis::FrameworkElement {
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

    Noesis::Stretch GetStretch() const;
    void SetStretch(Noesis::Stretch value);

    bool GetIsActive() const;
    void SetIsActive(bool value);

    void RenderFrame(float dt);

    static const Noesis::DependencyProperty* SceneKeyProperty;
    static const Noesis::DependencyProperty* ModelPathProperty;
    static const Noesis::DependencyProperty* SequenceProperty;
    static const Noesis::DependencyProperty* AutoPlayProperty;
    static const Noesis::DependencyProperty* UseModelCameraProperty;
    static const Noesis::DependencyProperty* StretchProperty;
    static const Noesis::DependencyProperty* IsActiveProperty;

protected:
    void OnRender(Noesis::DrawingContext* context) override;
    Noesis::Size MeasureOverride(const Noesis::Size& availableSize) override;
    Noesis::Size ArrangeOverride(const Noesis::Size& finalSize) override;

private:
    void MarkRendererDirty();
    void EnsureRendererConfigured();
    void EnsureTextureSource(int width, int height);
    Noesis::Rect ComputeImageRect(const Noesis::Size& arrangeSize) const;

    static Noesis::Texture* TextureRenderCallback(Noesis::RenderDevice* device, void* user);
    static void OnScenePropertyChanged(Noesis::DependencyObject* d,
        const Noesis::DependencyPropertyChangedEventArgs& e);

    MdxUiSceneRenderer renderer_;
    Noesis::Ptr<Noesis::DynamicTextureSource> textureSource_;
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
