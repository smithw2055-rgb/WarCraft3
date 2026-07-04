#pragma once

#include <string>
#include <vector>

const char* JassAnimationPrimaryTagName(int ordinal);
const char* JassAnimationSecondaryTagName(int ordinal);
int JassFindAnimationPrimaryTagOrdinal(const std::string& lowerToken);
int JassFindAnimationSecondaryTagOrdinal(const std::string& lowerToken);
void JassPopulateAnimationTagsFromSelector(const std::string& animationSelector,
                                           std::vector<int>& primaryOrdinals,
                                           std::vector<int>& secondaryOrdinals);
