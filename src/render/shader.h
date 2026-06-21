#pragma once
#include "third_party/sokol/sokol_gfx.h"

template<typename T>
inline sg_range sg_rng(T& data) {
    sg_range r = { &data, sizeof(data) };
    return r;
}
