#include "mdx_animator.h"
#include "renderer.h"
#include <iostream>
#include <cmath>
#include <cstring>
#include <map>

#define MAKE_TAG(A,B,C,D) ((A<<24)|(B<<16)|(C<<8)|D)
static const uint32_t TAG_KGTR = MAKE_TAG('K','G','T','R');
static const uint32_t TAG_KGRT = MAKE_TAG('K','G','R','T');
static const uint32_t TAG_KGSC = MAKE_TAG('K','G','S','C');
static const uint32_t NODE_DONT_INHERIT_TRANSLATION = 0x1;
static const uint32_t NODE_DONT_INHERIT_SCALING = 0x2;
static const uint32_t NODE_DONT_INHERIT_ROTATION = 0x4;
static const uint32_t NODE_BILLBOARDED = 0x8;
static const uint32_t NODE_BILLBOARDED_LOCK_Z = 0x40;

static Mat4 RotateY(float radians) {
    Mat4 r = Mat4::Identity();
    float c = std::cos(radians);
    float s = std::sin(radians);
    r.m[0] = c;
    r.m[2] = -s;
    r.m[8] = s;
    r.m[10] = c;
    return r;
}

static Vec4 NormalizeQuat(const Vec4& q) {
    float len = std::sqrt(q.x*q.x + q.y*q.y + q.z*q.z + q.w*q.w);
    if (len < 1e-7f) return {0,0,0,1};
    return {q.x / len, q.y / len, q.z / len, q.w / len};
}

static Vec4 MulQuat(const Vec4& a, const Vec4& b) {
    return NormalizeQuat({
        a.w*b.x + a.x*b.w + a.y*b.z - a.z*b.y,
        a.w*b.y - a.x*b.z + a.y*b.w + a.z*b.x,
        a.w*b.z + a.x*b.y - a.y*b.x + a.z*b.w,
        a.w*b.w - a.x*b.x - a.y*b.y - a.z*b.z
    });
}

static Vec4 InverseQuat(const Vec4& q) {
    Vec4 n = NormalizeQuat(q);
    return {-n.x, -n.y, -n.z, n.w};
}

static const MdxTimeline* findTimeline(const std::vector<MdxTimeline>& timelines, uint32_t tag) {
    for (const auto& tl : timelines)
        if (tl.tag == tag) return &tl;
    return nullptr;
}

MdxAnimator::MdxAnimator() = default;

int MdxAnimator::GetBoneIndexForObjectId(int objectId) const {
    auto it = objectIdToBoneIndex_.find(objectId);
    return it != objectIdToBoneIndex_.end() ? it->second : -1;
}

MdxAnimator::~MdxAnimator() {
    if (!Renderer::IsActive()) return;
    if (boneTexture_.id && sg_query_image_state(boneTexture_) == SG_RESOURCESTATE_VALID)
        sg_destroy_image(boneTexture_);
}

void MdxAnimator::ConvertMatrixToYUp(const Mat4& mdxMat, Mat4& out) {
    Mat4 basis = Mat4::Identity();
    basis.m[0] = 1.0f;  basis.m[1] = 0.0f;  basis.m[2] = 0.0f;  basis.m[3] = 0.0f;
    basis.m[4] = 0.0f;  basis.m[5] = 0.0f;  basis.m[6] = -1.0f; basis.m[7] = 0.0f;
    basis.m[8] = 0.0f;  basis.m[9] = 1.0f;  basis.m[10] = 0.0f; basis.m[11] = 0.0f;
    basis.m[12] = 0.0f; basis.m[13] = 0.0f; basis.m[14] = 0.0f; basis.m[15] = 1.0f;

    Mat4 basisInv = Mat4::Identity();
    basisInv.m[0] = 1.0f;  basisInv.m[1] = 0.0f; basisInv.m[2] = 0.0f;  basisInv.m[3] = 0.0f;
    basisInv.m[4] = 0.0f;  basisInv.m[5] = 0.0f; basisInv.m[6] = 1.0f;  basisInv.m[7] = 0.0f;
    basisInv.m[8] = 0.0f;  basisInv.m[9] = -1.0f; basisInv.m[10] = 0.0f; basisInv.m[11] = 0.0f;
    basisInv.m[12] = 0.0f; basisInv.m[13] = 0.0f; basisInv.m[14] = 0.0f; basisInv.m[15] = 1.0f;

    out = basis * mdxMat * basisInv;
}

