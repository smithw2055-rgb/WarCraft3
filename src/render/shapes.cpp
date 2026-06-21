#include "shapes.h"

Shapes& Shapes::Instance() {
    static Shapes instance;
    return instance;
}

void Shapes::EnsureCreated() {
    if (sg_query_buffer_state(vertexBuffer) != SG_RESOURCESTATE_VALID) {
        sg_buffer_desc vdesc = {};
        vdesc.usage.vertex_buffer = true;
        vdesc.usage.immutable = true;
        vdesc.data.ptr = vertices.data();
        vdesc.data.size = vertices.size() * sizeof(float);
        vertexBuffer = sg_make_buffer(&vdesc);

        sg_buffer_desc idesc = {};
        idesc.usage.index_buffer = true;
        idesc.usage.immutable = true;
        idesc.data.ptr = indices.data();
        idesc.data.size = indices.size() * sizeof(unsigned);
        indexBuffer = sg_make_buffer(&idesc);
    }
}

void Shapes::Bind() {
    EnsureCreated();
}
