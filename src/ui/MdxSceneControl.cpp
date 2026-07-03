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

Aero::Size FillOrFallback(const Aero::Size& size) {
    const bool finiteW = std::isfinite(size.width);
    const bool finiteH = std::isfinite(size.height);
    if (finiteW && finiteH) {
        return size;
    }
    if (finiteW) {
        return Aero::Size(size.width, 720.0f);
    }
    if (finiteH) {
        return Aero::Size(1280.0f, size.height);
    }
    return Aero::Size(1280.0f, 720.0f);
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
    return GetValue<Aero::String>(SceneKeyProperty).Str();
}

void MdxSceneControl::SetSceneKey(const char* value) {
    SetValue<Aero::String>(SceneKeyProperty, value ? value : "");
}

const char* MdxSceneControl::GetModelPath() const {
    return GetValue<Aero::String>(ModelPathProperty).Str();
}

void MdxSceneControl::SetModelPath(const char* value) {
    SetValue<Aero::String>(ModelPathProperty, value ? value : "");
}

const char* MdxSceneControl::GetSequence() const {
    return GetValue<Aero::String>(SequenceProperty).Str();
}

void MdxSceneControl::SetSequence(const char* value) {
    SetValue<Aero::String>(SequenceProperty, value ? value : "");
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

Aero::Stretch MdxSceneControl::GetStretch() const {
    return GetValue<Aero::Stretch>(StretchProperty);
}

void MdxSceneControl::SetStretch(Aero::Stretch value) {
    SetValue<Aero::Stretch>(StretchProperty, value);
}

bool MdxSceneControl::GetIsActive() const {
    return GetValue<bool>(IsActiveProperty);
}

void MdxSceneControl::SetIsActive(bool value) {
    SetValue<bool>(IsActiveProperty, value);
}

void MdxSceneControl::OnRender(Aero::DrawingContext* context) {
    if (!GetIsActive() || context == nullptr) {
        return;
    }

    const Aero::Size renderSize((float)GetActualWidth(), (float)GetActualHeight());
    if (renderSize.width <= 0.0f || renderSize.height <= 0.0f) {
        return;
    }

    EnsureTextureSource(TextureExtent(renderSize.width), TextureExtent(renderSize.height));
    if (textureSource_ != nullptr) {
        context->DrawImage(textureSource_, ComputeImageRect(renderSize));
    }
}

Aero::Size MdxSceneControl::MeasureOverride(const Aero::Size& availableSize) {
    return FillOrFallback(availableSize);
}

Aero::Size MdxSceneControl::ArrangeOverride(const Aero::Size& finalSize) {
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
        textureSource_ = *new Aero::DynamicTextureSource((uint32_t)width, (uint32_t)height,
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

Aero::Rect MdxSceneControl::ComputeImageRect(const Aero::Size& arrangeSize) const {
    if (textureWidth_ <= 0 || textureHeight_ <= 0) {
        return Aero::Rect(arrangeSize);
    }

    const Aero::Size contentSize((float)textureWidth_, (float)textureHeight_);
    const Aero::Point scale = Aero::Viewbox::GetStretchScale(contentSize, arrangeSize,
        GetStretch(), Aero::StretchDirection_Both);
    const float width = contentSize.width * scale.x;
    const float height = contentSize.height * scale.y;
    const float x = (arrangeSize.width - width) * 0.5f;
    const float y = (arrangeSize.height - height) * 0.5f;
    return Aero::Rect(x, y, width, height);
}

Aero::Texture* MdxSceneControl::TextureRenderCallback(Aero::RenderDevice*, void* user) {
    MdxSceneControl* control = static_cast<MdxSceneControl*>(user);
    return control != nullptr ? control->renderer_.GetTexture() : nullptr;
}

void MdxSceneControl::OnScenePropertyChanged(Aero::DependencyObject* d,
    const Aero::DependencyPropertyChangedEventArgs&) {
    MdxSceneControl* control = Aero::DynamicCast<MdxSceneControl*>(d);
    if (control != nullptr) {
        control->MarkRendererDirty();
    }
}

NS_BEGIN_COLD_REGION

NS_IMPLEMENT_REFLECTION(MdxSceneControl, "War3UI.MdxSceneControl")
{
    Aero::UIElementData* data = NsMeta<Aero::UIElementData>(Aero::TypeOf<SelfClass>());
    const uint32_t affectsMeasureRender =
        Aero::FrameworkPropertyMetadataOptions_AffectsMeasure |
        Aero::FrameworkPropertyMetadataOptions_AffectsRender;

    data->RegisterProperty<Aero::String>(SceneKeyProperty, "SceneKey",
        Aero::FrameworkPropertyMetadata::Create(Aero::String(),
            affectsMeasureRender, Aero::PropertyChangedCallback(OnScenePropertyChanged)));
    data->RegisterProperty<Aero::String>(ModelPathProperty, "ModelPath",
        Aero::FrameworkPropertyMetadata::Create(Aero::String(),
            affectsMeasureRender, Aero::PropertyChangedCallback(OnScenePropertyChanged)));
    data->RegisterProperty<Aero::String>(SequenceProperty, "Sequence",
        Aero::FrameworkPropertyMetadata::Create(Aero::String("Stand"),
            affectsMeasureRender, Aero::PropertyChangedCallback(OnScenePropertyChanged)));
    data->RegisterProperty<bool>(AutoPlayProperty, "AutoPlay",
        Aero::FrameworkPropertyMetadata::Create(true,
            affectsMeasureRender, Aero::PropertyChangedCallback(OnScenePropertyChanged)));
    data->RegisterProperty<bool>(UseModelCameraProperty, "UseModelCamera",
        Aero::FrameworkPropertyMetadata::Create(true,
            affectsMeasureRender, Aero::PropertyChangedCallback(OnScenePropertyChanged)));
    data->RegisterProperty<Aero::Stretch>(StretchProperty, "Stretch",
        Aero::FrameworkPropertyMetadata::Create(Aero::Stretch_Uniform,
            affectsMeasureRender, Aero::PropertyChangedCallback(OnScenePropertyChanged)));
    data->RegisterProperty<bool>(IsActiveProperty, "IsActive",
        Aero::FrameworkPropertyMetadata::Create(true,
            affectsMeasureRender, Aero::PropertyChangedCallback(OnScenePropertyChanged)));
}

NS_END_COLD_REGION

const Aero::DependencyProperty* MdxSceneControl::SceneKeyProperty;
const Aero::DependencyProperty* MdxSceneControl::ModelPathProperty;
const Aero::DependencyProperty* MdxSceneControl::SequenceProperty;
const Aero::DependencyProperty* MdxSceneControl::AutoPlayProperty;
const Aero::DependencyProperty* MdxSceneControl::UseModelCameraProperty;
const Aero::DependencyProperty* MdxSceneControl::StretchProperty;
const Aero::DependencyProperty* MdxSceneControl::IsActiveProperty;

} // namespace War3UI