void MdxAnimator::Init(const MdxModel& model) {
    sequences_ = &model.sequences;
    pivotPoints_ = &model.pivotPoints;
    BuildSkeleton(model);
    if (!nodes_.empty()) {
        SampleAnimations(0);
        UpdateSkeleton();
        needsBoneUpload_ = true;
        UploadBoneTexture();
    }
}

void MdxAnimator::BuildSkeleton(const MdxModel& model) {
    nodes_.clear();
    nodeOrder_.clear();
    objectIdToBoneIndex_.clear();
    boneCount_ = 0;
    int maxObjectId = -1;

    struct Entry {
        int objectId;
        int parentId;
        int pivotIndex;
        uint32_t flags;
        const std::vector<MdxTimeline>* timelines;
    };
    std::map<int, Entry> entries;
    auto addEntry = [&](int objectId, int parentId, uint32_t flags,
                        const std::vector<MdxTimeline>* timelines) {
        if (objectId < 0 || timelines == nullptr) {
            return;
        }
        if (entries.find(objectId) != entries.end()) {
            return;
        }
        int pivotIndex = -1;
        if (objectId < (int)model.pivotPoints.size()) {
            pivotIndex = objectId;
        }
        entries[objectId] = {objectId, parentId, pivotIndex, flags, timelines};
        maxObjectId = std::max(maxObjectId, objectId);
    };

    // Bones
    for (const auto& b : model.bones) {
        addEntry(b.objectId, b.parentId, b.flags, &b.timelines);
    }
    // Helpers
    for (const auto& h : model.helpers) {
        addEntry(h.objectId, h.parentId, h.flags, &h.timelines);
    }
    // Other transform-bearing nodes can still drive skinned geosets even when
    // they are not declared as bones/helpers in the MDX.
    for (const auto& attachment : model.attachments) {
        addEntry(attachment.objectId, attachment.parentId, attachment.flags, &attachment.timelines);
    }
    for (const auto& light : model.lights) {
        addEntry(light.objectId, light.parentId, light.flags, &light.timelines);
    }
    for (const auto& emitter : model.particleEmitters1) {
        addEntry(emitter.objectId, emitter.parentId, emitter.flags, &emitter.timelines);
    }
    for (const auto& emitter : model.particleEmitters2) {
        addEntry(emitter.objectId, emitter.parentId, (uint32_t)emitter.flags, &emitter.timelines);
    }
    for (const auto& ribbon : model.ribbonEmitters) {
        addEntry(ribbon.objectId, ribbon.parentId, ribbon.flags, &ribbon.timelines);
    }
    for (const auto& eventObject : model.eventObjects) {
        addEntry(eventObject.objectId, eventObject.parentId, eventObject.flags, &eventObject.timelines);
    }

    if (entries.empty()) return;

    // Topological sort: parents before children
    std::map<int,int> objIdToNodeIdx;
    std::vector<int> sorted;

    // Roots first
    for (const auto& [oid,e] : entries) {
        if (e.parentId < 0) {
            objIdToNodeIdx[oid] = (int)nodes_.size();
            sorted.push_back((int)nodes_.size());
            nodes_.resize(nodes_.size() + 1);
        }
    }
    if (sorted.empty()) {
        int oid = entries.begin()->first;
        objIdToNodeIdx[oid] = 0;
        sorted.push_back(0);
        nodes_.resize(1);
    }

    // BFS children
    size_t processed = 0;
    while (processed < sorted.size()) {
        int curIdx = sorted[processed];
        int curOid = -1;
        for (const auto& [oid,idx] : objIdToNodeIdx)
            if (idx == curIdx) { curOid = oid; break; }

        if (curOid >= 0) {
            for (const auto& [oid,e] : entries) {
                if (e.parentId == curOid && objIdToNodeIdx.find(oid) == objIdToNodeIdx.end()) {
                    objIdToNodeIdx[oid] = (int)nodes_.size();
                    sorted.push_back((int)nodes_.size());
                    nodes_.resize(nodes_.size() + 1);
                }
            }
        }
        processed++;
    }

    // Remaining unconnected
    for (const auto& [oid,e] : entries) {
        if (objIdToNodeIdx.find(oid) == objIdToNodeIdx.end()) {
            objIdToNodeIdx[oid] = (int)nodes_.size();
            sorted.push_back((int)nodes_.size());
            nodes_.resize(nodes_.size() + 1);
        }
    }

    // Initialize nodes
    for (const auto& [oid,e] : entries) {
        int idx = objIdToNodeIdx[oid];
        auto& n = nodes_[idx];
        n.objectId = e.objectId;
        n.parentIndex = (e.parentId >= 0 && objIdToNodeIdx.count(e.parentId))
            ? objIdToNodeIdx[e.parentId] : -1;
        n.pivotIndex = e.pivotIndex;
        n.flags = e.flags;
        n.timelines = e.timelines;
        n.boneIndex = n.objectId;
        objectIdToBoneIndex_[n.objectId] = n.boneIndex;

    }

    nodeOrder_ = sorted;
    boneCount_ = maxObjectId + 1;
    boneMatrices_.resize(boneCount_, Mat4::Identity());

    std::cout << "MdxAnimator: " << nodes_.size() << " nodes, " << boneCount_ << " bones" << std::endl;
}

