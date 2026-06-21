#include "terrain_shader.h"
#include "shaders_common.h"
#include <sstream>
#include <cstring>
#include <vector>

static std::string SamplerDeclarations(int count) {
    std::string result;
    for (int i = 0; i < count; ++i)
        result += "uniform sampler2DArray sample" + std::to_string(i) + ";\n";
    return result;
}

static std::string BuildGroundVertex() {
    using namespace ShaderUtils; (void)LightSystem; (void)FogSystem;
    std::ostringstream ss;
    ss << "#version 410 core\n"
       << "in vec2 vPosition;\n"
       << "uniform mat4 MVP;\n"
       << "uniform mat4 DepthBiasMVP;\n"
       << "uniform sampler2D height_texture;\n"
       << "uniform sampler2D height_cliff_texture;\n"
       << "uniform usampler2D terrain_texture_list;\n"
       << "uniform float centerOffsetX;\n"
       << "uniform float centerOffsetY;\n"
       << "uniform sampler2D lightTexture;\n"
       << "uniform float lightCount;\n"
       << "uniform float lightTextureHeight;\n"
       << "out vec2 UV;\nflat out uvec4 texture_indices;\nout vec2 pathing_map_uv;\nout vec3 position;\nout vec3 ShadowCoord;\nout vec2 v_suv;\nout vec3 shadeColor;\n"
       << "void main() {\n"
       << "    ivec2 size = textureSize(terrain_texture_list, 0);\n"
       << "    ivec2 pos = ivec2(gl_InstanceID % size.x, gl_InstanceID / size.x);\n"
       << "    ivec2 height_pos = ivec2(vPosition + pos);\n"
       << "    vec4 height = texelFetch(height_cliff_texture, height_pos, 0);\n"
       << "    ivec3 off = ivec3(1, 1, 0);\n"
       << "    float hL = texelFetch(height_texture, height_pos - off.xz, 0).r;\n"
       << "    float hR = texelFetch(height_texture, height_pos + off.xz, 0).r;\n"
       << "    float hD = texelFetch(height_texture, height_pos - off.zy, 0).r;\n"
       << "    float hU = texelFetch(height_texture, height_pos + off.zy, 0).r;\n"
       << "    vec3 normal = normalize(vec3(hL - hR, 2.0, hD - hU));\n"
       << "    UV = vec2(vPosition.x, 1 - vPosition.y);\n"
       << "    texture_indices = texelFetch(terrain_texture_list, pos, 0);\n"
       << "    pathing_map_uv = (vPosition + pos) * 4;\n"
       << "    vec3 positionWorld = vec3((vPosition.x + pos.x) * 128.0 + centerOffsetX, height.r * 128.0, (vPosition.y + pos.y) * 128.0 + centerOffsetY);\n"
       << "    position = positionWorld;\n"
       << "    gl_Position = ((texture_indices.a & 32768u) == 0u) ? MVP * vec4(position.xyz, 1) : vec4(2.0, 0.0, 0.0, 1.0);\n"
       << "    ShadowCoord = (((texture_indices.a & 32768u) == 0u) ? DepthBiasMVP * vec4(position.xyz, 1) : vec4(2.0, 0.0, 0.0, 1.0)).xyz;\n"
       << "    v_suv = (vPosition + pos) / size;\n"
       << "    position.x = (position.x - centerOffsetX) / (size.x * 128.0);\n"
       << "    position.y = (position.y - centerOffsetY) / (size.y * 128.0);\n"
       << ShaderUtils::LightSystem("normal", "positionWorld", "lightTexture", "lightTextureHeight", "lightCount", true)
       << "    shadeColor = clamp(lightFactor, 0.0, 1.0);\n}";
    return ss.str();
}

