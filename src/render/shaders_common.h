#pragma once
#include <string>

namespace ShaderUtils {
    std::string LightSystem(const char* normalVar, const char* positionVar,
                            const char* lightTex, const char* lightTexHeight,
                            const char* lightCount, bool useNormal);
    std::string FogSystem(bool useExp, const char* fragColor);
    std::string GetFragmentChain(int count = 17);
}
