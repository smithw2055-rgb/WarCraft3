#include "shaders_common.h"
#include <sstream>

std::string ShaderUtils::LightSystem(const char* normalVar, const char* positionVar,
                                     const char* lightTex, const char* lightTexHeight,
                                     const char* lightCount, bool useNormal) {
    std::ostringstream ss;
    ss << "        vec3 lightFactor = vec3(0.0,0.0,0.0);\n"
       << "        for(float lightIndex = 0.5; lightIndex < " << lightCount << "; lightIndex += 1.0) {\n"
       << "          float rowPos = (lightIndex) / " << lightTexHeight << ";\n"
       << "          vec4 lightPosition = texture(" << lightTex << ", vec2(0.125, rowPos));\n"
       << "          vec3 lightExtra = texture(" << lightTex << ", vec2(0.375, rowPos)).xyz;\n"
       << "          vec4 lightColor = texture(" << lightTex << ", vec2(0.625, rowPos));\n"
       << "          vec4 lightAmbColor = texture(" << lightTex << ", vec2(0.875, rowPos));\n"
       << "          if(lightExtra.x > 1.5) {\n"
       << "            float dist = length(" << positionVar << " - vec3(lightPosition." << (useNormal ? "xyw" : "xyz") << "));\n"
       << "            float attenuationStart = lightExtra.y;\n"
       << "            float attenuationEnd = lightExtra.z;\n"
       << "            if( dist <= attenuationEnd ) {\n"
       << "              float attenuationDist = clamp((dist-attenuationStart), 0.001, (attenuationEnd-attenuationStart));\n"
       << "              float attenuationFactor = 1.0/(attenuationDist);\n"
       << "              lightFactor += attenuationFactor * lightAmbColor.a * lightAmbColor.rgb;\n"
       << "            }\n"
       << "          } else if(lightExtra.x > 0.5) {\n"
       << "            vec3 lightDirection = vec3(lightPosition.xyz);\n"
       << "            vec3 lightFactorContribution = lightColor.a * lightColor.rgb * clamp(dot(" << normalVar << ", lightDirection), 0.0, 1.0);\n"
       << "            if(lightFactorContribution.r > 1.0 || lightFactorContribution.g > 1.0 || lightFactorContribution.b > 1.0)\n"
       << "              lightFactorContribution = clamp(lightFactorContribution, 0.0, 1.0);\n"
       << "            lightFactor += lightFactorContribution + lightAmbColor.a * lightAmbColor.rgb;\n"
       << "          } else {\n"
       << "            vec3 deltaBtwn = " << positionVar << " - lightPosition.xyz;\n"
       << "            float dist = length(" << positionVar << " - vec3(lightPosition.xyz)) / 64.0 + 1.0;\n"
       << "            vec3 lightDirection = normalize(-deltaBtwn);\n"
       << "            vec3 lightFactorContribution = (lightColor.a/(pow(dist, 2.0))) * lightColor.rgb * clamp(dot(" << normalVar << ", lightDirection), 0.0, 1.0);\n"
       << "            if(lightFactorContribution.r > 1.0 || lightFactorContribution.g > 1.0 || lightFactorContribution.b > 1.0)\n"
       << "              lightFactorContribution = clamp(lightFactorContribution, 0.0, 1.0);\n"
       << "            lightFactor += lightFactorContribution + (lightAmbColor.a/(pow(dist, 2.0))) * lightAmbColor.rgb;\n"
       << "          }\n"
       << "        }\n";
    return ss.str();
}

std::string ShaderUtils::FogSystem(bool useExp, const char* fragColor) {
    std::ostringstream ss;
    if (useExp) {
        ss << "      if(!u_unfogged && u_fogParams.x > 0.5) {\n";
    } else {
        ss << "      if(u_fogParams.x > 0.5) {\n";
    }
    ss << "        float fogAmount = clamp(((1.0 / gl_FragCoord.w) - u_fogParams.y) / (u_fogParams.z - u_fogParams.y), 0.0, 1.0);\n";
    if (fragColor) {
        ss << "        if (" << fragColor << ") {\n"
           << "          color.rgb = color.rgb * (1.0 - fogAmount) + u_fogColor.rgb * fogAmount;\n"
           << "        } else {\n"
           << "          color.rgb = color.rgb * (vec3(1.0 - fogAmount) + u_fogColor.rgb * fogAmount);\n"
           << "        }\n";
    } else {
        ss << "        color.rgb = color.rgb * (1.0 - fogAmount) + u_fogColor.rgb * fogAmount;\n";
    }
    ss << "      }\n";
    return ss.str();
}

std::string ShaderUtils::GetFragmentChain(int count) {
    std::ostringstream ss;
    ss << "vec4 get_fragment(uint id, vec3 uv) {\n"
       << "    vec2 dx = dFdx(uv.xy);\n"
       << "    vec2 dy = dFdy(uv.xy);\n"
       << "    switch(id) {\n";
    for (int i = 0; i < count; ++i) {
        ss << "        case " << i << "u:\n"
           << "            return textureGrad(sample" << i << ", uv, dx, dy);\n";
    }
    ss << "        default:\n"
       << "            return vec4(0, 0, 0, 0);\n"
       << "    }\n"
       << "}\n";
    return ss.str();
}