static std::string BuildGroundFragment(int textureCount) {
    std::ostringstream ss;
    ss << "#version 410 core\n"
       << "uniform bool show_pathing_map;\nuniform bool show_lighting;\nuniform bool base_texture_only;\n"
       << SamplerDeclarations(textureCount)
       << "uniform sampler2D shadowMap;\nuniform sampler2D fogOfWarMap;\n"
       << "uniform vec4 u_fogColor;\nuniform vec4 u_fogParams;\n"
       << "in vec2 UV;\nflat in uvec4 texture_indices;\nin vec2 pathing_map_uv;\nin vec3 position;\nin vec3 ShadowCoord;\nin vec2 v_suv;\nin vec3 shadeColor;\nout vec4 color;\n"
       << ShaderUtils::GetFragmentChain(textureCount)
       << "void main() {\n"
       << "    color = get_fragment(texture_indices.a & 31u, vec3(UV, texture_indices.a >> 5));\n"
       << "    color = color * color.a + get_fragment(texture_indices.b & 31u, vec3(UV, texture_indices.b >> 5)) * (1.0 - color.a);\n"
       << "    color = color * color.a + get_fragment(texture_indices.g & 31u, vec3(UV, texture_indices.g >> 5)) * (1.0 - color.a);\n"
       << "    color = color * color.a + get_fragment(texture_indices.r & 31u, vec3(UV, texture_indices.r >> 5)) * (1.0 - color.a);\n"
       << "\n"
       << "    // Edge darkening: subtle seam at tile boundaries\n"
       << "    vec2 dUV = fwidth(UV);\n"
       << "    float edgeDist = min(dUV.x, dUV.y) * 100.0;\n"
       << "    float edgeFactor = 1.0 - 0.12 * exp(-edgeDist * edgeDist);\n"
       << "    color.rgb *= edgeFactor;\n"
       << "\n"
       << "    if (base_texture_only) { color = vec4(color.rgb, 1.0); return; }\n"
       << "    float shadow = texture(shadowMap, v_suv).r;\n"
       << "    float fogOfWarData = texture(fogOfWarMap, v_suv).r;\n"
       << "    shadow = clamp(shadow + fogOfWarData, 0.0, 1.0);\n"
       << "    if (show_lighting) color = vec4(color.xyz * (1.0 - shadow) * shadeColor, 1.0);\n"
       << "    else color = vec4(color.xyz * (1.0 - shadow), 1.0);\n"
       << ShaderUtils::FogSystem(false, nullptr) << "}";
    return ss.str();
}

static std::string BuildCliffVertex() {
    std::ostringstream ss;
    ss << "#version 410 core\n"
       << "in vec3 vPosition;\nin vec2 vUV;\nin vec3 vNormal;\n"
       << "uniform mat4 MVP;\nuniform float centerOffsetX;\nuniform float centerOffsetY;\n"
       << "uniform vec4 cliffOffset;\n"
       << "uniform sampler2D height_texture;\nuniform sampler2D shadowMap;\nuniform sampler2D lightTexture;\n"
       << "uniform float lightCount;\nuniform float lightTextureHeight;\n"
       << "out vec3 UV;\nout vec3 Normal;\nout vec2 pathing_map_uv;\nout vec3 position;\nout vec2 v_suv;\nout vec3 shadeColor;\n"
       << "void main() {\n"
       << "    pathing_map_uv = (vec2(vPosition.y, -vPosition.x) / 128.0 + cliffOffset.xy) * 4.0;\n"
       << "    ivec2 size = textureSize(height_texture, 0);\n"
       << "    ivec2 shadowSize = textureSize(shadowMap, 0);\n"
       << "    v_suv = pathing_map_uv / vec2(shadowSize);\n"
       << "    float value = texture(height_texture, (cliffOffset.xy + vec2(vPosition.y + 64.0, -vPosition.x + 64.0) / 128.0) / vec2(size)).r;\n"
       << "    vec3 positionWorld = vec3(vPosition.y + cliffOffset.x * 128.0 + centerOffsetX,\n"
       << "                              vPosition.z + (cliffOffset.z + value) * 128.0,\n"
       << "                              -vPosition.x + cliffOffset.y * 128.0 + centerOffsetY);\n"
       << "    gl_Position = MVP * vec4(positionWorld, 1.0);\n"
       << "    UV = vec3(vUV, cliffOffset.w);\n"
       << "    ivec2 height_pos = ivec2(cliffOffset.xy + vec2(vPosition.y, -vPosition.x) / 128.0);\n"
       << "    ivec3 off = ivec3(1, 1, 0);\n"
       << "    float hL = texelFetch(height_texture, height_pos - off.xz, 0).r;\n"
       << "    float hR = texelFetch(height_texture, height_pos + off.xz, 0).r;\n"
       << "    float hD = texelFetch(height_texture, height_pos - off.zy, 0).r;\n"
       << "    float hU = texelFetch(height_texture, height_pos + off.zy, 0).r;\n"
       << "    bool edgeX = (vPosition.y) == float((int(vPosition.y)) / 128 * 128);\n"
       << "    bool edgeY = (vPosition.x) == float((int(vPosition.x)) / 128 * 128);\n"
       << "    bool edgeZ = (vPosition.z) == float((int(vPosition.z)) / 128 * 128);\n"
       << "    vec3 terrainNormal = vec3(vNormal.y, vNormal.z, -vNormal.x);\n"
       << "    if (edgeX) terrainNormal.x = hL - hR;\n"
       << "    if (edgeY) terrainNormal.z = hD - hU;\n"
       << "    if (edgeZ) terrainNormal.y = 2.0;\n"
       << "    Normal = normalize(terrainNormal);\n"
       << "    position = vec3((positionWorld.x - centerOffsetX) / (size.x * 128.0),\n"
       << "                    (positionWorld.z - centerOffsetY) / (size.y * 128.0),\n"
       << "                    positionWorld.y);\n"
       << ShaderUtils::LightSystem("Normal", "positionWorld", "lightTexture", "lightTextureHeight", "lightCount", true)
       << "    shadeColor = clamp(lightFactor, 0.0, 1.0);\n}";
    return ss.str();
}

