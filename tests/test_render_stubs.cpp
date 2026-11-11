#include "third_party/sokol/sokol_gfx.h"

bool sg_isvalid(void) {
    return false;
}

sg_image sg_make_image(const sg_image_desc*) {
    return {0};
}

void sg_destroy_image(sg_image) {
}

void sg_update_image(sg_image, const sg_image_data*) {
}

sg_resource_state sg_query_image_state(sg_image) {
    return SG_RESOURCESTATE_INVALID;
}

sg_buffer sg_make_buffer(const sg_buffer_desc*) {
    return {0};
}

void sg_destroy_buffer(sg_buffer) {
}

void sg_update_buffer(sg_buffer, const sg_range*) {
}

sg_resource_state sg_query_buffer_state(sg_buffer) {
    return SG_RESOURCESTATE_INVALID;
}

sg_view sg_make_view(const sg_view_desc*) {
    return {0};
}

void sg_destroy_view(sg_view) {
}

sg_resource_state sg_query_view_state(sg_view) {
    return SG_RESOURCESTATE_INVALID;
}

sg_sampler sg_make_sampler(const sg_sampler_desc*) {
    return {0};
}

void sg_destroy_sampler(sg_sampler) {
}

sg_resource_state sg_query_sampler_state(sg_sampler) {
    return SG_RESOURCESTATE_INVALID;
}

sg_shader sg_make_shader(const sg_shader_desc*) {
    return {0};
}

void sg_destroy_shader(sg_shader) {
}

sg_resource_state sg_query_shader_state(sg_shader) {
    return SG_RESOURCESTATE_INVALID;
}

sg_pipeline sg_make_pipeline(const sg_pipeline_desc*) {
    return {0};
}

void sg_destroy_pipeline(sg_pipeline) {
}

sg_resource_state sg_query_pipeline_state(sg_pipeline) {
    return SG_RESOURCESTATE_INVALID;
}

void sg_apply_pipeline(sg_pipeline) {
}

void sg_apply_bindings(const sg_bindings*) {
}

void sg_apply_uniforms(int, const sg_range*) {
}

void sg_draw(int, int, int) {
}

void sg_apply_viewport(int, int, int, int, bool) {
}

void sg_apply_scissor_rect(int, int, int, int, bool) {
}

namespace Renderer {

bool IsActive() {
    return false;
}

void BeginFrame() {
}

void EndFrame() {
}

void SetClearColor(const float[4]) {
}

}

int sg_append_buffer(sg_buffer, const sg_range*) {
    return 0;
}
