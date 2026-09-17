#pragma once

#include "model/mdx.h"
#include "render/math.h"
#include "render/picking.h"

class MdxAnimator;

bool MdxIntersectCollisionShapes(const MdxModel& model,
                                 const MdxAnimator& animator,
                                 const picking::PickRay& ray,
                                 const Mat4& instanceModelMatrix,
                                 float& outT);