static std::string BuildCliffFragment() {
    std::ostringstream ss;
    ss << "#version 410 core\n"
       << "uniform sampler2DArray cliff_textures;\nuniform sampler2D shadowMap;\nuniform sampler2D fogOfWarMap;\n"
       << "uniform bool show_lighting;\nuniform bool base_texture_only;\n"
       << "uniform vec4 u_fogColor;\nuniform vec4 u_fogParams;\n"
       << "in vec3 UV;\nin vec3 Normal;\nin vec2 pathing_map_uv;\nin vec3 position;\nin vec2 v_suv;\nin vec3 shadeColor;\nout vec4 color;\n"
       << "void main() {\n"
       << "    color = texture(cliff_textures, UV);\n"
       << "    if (base_texture_only) return;\n"
       << "    float shadow = texture(shadowMap, v_suv).r;\n"
       << "    float fogOfWarData = texture(fogOfWarMap, v_suv).r;\n"
       << "    shadow = clamp(shadow + fogOfWarData, 0.0, 1.0);\n"
       << "    color.rgb *= (1.0 - shadow);\n"
       << "    if (show_lighting) color.rgb *= shadeColor;\n"
       << ShaderUtils::FogSystem(false, nullptr) << "}";
    return ss.str();
}

static std::string BuildWaterVertex() {
    std::ostringstream ss;
    ss << "#version 410 core\n"
       << "in vec2 vPosition;\n"
       << "uniform sampler2D water_height_texture;\nuniform sampler2D ground_height_texture;\nuniform sampler2D water_exists_texture;\n"
       << "uniform float centerOffsetX;\nuniform float centerOffsetY;\nuniform mat4 MVP;\n"
       << "uniform vec4 shallow_color_min;\nuniform vec4 shallow_color_max;\nuniform vec4 deep_color_min;\nuniform vec4 deep_color_max;\n"
       << "uniform float water_offset;\nuniform sampler2D lightTexture;\nuniform float lightCount;\nuniform float lightTextureHeight;\n"
       << "out vec2 UV;\nout vec4 vertexColor;\nout vec2 position;\nout vec3 shadeColor;\nout vec2 v_suv;\nflat out float waterCoverage;\n"
       << "const float min_depth = 10.f / 128;\nconst float deeplevel = 64.f / 128;\nconst float maxdepth = 72.f / 128;\n"
       << "void main() {\n"
       << "    ivec2 size = textureSize(water_height_texture, 0) - 1;\n"
       << "    ivec2 pos = ivec2(gl_InstanceID % size.x, gl_InstanceID / size.x);\n"
       << "    ivec2 height_pos = ivec2(vPosition + pos);\n"
       << "    float water_height = texelFetch(water_height_texture, height_pos, 0).r + water_offset;\n"
       << "    float we0 = float(texelFetch(water_exists_texture, pos, 0).r);\n"
       << "    float we1 = float(texelFetch(water_exists_texture, pos + ivec2(1, 0), 0).r);\n"
       << "    float we2 = float(texelFetch(water_exists_texture, pos + ivec2(0, 1), 0).r);\n"
       << "    float we3 = float(texelFetch(water_exists_texture, pos + ivec2(1, 1), 0).r);\n"
       << "    bool is_water = we0 > 0.0 || we1 > 0.0 || we2 > 0.0 || we3 > 0.0;\n"
       << "    waterCoverage = (we0 + we1 + we2 + we3) / 4.0;\n"
       << "    position = vec2((vPosition.x + pos.x) * 128.0 + centerOffsetX, (vPosition.y + pos.y) * 128.0 + centerOffsetY);\n"
       << "    vec4 myposition = vec4(position.x, water_height * 128.0, position.y, 1);\n"
       << "    vec3 Normal = vec3(0, 1, 0);\n"
       << "    gl_Position = is_water ? MVP * myposition : vec4(2.0, 0.0, 0.0, 1.0);\n"
       << "    UV = vec2((vPosition.x + pos.x % 2) / 2.0, (vPosition.y + pos.y % 2) / 2.0);\n"
       << "    float ground_height = texelFetch(ground_height_texture, height_pos, 0).r;\n"
       << "    float value = clamp(water_height - ground_height, 0.f, 1.f);\n"
       << "    if (value <= deeplevel) { value = max(0.f, value - min_depth) / (deeplevel - min_depth);\n"
       << "        vertexColor = shallow_color_min * (1.f - value) + shallow_color_max * value; }\n"
       << "    else { value = clamp(value - deeplevel, 0.f, maxdepth - deeplevel) / (maxdepth - deeplevel);\n"
       << "        vertexColor = deep_color_min * (1.f - value) + deep_color_max * value; }\n"
       << ShaderUtils::LightSystem("Normal", "myposition.xyz", "lightTexture", "lightTextureHeight", "lightCount", true)
       << "    shadeColor = clamp(lightFactor+ vec3(0.5, 0.5, 0.5), 0.0, 1.0);\n"
       << "    v_suv = (vPosition + pos) / size;\n}";
    return ss.str();
}

