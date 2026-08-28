// Compile ufbx as C++ so we don't need to enable C in the project.
// ufbx is written as clean C that compiles as C++ without issues.
#ifdef _MSC_VER
#pragma warning(push, 0)
#endif
#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Weverything"
#endif

#include "ufbx.c"

#ifdef __clang__
#pragma clang diagnostic pop
#endif
#ifdef _MSC_VER
#pragma warning(pop)
#endif