bool MdxAnimator::SetSequence(int index, bool loop, float speed) {
    if (!sequences_ || index < 0 || index >= (int)sequences_->size()) return false;
    const auto& seq = (*sequences_)[index];
    sequenceState_.sequenceIndex = index;
    sequenceState_.currentFrame = seq.startFrame;
    sequenceState_.frameTime = 0;
    sequenceState_.speed = speed;
    sequenceState_.loop = loop;
    sequenceState_.finished = false;
    return true;
}

int MdxAnimator::FindSequence(const std::string& name) const {
    if (!sequences_) return -1;
    for (int i = 0; i < (int)sequences_->size(); i++) {
        if ((*sequences_)[i].name == name) return i;
    }
    return -1;
}

bool MdxAnimator::PlaySequence(const std::string& name, bool loop, float speed) {
    int idx = FindSequence(name);
    if (idx < 0) return false;
    return SetSequence(idx, loop, speed);
}

float MdxAnimator::GetSequenceProgress() const {
    if (sequenceState_.sequenceIndex < 0 || !sequences_) return 0;
    const auto& seq = (*sequences_)[sequenceState_.sequenceIndex];
    uint32_t range = seq.endFrame - seq.startFrame;
    if (range == 0) return 1.0f;
    return (float)(sequenceState_.currentFrame - seq.startFrame) / (float)range;
}