static std::string BuildWaterFragment() {
    std::ostringstream ss;
    ss << "#version 410 core\n"
       << "uniform sampler2DArray water_textures;\nuniform sampler2D fogOfWarMap;\n"
       << "uniform float current_texture;\nuniform vec4 mapBounds;\n"
       << "uniform vec4 u_fogColor;\nuniform vec4 u_fogParams;\n"
       << "in vec2 UV;\nin vec4 vertexColor;\nin vec3 shadeColor;\nin vec2 position;\nflat in float waterCoverage;\nin vec2 v_suv;\nout vec4 color;\n"
       << "void main() {\n"
       << "    vec2 d2 = min(position - mapBounds.xy, mapBounds.zw - position);\n"
       << "    float d1 = clamp(min(d2.x, d2.y) / 64.0 + 1.0, 0.0, 1.0) * 0.8 + 0.2;\n"
       << "    vec3 visualLight = clamp(shadeColor + vec3(0.6), 0.0, 1.0);\n"
       << "    float fogOfWarData = texture(fogOfWarMap, v_suv).r;\n"
       << "    float fcount = float(textureSize(water_textures, 0).z);\n"
       << "    float idx = current_texture;\n"
       << "    float la = floor(mod(idx, fcount));\n"
       << "    float lb = floor(mod(idx + 1.0, fcount));\n"
       << "    float frac = fract(idx);\n"
       << "    vec4 waterTex = mix(texture(water_textures, vec3(UV, la)), texture(water_textures, vec3(UV, lb)), frac);\n"
       << "    color = waterTex * vec4(vertexColor.rgb * visualLight * d1 * (1.0 - fogOfWarData), vertexColor.a * (waterCoverage));\n"
       << ShaderUtils::FogSystem(false, nullptr) << "}";
    return ss.str();
}

static std::string BuildUberSplatVertex() {
    return
        "#version 410 core\n"
        "in vec3 vPosition;\nin vec2 vUV;\n"
        "uniform mat4 MVP;\nuniform sampler2D u_heightMap;\nuniform sampler2D u_waterHeightMap;\n"
        "uniform vec2 u_centerOffset;\nuniform vec2 u_mapSize;\nuniform vec2 u_splatCenter;\n"
        "uniform float u_splatScale;\nuniform float u_splatRotation;\nuniform float u_waterHeightOffset;\n"
        "out vec2 UV;\nout vec2 vShadowUv;\nout vec2 vWorldPos;\n"
        "void main() {\n"
        "    float c = cos(u_splatRotation);\n"
        "    float s = sin(u_splatRotation);\n"
        "    vec2 local = vPosition.xz * u_splatScale;\n"
        "    vec2 rotated = vec2(local.x * c - local.y * s, local.x * s + local.y * c);\n"
        "    vec2 worldPos = u_splatCenter + rotated;\n"
        "    vec2 tilePos = (worldPos - u_centerOffset) / 128.0;\n"
        "    vec2 texCoord = clamp(tilePos / max(u_mapSize - 1.0, vec2(1.0)), vec2(0.0), vec2(1.0));\n"
        "    float groundHeight = texture(u_heightMap, texCoord).r;\n"
        "    float waterHeight = texture(u_waterHeightMap, texCoord).r + u_waterHeightOffset;\n"
        "    float finalHeight = max(groundHeight, waterHeight) * 128.0 + 2.0;\n"
        "    gl_Position = MVP * vec4(worldPos.x, finalHeight, worldPos.y, 1.0);\n"
        "    UV = vUV;\n    vWorldPos = worldPos;\n    vShadowUv = texCoord;\n"
        "}";
}

