#pragma once
#include <memory>

#include "resource/slk.h"

class CAbility;

// Batch 22: retail code variants reusing existing spell classes.
// Afod/Acht share the ANfd/ANht effects (Finger of Death / Howl of Terror)
// with their own retail orders; ANin/Aams are the generic Inferno and base
// Anti-Magic Shell codes. No new effect classes: this module owns the alias
// construction so the numbered batch sequence has no gap. Codes and order
// ids verified against Units\AbilityData.slk (see test_spells22.cpp).
std::unique_ptr<CAbility> CreateSpells22Alias(int handleId, War3ID code);
