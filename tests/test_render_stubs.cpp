#include "third_party/sokol/sokol_gfx.h"

sg_image sg_make_image(const sg_image_desc*) {
    return {0};
}

void sg_destroy_image(sg_image) {
}

sg_resource_state sg_query_image_state(sg_image) {
    return SG_RESOURCESTATE_INVALID;
}

namespace Renderer {

bool IsActive() {
    return false;
}

}
