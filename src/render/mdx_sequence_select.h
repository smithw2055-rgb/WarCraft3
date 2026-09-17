#pragma once

#include "model/mdx.h"
#include <string>
#include <vector>

int MdxSelectSequenceIndexByPrimaryAndTags(
    const MdxModel& model,
    const char* primaryToken,
    const std::vector<std::string>& secondaryTags);

int MdxSelectSpellSequenceIndex(
    const MdxModel& model,
    const std::vector<std::string>& secondaryTags);

float MdxSequenceDurationSeconds(const MdxModel& model, int sequenceIndex);