static std::string BuildUberSplatFragment() {
    return
        "#version 410 core\n"
        "in vec2 UV;\nin vec2 vShadowUv;\nin vec2 vWorldPos;\nout vec4 color;\n"
        "uniform sampler2D diffuseTexture;\nuniform sampler2D u_shadowMap;\nuniform sampler2D u_fogOfWarMap;\n"
        "uniform vec2 u_shadowScale;\nuniform vec4 u_fogColor;\nuniform vec4 u_fogParams;\n"
        "void main() {\n"
        "    color = texture(diffuseTexture, UV);\n"
        "    vec2 shadowUv = clamp(vShadowUv * u_shadowScale, vec2(0.0), vec2(1.0));\n"
        "    float shadow = texture(u_shadowMap, shadowUv).r;\n"
        "    float fogOfWar = texture(u_fogOfWarMap, vShadowUv).r;\n"
        "    color.rgb *= (1.0 - clamp(shadow + fogOfWar, 0.0, 1.0));\n"
        "    color.a *= (1.0 - fogOfWar);\n"
        + ShaderUtils::FogSystem(false, nullptr) + "}";
}

// Helper: add a texture-sampler binding pair
static void AddTexSmpPair(sg_shader_desc& desc, int viewSlot, int samplerSlot,
                           sg_shader_stage stage, sg_image_type imgType, sg_image_sample_type smpType,
                           const char* glslName, int& pairIdx) {
    desc.views[viewSlot].texture.stage = stage;
    desc.views[viewSlot].texture.image_type = imgType;
    desc.views[viewSlot].texture.sample_type = smpType;
    desc.samplers[samplerSlot].stage = stage;
    desc.samplers[samplerSlot].sampler_type = SG_SAMPLERTYPE_FILTERING;
    desc.texture_sampler_pairs[pairIdx].stage = stage;
    desc.texture_sampler_pairs[pairIdx].glsl_name = glslName;
    desc.texture_sampler_pairs[pairIdx].view_slot = (uint8_t)viewSlot;
    desc.texture_sampler_pairs[pairIdx].sampler_slot = (uint8_t)samplerSlot;
    pairIdx++;
}

