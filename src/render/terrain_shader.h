#pragma once
#include "third_party/sokol/sokol_gfx.h"

class TerrainShaders {
public:
    static sg_shader CreateGroundShader(int textureCount);
    static sg_shader CreateCliffShader(int cliffTextureCount);
    static sg_shader CreateWaterShader(int waterTextureCount);
    static sg_shader CreateUberSplatShader();
};
