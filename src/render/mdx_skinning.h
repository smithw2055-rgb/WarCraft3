#pragma once

#include "model/mdx.h"

#include <cstddef>
#include <cstdint>
#include <functional>
#include <vector>

struct MdxSkinnedVertex {
    float px, py, pz;
    float nx, ny, nz;
    float tu, tv;
    uint8_t boneIndices[8];
    float boneWeights[8];
    float boneCount;
};

static_assert(offsetof(MdxSkinnedVertex, px) == 0, "Unexpected skinned vertex position offset");
static_assert(offsetof(MdxSkinnedVertex, nx) == 12, "Unexpected skinned vertex normal offset");
static_assert(offsetof(MdxSkinnedVertex, tu) == 24, "Unexpected skinned vertex uv offset");
static_assert(offsetof(MdxSkinnedVertex, boneIndices) == 32, "Unexpected skinned vertex bone index offset");
static_assert(offsetof(MdxSkinnedVertex, boneWeights) == 40, "Unexpected skinned vertex bone weight offset");
static_assert(offsetof(MdxSkinnedVertex, boneCount) == 72, "Unexpected skinned vertex bone count offset");
static_assert(sizeof(MdxSkinnedVertex) == 76, "Unexpected skinned vertex stride");

struct MdxSkinningBuildStats {
    int skinnedGeosets = 0;
    int vertices = 0;
    int verticesWithInfluence = 0;
    int invalidGroupReferences = 0;
    int invalidBoneReferences = 0;
    int badWeightSums = 0;
};

using MdxBoneIndexResolver = std::function<int(int objectId)>;

bool MdxBuildSkinnedVertices(const MdxGeoset& geo,
                             const std::vector<float>* uvSource,
                             const MdxBoneIndexResolver& resolveBoneIndex,
                             std::vector<MdxSkinnedVertex>& outVertices,
                             MdxSkinningBuildStats* stats = nullptr);