sg_shader TerrainShaders::CreateGroundShader(int textureCount) {
    if (textureCount < 1) textureCount = 1;
    if (textureCount > 17) textureCount = 17;
    std::string vs = BuildGroundVertex();
    std::string fs = BuildGroundFragment(textureCount);
    sg_shader_desc desc = {};
    desc.vertex_func.source = vs.c_str();
    desc.fragment_func.source = fs.c_str();

    // VS uniform block
    desc.uniform_blocks[0].stage = SG_SHADERSTAGE_VERTEX;
    desc.uniform_blocks[0].size = 144;
    desc.uniform_blocks[0].layout = SG_UNIFORMLAYOUT_NATIVE;
    const char* vsNames[] = {"MVP","DepthBiasMVP","centerOffsetX","centerOffsetY","lightCount","lightTextureHeight"};
    sg_uniform_type vsTypes[] = {SG_UNIFORMTYPE_MAT4,SG_UNIFORMTYPE_MAT4,SG_UNIFORMTYPE_FLOAT,SG_UNIFORMTYPE_FLOAT,SG_UNIFORMTYPE_FLOAT,SG_UNIFORMTYPE_FLOAT};
    for (int i = 0; i < 6; ++i) {
        desc.uniform_blocks[0].glsl_uniforms[i].glsl_name = vsNames[i];
        desc.uniform_blocks[0].glsl_uniforms[i].type = vsTypes[i];
        desc.uniform_blocks[0].glsl_uniforms[i].array_count = 1;
    }

    // FS uniform block
    desc.uniform_blocks[1].stage = SG_SHADERSTAGE_FRAGMENT;
    desc.uniform_blocks[1].size = 44;
    desc.uniform_blocks[1].layout = SG_UNIFORMLAYOUT_NATIVE;
    const char* fsNames[] = {"show_pathing_map","show_lighting","base_texture_only","u_fogColor","u_fogParams"};
    sg_uniform_type fsTypes[] = {SG_UNIFORMTYPE_FLOAT,SG_UNIFORMTYPE_FLOAT,SG_UNIFORMTYPE_FLOAT,SG_UNIFORMTYPE_FLOAT4,SG_UNIFORMTYPE_FLOAT4};
    for (int i = 0; i < 5; ++i) {
        desc.uniform_blocks[1].glsl_uniforms[i].glsl_name = fsNames[i];
        desc.uniform_blocks[1].glsl_uniforms[i].type = fsTypes[i];
        desc.uniform_blocks[1].glsl_uniforms[i].array_count = 1;
    }

    // VS texture bindings: height_texture, height_cliff_texture, terrain_texture_list(usampler), lightTexture
    int pairIdx = 0;
    AddTexSmpPair(desc, 0, 0, SG_SHADERSTAGE_VERTEX, SG_IMAGETYPE_2D, SG_IMAGESAMPLETYPE_FLOAT, "height_texture", pairIdx);
    AddTexSmpPair(desc, 1, 1, SG_SHADERSTAGE_VERTEX, SG_IMAGETYPE_2D, SG_IMAGESAMPLETYPE_FLOAT, "height_cliff_texture", pairIdx);
    desc.views[2].texture.stage = SG_SHADERSTAGE_VERTEX;
    desc.views[2].texture.image_type = SG_IMAGETYPE_2D;
    desc.views[2].texture.sample_type = SG_IMAGESAMPLETYPE_UINT;
    desc.samplers[2].stage = SG_SHADERSTAGE_VERTEX;
    desc.samplers[2].sampler_type = SG_SAMPLERTYPE_NONFILTERING;
    desc.texture_sampler_pairs[pairIdx].stage = SG_SHADERSTAGE_VERTEX;
    desc.texture_sampler_pairs[pairIdx].glsl_name = "terrain_texture_list";
    desc.texture_sampler_pairs[pairIdx].view_slot = 2;
    desc.texture_sampler_pairs[pairIdx].sampler_slot = 2;
    pairIdx++;
    AddTexSmpPair(desc, 3, 3, SG_SHADERSTAGE_VERTEX, SG_IMAGETYPE_2D, SG_IMAGESAMPLETYPE_FLOAT, "lightTexture", pairIdx);

    // FS texture bindings: sample0..N (texture arrays), shadowMap, fogOfWarMap
    std::vector<std::string> samplerNames;
    samplerNames.reserve(textureCount);
    for (int i = 0; i < textureCount; ++i) {
        samplerNames.push_back("sample" + std::to_string(i));
    }
    for (int i = 0; i < textureCount; ++i) {
        AddTexSmpPair(desc, 4 + i, 4 + i, SG_SHADERSTAGE_FRAGMENT, SG_IMAGETYPE_ARRAY, SG_IMAGESAMPLETYPE_FLOAT, samplerNames[i].c_str(), pairIdx);
    }
    AddTexSmpPair(desc, 4 + textureCount, 4 + textureCount, SG_SHADERSTAGE_FRAGMENT, SG_IMAGETYPE_2D, SG_IMAGESAMPLETYPE_FLOAT, "shadowMap", pairIdx);
    AddTexSmpPair(desc, 5 + textureCount, 5 + textureCount, SG_SHADERSTAGE_FRAGMENT, SG_IMAGETYPE_2D, SG_IMAGESAMPLETYPE_FLOAT, "fogOfWarMap", pairIdx);

    sg_shader shader = sg_make_shader(&desc);
    if (sg_query_shader_state(shader) != SG_RESOURCESTATE_VALID) shader = {0};
    return shader;
}

