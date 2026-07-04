#pragma once
#include "third_party/sokol/sokol_gfx.h"
#include "model/mdx.h"
#include "math.h"
#include <string>
#include <vector>
#include <algorithm>

struct MdxAnimNode {
    int parentIndex = -1;
    int objectId = -1;
    int pivotIndex = -1;
    uint32_t flags = 0;
    int boneIndex = -1;
    const MdxGeosetAnim* visibilityGeosetAnim = nullptr;
    uint32_t visibilityTimelineTag = 0;

    // Transform (computed each frame)
    Vec3 localTranslation = {0,0,0};
    Vec4 localRotation = {0,0,0,1};
    Vec3 localScale = {1,1,1};
    Mat4 worldMatrix = Mat4::Identity();
    Vec4 worldRotation = {0,0,0,1};
    Vec4 inverseWorldRotation = {0,0,0,1};
    Vec3 worldScale = {1,1,1};
    Vec3 inverseWorldScale = {1,1,1};
    float localVisibility = 1.0f;
    bool worldVisible = true;

    // Timelines from MdxModel (non-owning pointers, valid while model is alive)
    const std::vector<MdxTimeline>* timelines = nullptr;
};

struct SequenceState {
    int sequenceIndex = -1;
    uint32_t currentFrame = 0;
    float frameTime = 0;
    float speed = 1.0f;
    bool loop = true;
    bool finished = false;
};

struct MdxEventTrigger {
    int objectId = -1;
    std::string name;
    std::string type;
    std::string id;
    uint32_t flags = 0;
    int keyframeIndex = -1;
    uint32_t frame = 0;
    bool hasLocalTransform = false;
    Mat4 localTransform = Mat4::Identity();
    Vec3 localPosition = {0,0,0};
    Vec3 localScale = {1,1,1};
};

class MdxAnimator {
public:
    MdxAnimator();
    ~MdxAnimator();

    void Init(const MdxModel& model);
    void Update(float dt);
    void SetCameraForward(const Vec3& forward) { cameraForward_ = Normalize(forward); }

    bool SetSequence(int index, bool loop = true, float speed = 1.0f);
    int FindSequence(const std::string& name) const;
    bool PlaySequence(const std::string& name, bool loop = true, float speed = 1.0f);
    int GetCurrentSequence() const { return sequenceState_.sequenceIndex; }
    uint32_t GetCurrentFrame() const { return sequenceState_.currentFrame; }
    int GetSequenceCount() const { return sequences_ ? (int)sequences_->size() : 0; }
    float GetSequenceProgress() const;
    bool SetFrameByRatio(float ratio);
    bool IsSequenceFinished() const { return sequenceState_.finished; }
    bool IsSequenceLooping() const { return sequenceState_.loop; }
    int GetBoneIndexForObjectId(int objectId) const;
    bool IsObjectVisible(int objectId) const;
    bool GetObjectWorldTransformYUp(int objectId, Mat4& outMatrix, Vec3* outScale = nullptr) const;
    bool GetObjectWorldPositionYUp(int objectId, Vec3& outPosition) const;
    const std::vector<MdxEventTrigger>& GetTriggeredEvents() const { return triggeredEvents_; }

    // Bone matrices for skinning
    const std::vector<Mat4>& GetBoneMatrices() const { return boneMatrices_; }
    sg_image GetBoneTexture() const { return boneTexture_; }
    int GetBoneCount() const { return boneCount_; }
    bool HasAnimations() const { return boneCount_ > 0; }

private:
    void BuildSkeleton(const MdxModel& model);
    void BuildEventTracks(const MdxModel& model);
    void ResetEventEmitters();
    void UpdateEventEmitters();
    void SampleAnimations(uint32_t frame);
    void UpdateSkeleton();
    void UploadBoneTexture();

    static void ConvertMatrixToYUp(const Mat4& mdxMat, Mat4& out);

    std::vector<MdxAnimNode> nodes_;
    std::vector<int> nodeOrder_; // parent-first traversal order
    std::vector<Mat4> boneMatrices_;
    std::vector<std::pair<int, int>> objectIdToBoneIndex_;
    struct EventTrackState {
        int objectId = -1;
        std::string name;
        std::string type;
        std::string id;
        uint32_t flags = 0;
        const MdxTimeline* timeline = nullptr;
        int lastEmissionKey = -1;
    };
    std::vector<EventTrackState> eventTracks_;
    std::vector<MdxEventTrigger> triggeredEvents_;
    int boneCount_ = 0;

    // Model data references
    const std::vector<MdxSequence>* sequences_ = nullptr;
    const std::vector<uint32_t>* globalSequences_ = nullptr;
    const std::vector<std::array<float,3>>* pivotPoints_ = nullptr;
    float elapsedMs_ = 0.0f;

    SequenceState sequenceState_;
    sg_image boneTexture_ = {0};
    bool needsBoneUpload_ = false;
    Vec3 cameraForward_ = {0, 0, -1};
};