void MdxAnimator::Update(float dt) {
    if (sequenceState_.sequenceIndex < 0) {
        return;
    }
    if (!sequences_ || sequenceState_.sequenceIndex >= (int)sequences_->size()) return;

    const auto& seq = (*sequences_)[sequenceState_.sequenceIndex];
    uint32_t start = seq.startFrame;
    uint32_t end = seq.endFrame;
    uint32_t range = end - start;
    if (range == 0) return;

    float advance = dt * 1000.0f * sequenceState_.speed;
    sequenceState_.frameTime += advance;
    int intAdv = (int)sequenceState_.frameTime;
    sequenceState_.frameTime -= (float)intAdv;
    sequenceState_.currentFrame += intAdv;

    if (sequenceState_.currentFrame >= end) {
        if (sequenceState_.loop) {
            uint32_t over = sequenceState_.currentFrame - end;
            sequenceState_.currentFrame = start + (over % range);
        } else {
            sequenceState_.currentFrame = end;
            sequenceState_.finished = true;
        }
    }

    if (!nodes_.empty()) {
        SampleAnimations(sequenceState_.currentFrame);
        UpdateSkeleton();
        needsBoneUpload_ = true;
    }
}

void MdxAnimator::SampleAnimations(uint32_t frame) {
    for (auto& node : nodes_) {
        if (!node.timelines || node.timelines->empty()) continue;

        float tmp[4];
        const MdxTimeline* tl;

        tl = findTimeline(*node.timelines, TAG_KGTR);
        if (tl && !tl->frames.empty() && MdxSampleTimeline(*tl, frame, tmp, 3))
            node.localTranslation = {tmp[0], tmp[1], tmp[2]};

        tl = findTimeline(*node.timelines, TAG_KGRT);
        if (tl && !tl->frames.empty() && MdxSampleTimeline(*tl, frame, tmp, 4))
            node.localRotation = {tmp[0], tmp[1], tmp[2], tmp[3]};

        tl = findTimeline(*node.timelines, TAG_KGSC);
        if (tl && !tl->frames.empty() && MdxSampleTimeline(*tl, frame, tmp, 3))
            node.localScale = {tmp[0], tmp[1], tmp[2]};
    }
}

void MdxAnimator::UpdateSkeleton() {
    for (int idx : nodeOrder_) {
        auto& node = nodes_[idx];
        const MdxAnimNode* parent = (node.parentIndex >= 0) ? &nodes_[node.parentIndex] : nullptr;

        Vec3 computedScale = node.localScale;
        if ((node.flags & NODE_DONT_INHERIT_SCALING) && parent) {
            computedScale.x = parent->inverseWorldScale.x * node.localScale.x;
            computedScale.y = parent->inverseWorldScale.y * node.localScale.y;
            computedScale.z = parent->inverseWorldScale.z * node.localScale.z;
            node.worldScale = node.localScale;
        } else if (parent) {
            node.worldScale.x = parent->worldScale.x * node.localScale.x;
            node.worldScale.y = parent->worldScale.y * node.localScale.y;
            node.worldScale.z = parent->worldScale.z * node.localScale.z;
        } else {
            node.worldScale = node.localScale;
        }

        Vec4 computedRotation = node.localRotation;
        if ((node.flags & NODE_DONT_INHERIT_ROTATION) && parent) {
            computedRotation = MulQuat(parent->inverseWorldRotation, computedRotation);
        }

        // Build local matrix in MDX Z-up space
        // local = translate(location + pivot) * rotate * scale * translate(-pivot)
        Mat4 local = Mat4::Identity();
        Vec3 pivot = {0, 0, 0};
        bool hasPivot = false;

        // -pivot
        if (node.pivotIndex >= 0 && node.pivotIndex < (int)pivotPoints_->size()) {
            const auto& piv = (*pivotPoints_)[node.pivotIndex];
            pivot = {piv[0], piv[1], piv[2]};
            hasPivot = true;
            local = Mat4::Translate(-pivot.x, -pivot.y, -pivot.z);
        }

        // S * R * T (applied right-to-left: first -pivot, then scale, then rotate, then translate)
        local = Mat4::Scale(computedScale.x, computedScale.y, computedScale.z) * local;
        local = Mat4::FromQuaternion(computedRotation) * local;
        Vec3 finalTranslation = node.localTranslation;
        if (hasPivot) {
            finalTranslation = finalTranslation + pivot;
        }
        if ((node.flags & NODE_DONT_INHERIT_TRANSLATION) && parent) {
            finalTranslation = finalTranslation - Vec3{
                parent->worldMatrix.m[12],
                parent->worldMatrix.m[13],
                parent->worldMatrix.m[14]
            };
        }
        local = Mat4::Translate(finalTranslation.x, finalTranslation.y, finalTranslation.z) * local;

        // Parent
        if (node.parentIndex >= 0)
            local = nodes_[node.parentIndex].worldMatrix * local;

        node.worldMatrix = local;
        node.worldRotation = parent ? MulQuat(parent->worldRotation, computedRotation) : NormalizeQuat(computedRotation);
        node.inverseWorldRotation = InverseQuat(node.worldRotation);
        node.inverseWorldScale = {
            std::fabs(node.worldScale.x) > 1e-7f ? 1.0f / node.worldScale.x : 0.0f,
            std::fabs(node.worldScale.y) > 1e-7f ? 1.0f / node.worldScale.y : 0.0f,
            std::fabs(node.worldScale.z) > 1e-7f ? 1.0f / node.worldScale.z : 0.0f
        };

        if (node.boneIndex >= 0 && node.boneIndex < boneCount_) {
            Mat4 yup;
            ConvertMatrixToYUp(local, yup);
            if (node.flags & (NODE_BILLBOARDED | NODE_BILLBOARDED_LOCK_Z)) {
                Vec3 cam = cameraForward_;
                cam.y = 0.0f;
                float camLen2 = Dot(cam, cam);
                if (camLen2 > 1e-6f) {
                    cam = Normalize(cam);
                    float yaw = std::atan2(-cam.x, -cam.z);
                    Vec3 pos = {yup.m[12], yup.m[13], yup.m[14]};
                    Mat4 billboard = Mat4::Translate(pos.x, pos.y, pos.z)
                                   * RotateY(yaw)
                                   * Mat4::Translate(-pos.x, -pos.y, -pos.z);
                    yup = billboard * yup;
                }
            }
            boneMatrices_[node.boneIndex] = yup;
        }
    }
    UploadBoneTexture();
}