sg_shader TerrainShaders::CreateCliffShader(int) {
    std::string vs = BuildCliffVertex();
    std::string fs = BuildCliffFragment();
    sg_shader_desc desc = {};
    desc.vertex_func.source = vs.c_str();
    desc.fragment_func.source = fs.c_str();

    desc.uniform_blocks[0].stage = SG_SHADERSTAGE_VERTEX;
    desc.uniform_blocks[0].size = 96;
    desc.uniform_blocks[0].layout = SG_UNIFORMLAYOUT_NATIVE;
    const char* vsUnames[] = {"MVP","centerOffsetX","centerOffsetY","cliffOffset","lightCount","lightTextureHeight"};
    sg_uniform_type vsUtypes[] = {SG_UNIFORMTYPE_MAT4,SG_UNIFORMTYPE_FLOAT,SG_UNIFORMTYPE_FLOAT,SG_UNIFORMTYPE_FLOAT4,SG_UNIFORMTYPE_FLOAT,SG_UNIFORMTYPE_FLOAT};
    for (int i = 0; i < 6; ++i) {
        desc.uniform_blocks[0].glsl_uniforms[i].glsl_name = vsUnames[i];
        desc.uniform_blocks[0].glsl_uniforms[i].type = vsUtypes[i];
        desc.uniform_blocks[0].glsl_uniforms[i].array_count = 1;
    }

    desc.uniform_blocks[1].stage = SG_SHADERSTAGE_FRAGMENT;
    desc.uniform_blocks[1].size = 40;
    desc.uniform_blocks[1].layout = SG_UNIFORMLAYOUT_NATIVE;
    const char* fsUnames[] = {"show_lighting","base_texture_only","u_fogColor","u_fogParams"};
    for (int i = 0; i < 4; ++i) {
        desc.uniform_blocks[1].glsl_uniforms[i].glsl_name = fsUnames[i];
        desc.uniform_blocks[1].glsl_uniforms[i].type = (i < 2) ? SG_UNIFORMTYPE_FLOAT : SG_UNIFORMTYPE_FLOAT4;
        desc.uniform_blocks[1].glsl_uniforms[i].array_count = 1;
    }

    int p = 0;
    AddTexSmpPair(desc, 0, 0, SG_SHADERSTAGE_VERTEX, SG_IMAGETYPE_2D, SG_IMAGESAMPLETYPE_FLOAT, "height_texture", p);
    AddTexSmpPair(desc, 1, 1, SG_SHADERSTAGE_VERTEX, SG_IMAGETYPE_2D, SG_IMAGESAMPLETYPE_FLOAT, "shadowMap", p);
    AddTexSmpPair(desc, 2, 2, SG_SHADERSTAGE_VERTEX, SG_IMAGETYPE_2D, SG_IMAGESAMPLETYPE_FLOAT, "lightTexture", p);
    AddTexSmpPair(desc, 3, 3, SG_SHADERSTAGE_FRAGMENT, SG_IMAGETYPE_ARRAY, SG_IMAGESAMPLETYPE_FLOAT, "cliff_textures", p);
    AddTexSmpPair(desc, 4, 4, SG_SHADERSTAGE_FRAGMENT, SG_IMAGETYPE_2D, SG_IMAGESAMPLETYPE_FLOAT, "shadowMap", p);
    AddTexSmpPair(desc, 5, 5, SG_SHADERSTAGE_FRAGMENT, SG_IMAGETYPE_2D, SG_IMAGESAMPLETYPE_FLOAT, "fogOfWarMap", p);

    sg_shader s = sg_make_shader(&desc);
    if (sg_query_shader_state(s) != SG_RESOURCESTATE_VALID) s = {0};
    return s;
}

sg_shader TerrainShaders::CreateWaterShader(int) {
    std::string vs = BuildWaterVertex();
    std::string fs = BuildWaterFragment();
    sg_shader_desc desc = {};
    desc.vertex_func.source = vs.c_str();
    desc.fragment_func.source = fs.c_str();

    desc.uniform_blocks[0].stage = SG_SHADERSTAGE_VERTEX;
    desc.uniform_blocks[0].size = 148;
    desc.uniform_blocks[0].layout = SG_UNIFORMLAYOUT_NATIVE;
    const char* vsU[] = {"MVP","centerOffsetX","centerOffsetY","water_offset","lightCount","lightTextureHeight",
                         "shallow_color_min","shallow_color_max","deep_color_min","deep_color_max"};
    sg_uniform_type vsT[] = {SG_UNIFORMTYPE_MAT4,SG_UNIFORMTYPE_FLOAT,SG_UNIFORMTYPE_FLOAT,SG_UNIFORMTYPE_FLOAT,
                             SG_UNIFORMTYPE_FLOAT,SG_UNIFORMTYPE_FLOAT,SG_UNIFORMTYPE_FLOAT4,SG_UNIFORMTYPE_FLOAT4,
                             SG_UNIFORMTYPE_FLOAT4,SG_UNIFORMTYPE_FLOAT4};
    for (int i = 0; i < 10; ++i) { desc.uniform_blocks[0].glsl_uniforms[i].glsl_name = vsU[i]; desc.uniform_blocks[0].glsl_uniforms[i].type = vsT[i]; desc.uniform_blocks[0].glsl_uniforms[i].array_count = 1; }

    desc.uniform_blocks[1].stage = SG_SHADERSTAGE_FRAGMENT;
    desc.uniform_blocks[1].size = 52;
    desc.uniform_blocks[1].layout = SG_UNIFORMLAYOUT_NATIVE;
    const char* fsU[] = {"current_texture","mapBounds","u_fogColor","u_fogParams"};
    for (int i = 0; i < 4; ++i) { desc.uniform_blocks[1].glsl_uniforms[i].glsl_name = fsU[i]; desc.uniform_blocks[1].glsl_uniforms[i].type = (i==0)?SG_UNIFORMTYPE_FLOAT:SG_UNIFORMTYPE_FLOAT4; desc.uniform_blocks[1].glsl_uniforms[i].array_count = 1; }

    int p = 0;
    AddTexSmpPair(desc, 0, 0, SG_SHADERSTAGE_VERTEX, SG_IMAGETYPE_2D, SG_IMAGESAMPLETYPE_FLOAT, "water_height_texture", p);
    AddTexSmpPair(desc, 1, 1, SG_SHADERSTAGE_VERTEX, SG_IMAGETYPE_2D, SG_IMAGESAMPLETYPE_FLOAT, "ground_height_texture", p);
    AddTexSmpPair(desc, 2, 2, SG_SHADERSTAGE_VERTEX, SG_IMAGETYPE_2D, SG_IMAGESAMPLETYPE_FLOAT, "water_exists_texture", p);
    AddTexSmpPair(desc, 3, 3, SG_SHADERSTAGE_VERTEX, SG_IMAGETYPE_2D, SG_IMAGESAMPLETYPE_FLOAT, "lightTexture", p);
    AddTexSmpPair(desc, 4, 4, SG_SHADERSTAGE_FRAGMENT, SG_IMAGETYPE_ARRAY, SG_IMAGESAMPLETYPE_FLOAT, "water_textures", p);
    AddTexSmpPair(desc, 5, 5, SG_SHADERSTAGE_FRAGMENT, SG_IMAGETYPE_2D, SG_IMAGESAMPLETYPE_FLOAT, "fogOfWarMap", p);

    sg_shader s = sg_make_shader(&desc);
    if (sg_query_shader_state(s) != SG_RESOURCESTATE_VALID) s = {0};
    return s;
}

