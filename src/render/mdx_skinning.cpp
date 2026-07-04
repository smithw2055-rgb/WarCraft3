#include "mdx_skinning.h"

#include <cmath>
#include <cstring>

bool MdxBuildSkinnedVertices(const MdxGeoset& geo,
                             const std::vector<float>* uvSource,
                             const MdxBoneIndexResolver& resolveBoneIndex,
                             std::vector<MdxSkinnedVertex>& outVertices,
                             MdxSkinningBuildStats* stats) {
    const int vertexCount = (int)geo.vertices.size() / 3;
    outVertices.clear();
    if (vertexCount <= 0 || !resolveBoneIndex) {
        return false;
    }

    const bool hasCompactSkin = geo.skin.size() >= (size_t)vertexCount * 4;
    const bool hasClassicSkin = !geo.vertexGroups.empty() && !geo.matrixGroups.empty();
    if (!hasCompactSkin && !hasClassicSkin) {
        return false;
    }

    if (stats) {
        stats->skinnedGeosets += 1;
        stats->vertices += vertexCount;
    }

    outVertices.resize(vertexCount);

    for (int i = 0; i < vertexCount; i++) {
        MdxSkinnedVertex& vertex = outVertices[(size_t)i];

        const float mx = geo.vertices[(size_t)i * 3];
        const float my = geo.vertices[(size_t)i * 3 + 1];
        const float mz = geo.vertices[(size_t)i * 3 + 2];
        vertex.px = mx;
        vertex.py = mz;
        vertex.pz = -my;

        if (!geo.normals.empty() && geo.normals.size() >= (size_t)i * 3 + 3) {
            vertex.nx = geo.normals[(size_t)i * 3];
            vertex.ny = geo.normals[(size_t)i * 3 + 2];
            vertex.nz = -geo.normals[(size_t)i * 3 + 1];
        } else {
            vertex.nx = 0.0f;
            vertex.ny = 1.0f;
            vertex.nz = 0.0f;
        }

        if (uvSource && uvSource->size() >= (size_t)i * 2 + 2) {
            vertex.tu = (*uvSource)[(size_t)i * 2];
            vertex.tv = (*uvSource)[(size_t)i * 2 + 1];
        } else {
            vertex.tu = 0.0f;
            vertex.tv = 0.0f;
        }

        std::memset(vertex.boneIndices, 0, sizeof(vertex.boneIndices));
        std::memset(vertex.boneWeights, 0, sizeof(vertex.boneWeights));
        vertex.boneWeights[0] = 1.0f;
        vertex.boneCount = 1.0f;

        int influenceCount = 0;
        if (hasCompactSkin) {
            float totalWeight = 0.0f;
            for (int b = 0; b < 4; ++b) {
                const uint16_t skinEntry = geo.skin[(size_t)i * 4 + b];
                const int objectId = (int)(skinEntry >> 8);
                const float weight = float(skinEntry & 0xFF) / 255.0f;
                if (weight <= 0.0f) {
                    continue;
                }

                const int boneIndex = resolveBoneIndex(objectId);
                if (boneIndex < 0 || boneIndex >= 256) {
                    if (stats) ++stats->invalidBoneReferences;
                    continue;
                }

                vertex.boneIndices[influenceCount] = (uint8_t)boneIndex;
                vertex.boneWeights[influenceCount] = weight;
                totalWeight += weight;
                ++influenceCount;
            }
            if (influenceCount > 0 && totalWeight > 0.0f) {
                for (int b = 0; b < influenceCount; ++b) {
                    vertex.boneWeights[b] /= totalWeight;
                }
            }
        } else {
            if (i >= (int)geo.vertexGroups.size()) {
                if (stats) ++stats->invalidGroupReferences;
                continue;
            }

            const int groupIdx = geo.vertexGroups[(size_t)i];
            if (groupIdx < 0 || groupIdx >= (int)geo.matrixGroups.size()) {
                if (stats) ++stats->invalidGroupReferences;
                continue;
            }

            int groupStart = 0;
            for (int mg = 0; mg < groupIdx; mg++) {
                groupStart += (int)geo.matrixGroups[(size_t)mg];
            }

            const int groupBoneCount = (int)geo.matrixGroups[(size_t)groupIdx];
            for (int b = 0; b < groupBoneCount && influenceCount < 8; ++b) {
                const int matrixIndex = groupStart + b;
                if (matrixIndex < 0 || matrixIndex >= (int)geo.matrixIndices.size()) {
                    if (stats) ++stats->invalidGroupReferences;
                    continue;
                }

                const int objectId = (int)geo.matrixIndices[(size_t)matrixIndex];
                const int boneIndex = resolveBoneIndex(objectId);
                if (boneIndex >= 0 && boneIndex < 256) {
                    vertex.boneIndices[influenceCount] = (uint8_t)boneIndex;
                    ++influenceCount;
                } else if (stats) {
                    ++stats->invalidBoneReferences;
                }
            }
            if (influenceCount > 0) {
                const float weight = 1.0f / (float)influenceCount;
                for (int b = 0; b < influenceCount; ++b) {
                    vertex.boneWeights[b] = weight;
                }
            }
        }

        if (influenceCount > 0) {
            vertex.boneCount = (float)influenceCount;
            if (stats) {
                ++stats->verticesWithInfluence;
                float weightSum = 0.0f;
                for (int b = 0; b < influenceCount; ++b) {
                    weightSum += vertex.boneWeights[b];
                }
                if (std::fabs(weightSum - 1.0f) > 0.01f) {
                    ++stats->badWeightSums;
                }
            }
        }
    }

    return true;
}
