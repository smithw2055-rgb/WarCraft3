#pragma once
// MDL text model parser. Produces the same MdxModel representation as the
// MDX binary loader (single internal model, like ref's mdlx layer), so
// custom-map and user models shipped as .mdl text work everywhere .mdx
// does. Unknown blocks/fields are skipped gracefully; malformed input
// fails with a line-anchored diagnostic.
#include <cstddef>
#include <string>

struct MdxModel;

bool MdlLoad(const char* text, size_t size, MdxModel& out,
             std::string* error = nullptr);
