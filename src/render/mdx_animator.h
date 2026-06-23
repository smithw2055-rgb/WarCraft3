#pragma once
#include "third_party/sokol/sokol_gfx.h"
#include "model/mdx.h"
#include "math.h"
#include <string>
#include <unordered_map>
#include <vector>
#include <algorithm>

struct MdxAnimNode {
    int parentIndex = -1;
    int objectId = -1;
    int pivotIndex = -1;
    uint32_t flags = 0;
    int boneIndex = -1;

    // Transform (computed each frame)
    Vec3 localTranslation = {0,0,0};
    Vec4 localRotation = {0,0,0,1};
    Vec3 localScale = {1,1,1};
    Mat4 worldMatrix = Mat4::Identity();
    Vec4 worldRotation = {0,0,0,1};
    Vec4 inverseWorldRotation = {0,0,0,1};
    Vec3 worldScale = {1,1,1};
    Vec3 inverseWorldScale = {1,1,1};

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
    bool IsSequenceFinished() const { return sequenceState_.finished; }
    int GetBoneIndexForObjectId(int objectId) const;

    // Bone matrices for skinning
    const std::vector<Mat4>& GetBoneMatrices() const { return boneMatrices_; }
    sg_image GetBoneTexture() const { return boneTexture_; }
    int GetBoneCount() const { return boneCount_; }
    bool HasAnimations() const { return boneCount_ > 0; }

private:
    void BuildSkeleton(const MdxModel& model);
    void SampleAnimations(uint32_t frame);
    void UpdateSkeleton();
    void UploadBoneTexture();

    static void ConvertMatrixToYUp(const Mat4& mdxMat, Mat4& out);

    std::vector<MdxAnimNode> nodes_;
    std::vector<int> nodeOrder_; // parent-first traversal order
    std::vector<Mat4> boneMatrices_;
    std::unordered_map<int, int> objectIdToBoneIndex_;
    int boneCount_ = 0;

    // Model data references
    const std::vector<MdxSequence>* sequences_ = nullptr;
    const std::vector<std::array<float,3>>* pivotPoints_ = nullptr;

    SequenceState sequenceState_;
    sg_image boneTexture_ = {0};
    bool needsBoneUpload_ = false;
    Vec3 cameraForward_ = {0, 0, -1};
};