sg_shader TerrainShaders::CreateUberSplatShader() {
    std::string vs = BuildUberSplatVertex();
    std::string fs = BuildUberSplatFragment();
    sg_shader_desc desc = {};
    desc.vertex_func.source = vs.c_str();
    desc.fragment_func.source = fs.c_str();

    desc.uniform_blocks[0].stage = SG_SHADERSTAGE_VERTEX;
    desc.uniform_blocks[0].size = 100;
    desc.uniform_blocks[0].layout = SG_UNIFORMLAYOUT_NATIVE;
    const char* vsU[] = {"MVP","u_centerOffset","u_mapSize","u_splatCenter","u_splatScale","u_splatRotation","u_waterHeightOffset"};
    sg_uniform_type vsT[] = {SG_UNIFORMTYPE_MAT4,SG_UNIFORMTYPE_FLOAT2,SG_UNIFORMTYPE_FLOAT2,SG_UNIFORMTYPE_FLOAT2,SG_UNIFORMTYPE_FLOAT,SG_UNIFORMTYPE_FLOAT,SG_UNIFORMTYPE_FLOAT};
    for (int i = 0; i < 7; ++i) { desc.uniform_blocks[0].glsl_uniforms[i].glsl_name = vsU[i]; desc.uniform_blocks[0].glsl_uniforms[i].type = vsT[i]; desc.uniform_blocks[0].glsl_uniforms[i].array_count = 1; }

    desc.uniform_blocks[1].stage = SG_SHADERSTAGE_FRAGMENT;
    desc.uniform_blocks[1].size = 40;
    desc.uniform_blocks[1].layout = SG_UNIFORMLAYOUT_NATIVE;
    const char* fsU[] = {"u_shadowScale","u_fogColor","u_fogParams"};
    for (int i = 0; i < 3; ++i) { desc.uniform_blocks[1].glsl_uniforms[i].glsl_name = fsU[i]; desc.uniform_blocks[1].glsl_uniforms[i].type = (i==0)?SG_UNIFORMTYPE_FLOAT2:SG_UNIFORMTYPE_FLOAT4; desc.uniform_blocks[1].glsl_uniforms[i].array_count = 1; }

    int p = 0;
    AddTexSmpPair(desc, 0, 0, SG_SHADERSTAGE_VERTEX, SG_IMAGETYPE_2D, SG_IMAGESAMPLETYPE_FLOAT, "u_heightMap", p);
    AddTexSmpPair(desc, 1, 1, SG_SHADERSTAGE_VERTEX, SG_IMAGETYPE_2D, SG_IMAGESAMPLETYPE_FLOAT, "u_waterHeightMap", p);
    AddTexSmpPair(desc, 2, 2, SG_SHADERSTAGE_FRAGMENT, SG_IMAGETYPE_2D, SG_IMAGESAMPLETYPE_FLOAT, "diffuseTexture", p);
    AddTexSmpPair(desc, 3, 3, SG_SHADERSTAGE_FRAGMENT, SG_IMAGETYPE_2D, SG_IMAGESAMPLETYPE_FLOAT, "u_shadowMap", p);
    AddTexSmpPair(desc, 4, 4, SG_SHADERSTAGE_FRAGMENT, SG_IMAGETYPE_2D, SG_IMAGESAMPLETYPE_FLOAT, "u_fogOfWarMap", p);

    sg_shader s = sg_make_shader(&desc);
    if (sg_query_shader_state(s) != SG_RESOURCESTATE_VALID) s = {0};
    return s;
}
