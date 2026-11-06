#include "SimpleFrame.h"

SimpleFrame::~SimpleFrame() {
    for (auto* c : children_) {
        if (c) {
            c->SetParent(nullptr);
            delete c;
        }
    }
    children_.clear();
}