void MdxAnimator::UploadBoneTexture() {
    if (!needsBoneUpload_) return;
    needsBoneUpload_ = false;
    if (boneMatrices_.empty()) return;

    int texWidth = boneCount_ * 4;
    std::vector<float> texData((size_t)texWidth * 4, 0.0f);

    for (int i = 0; i < boneCount_; i++) {
        const float* m = boneMatrices_[i].m;
        // 4 columns of vec4
        for (int j = 0; j < 4; j++) {
            texData[(size_t)i * 16 + j * 4 + 0] = m[j * 4 + 0];
            texData[(size_t)i * 16 + j * 4 + 1] = m[j * 4 + 1];
            texData[(size_t)i * 16 + j * 4 + 2] = m[j * 4 + 2];
            texData[(size_t)i * 16 + j * 4 + 3] = m[j * 4 + 3];
        }
    }

    if (sg_query_image_state(boneTexture_) == SG_RESOURCESTATE_VALID)
        sg_destroy_image(boneTexture_);

    sg_image_desc desc = {};
    desc.type = SG_IMAGETYPE_2D;
    desc.width = texWidth;
    desc.height = 1;
    desc.pixel_format = SG_PIXELFORMAT_RGBA32F;
    desc.num_mipmaps = 1;
    desc.data.mip_levels[0].ptr = texData.data();
    desc.data.mip_levels[0].size = (size_t)texWidth * 4 * sizeof(float);
    desc.usage.immutable = true;
    boneTexture_ = sg_make_image(&desc);

    if (sg_query_image_state(boneTexture_) != SG_RESOURCESTATE_VALID) {
        std::cerr << "MdxAnimator: bone texture upload failed" << std::endl;
        boneTexture_ = {0};
    }
}
