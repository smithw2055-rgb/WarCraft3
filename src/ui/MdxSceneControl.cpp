#include "MdxSceneControl.h"

#include <NsCore/DynamicCast.h>
#include <NsCore/ReflectionImplement.h>
#include <NsCore/String.h>
#include <NsDrawing/Point.h>
#include <NsDrawing/Rect.h>
#include <NsDrawing/Size.h>
#include <NsGui/DrawingContext.h>
#include <NsGui/FrameworkPropertyMetadata.h>
#include <NsGui/UIElementData.h>
#include <NsGui/Viewbox.h>
#include <NsRender/RenderDevice.h>
#include <NsRender/Texture.h>

#include <algorithm>
#include <cmath>
#include <vector>

namespace War3UI {
namespace {

std::vector<MdxSceneControl*>& Controls() {
    static std::vector<MdxSceneControl*> controls;
    return controls;
}

int TextureExtent(float value) {
    if (!std::isfinite(value) || value <= 0.0f) {
        return 1;
    }
    return std::max(1, (int)(value + 0.5f));
}

Noesis::Size FillOrFallback(const Noesis::Size& size) {
    const bool finiteW = std::isfinite(size.width);
    const bool finiteH = std::isfinite(size.height);
    if (finiteW && finiteH) {
        return size;
    }
    if (finiteW) {
        return Noesis::Size(size.width, 720.0f);
    }
    if (finiteH) {
        return Noesis::Size(1280.0f, size.height);
    }
    return Noesis::Size(1280.0f, 720.0f);
}

} // namespace

MdxSceneControl::MdxSceneControl() {
    Controls().push_back(this);
}

MdxSceneControl::~MdxSceneControl() {
    auto& controls = Controls();
    controls.erase(std::remove(controls.begin(), controls.end(), this), controls.end());
}

void MdxSceneControl::SetSharedContext(MpqManager* mpq, int gameVersion) {
    MdxUiSceneRenderer::SetSharedContext(mpq, gameVersion);
}

void MdxSceneControl::RenderActiveScenes(float dt) {
    for (MdxSceneControl* control : Controls()) {
        if (control != nullptr) {
            control->RenderFrame(dt);
        }
    }
}

const char* MdxSceneControl::GetSceneKey() const {
    return GetValue<Noesis::String>(SceneKeyProperty).Str();
}

void MdxSceneControl::SetSceneKey(const char* value) {
    SetValue<Noesis::String>(SceneKeyProperty, value ? value : "");
}

const char* MdxSceneControl::GetModelPath() const {
    return GetValue<Noesis::String>(ModelPathProperty).Str();
}

void MdxSceneControl::SetModelPath(const char* value) {
    SetValue<Noesis::String>(ModelPathProperty, value ? value : "");
}

const char* MdxSceneControl::GetSequence() const {
    return GetValue<Noesis::String>(SequenceProperty).Str();
}

void MdxSceneControl::SetSequence(const char* value) {
    SetValue<Noesis::String>(SequenceProperty, value ? value : "");
}

bool MdxSceneControl::GetAutoPlay() const {
    return GetValue<bool>(AutoPlayProperty);
}

void MdxSceneControl::SetAutoPlay(bool value) {
    SetValue<bool>(AutoPlayProperty, value);
}

bool MdxSceneControl::GetUseModelCamera() const {
    return GetValue<bool>(UseModelCameraProperty);
}

void MdxSceneControl::SetUseModelCamera(bool value) {
    SetValue<bool>(UseModelCameraProperty, value);
}

Noesis::Stretch MdxSceneControl::GetStretch() const {
    return GetValue<Noesis::Stretch>(StretchProperty);
}

void MdxSceneControl::SetStretch(Noesis::Stretch value) {
    SetValue<Noesis::Stretch>(StretchProperty, value);
}

bool MdxSceneControl::GetIsActive() const {
    return GetValue<bool>(IsActiveProperty);
}

void MdxSceneControl::SetIsActive(bool value) {
    SetValue<bool>(IsActiveProperty, value);
}

void MdxSceneControl::OnRender(Noesis::DrawingContext* context) {
    if (!GetIsActive() || context == nullptr) {
        return;
    }

    const Noesis::Size renderSize((float)GetActualWidth(), (float)GetActualHeight());
    if (renderSize.width <= 0.0f || renderSize.height <= 0.0f) {
        return;
    }

    EnsureTextureSource(TextureExtent(renderSize.width), TextureExtent(renderSize.height));
    if (textureSource_ != nullptr) {
        context->DrawImage(textureSource_, ComputeImageRect(renderSize));
    }
}

Noesis::Size MdxSceneControl::MeasureOverride(const Noesis::Size& availableSize) {
    return FillOrFallback(availableSize);
}

Noesis::Size MdxSceneControl::ArrangeOverride(const Noesis::Size& finalSize) {
    return FillOrFallback(finalSize);
}

void MdxSceneControl::MarkRendererDirty() {
    rendererDirty_ = true;
    InvalidateMeasure();
    InvalidateVisual();
}

void MdxSceneControl::EnsureRendererConfigured() {
    const std::string sceneKey = GetSceneKey() ? GetSceneKey() : "";
    const std::string modelPath = GetModelPath() ? GetModelPath() : "";
    const std::string sequence = GetSequence() ? GetSequence() : "";
    const bool autoPlay = GetAutoPlay();
    const bool useModelCamera = GetUseModelCamera();

    if (!rendererDirty_ && sceneKey == appliedSceneKey_ && modelPath == appliedModelPath_ &&
        sequence == appliedSequence_ && autoPlay == appliedAutoPlay_ &&
        useModelCamera == appliedUseModelCamera_) {
        return;
    }

    renderer_.Configure(sceneKey.c_str(), modelPath.c_str(), sequence.c_str(), autoPlay, useModelCamera);
    appliedSceneKey_ = sceneKey;
    appliedModelPath_ = modelPath;
    appliedSequence_ = sequence;
    appliedAutoPlay_ = autoPlay;
    appliedUseModelCamera_ = useModelCamera;
    rendererDirty_ = false;
}

void MdxSceneControl::EnsureTextureSource(int width, int height) {
    width = std::max(1, width);
    height = std::max(1, height);
    renderer_.Resize(width, height);
    if (textureSource_ == nullptr) {
        textureSource_ = *new Noesis::DynamicTextureSource((uint32_t)width, (uint32_t)height,
            &MdxSceneControl::TextureRenderCallback, this);
    } else if (textureWidth_ != width || textureHeight_ != height) {
        textureSource_->Resize((uint32_t)width, (uint32_t)height);
    }
    textureWidth_ = width;
    textureHeight_ = height;
}

void MdxSceneControl::RenderFrame(float dt) {
    if (!GetIsActive()) {
        return;
    }

    const int width = TextureExtent((float)GetActualWidth());
    const int height = TextureExtent((float)GetActualHeight());
    EnsureTextureSource(width, height);
    EnsureRendererConfigured();
    renderer_.Render(dt);
    InvalidateVisual();
}

Noesis::Rect MdxSceneControl::ComputeImageRect(const Noesis::Size& arrangeSize) const {
    if (textureWidth_ <= 0 || textureHeight_ <= 0) {
        return Noesis::Rect(arrangeSize);
    }

    const Noesis::Size contentSize((float)textureWidth_, (float)textureHeight_);
    const Noesis::Point scale = Noesis::Viewbox::GetStretchScale(contentSize, arrangeSize,
        GetStretch(), Noesis::StretchDirection_Both);
    const float width = contentSize.width * scale.x;
    const float height = contentSize.height * scale.y;
    const float x = (arrangeSize.width - width) * 0.5f;
    const float y = (arrangeSize.height - height) * 0.5f;
    return Noesis::Rect(x, y, width, height);
}

Noesis::Texture* MdxSceneControl::TextureRenderCallback(Noesis::RenderDevice*, void* user) {
    MdxSceneControl* control = static_cast<MdxSceneControl*>(user);
    return control != nullptr ? control->renderer_.GetTexture() : nullptr;
}

void MdxSceneControl::OnScenePropertyChanged(Noesis::DependencyObject* d,
    const Noesis::DependencyPropertyChangedEventArgs&) {
    MdxSceneControl* control = Noesis::DynamicCast<MdxSceneControl*>(d);
    if (control != nullptr) {
        control->MarkRendererDirty();
    }
}

NS_BEGIN_COLD_REGION

NS_IMPLEMENT_REFLECTION(MdxSceneControl, "War3UI.MdxSceneControl")
{
    Noesis::UIElementData* data = NsMeta<Noesis::UIElementData>(Noesis::TypeOf<SelfClass>());
    const uint32_t affectsMeasureRender =
        Noesis::FrameworkPropertyMetadataOptions_AffectsMeasure |
        Noesis::FrameworkPropertyMetadataOptions_AffectsRender;

    data->RegisterProperty<Noesis::String>(SceneKeyProperty, "SceneKey",
        Noesis::FrameworkPropertyMetadata::Create(Noesis::String(),
            affectsMeasureRender, Noesis::PropertyChangedCallback(OnScenePropertyChanged)));
    data->RegisterProperty<Noesis::String>(ModelPathProperty, "ModelPath",
        Noesis::FrameworkPropertyMetadata::Create(Noesis::String(),
            affectsMeasureRender, Noesis::PropertyChangedCallback(OnScenePropertyChanged)));
    data->RegisterProperty<Noesis::String>(SequenceProperty, "Sequence",
        Noesis::FrameworkPropertyMetadata::Create(Noesis::String("Stand"),
            affectsMeasureRender, Noesis::PropertyChangedCallback(OnScenePropertyChanged)));
    data->RegisterProperty<bool>(AutoPlayProperty, "AutoPlay",
        Noesis::FrameworkPropertyMetadata::Create(true,
            affectsMeasureRender, Noesis::PropertyChangedCallback(OnScenePropertyChanged)));
    data->RegisterProperty<bool>(UseModelCameraProperty, "UseModelCamera",
        Noesis::FrameworkPropertyMetadata::Create(true,
            affectsMeasureRender, Noesis::PropertyChangedCallback(OnScenePropertyChanged)));
    data->RegisterProperty<Noesis::Stretch>(StretchProperty, "Stretch",
        Noesis::FrameworkPropertyMetadata::Create(Noesis::Stretch_Uniform,
            affectsMeasureRender, Noesis::PropertyChangedCallback(OnScenePropertyChanged)));
    data->RegisterProperty<bool>(IsActiveProperty, "IsActive",
        Noesis::FrameworkPropertyMetadata::Create(true,
            affectsMeasureRender, Noesis::PropertyChangedCallback(OnScenePropertyChanged)));
}

NS_END_COLD_REGION

const Noesis::DependencyProperty* MdxSceneControl::SceneKeyProperty;
const Noesis::DependencyProperty* MdxSceneControl::ModelPathProperty;
const Noesis::DependencyProperty* MdxSceneControl::SequenceProperty;
const Noesis::DependencyProperty* MdxSceneControl::AutoPlayProperty;
const Noesis::DependencyProperty* MdxSceneControl::UseModelCameraProperty;
const Noesis::DependencyProperty* MdxSceneControl::StretchProperty;
const Noesis::DependencyProperty* MdxSceneControl::IsActiveProperty;

} // namespace War3UI
