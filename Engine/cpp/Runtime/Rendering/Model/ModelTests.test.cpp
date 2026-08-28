#include <doctest_with_main.h>

import core;
import model;

using namespace draco;
using namespace draco::model;

TEST_CASE("model: core data types - names round-trip as UTF-8 strings")
{
    ModelMaterial mat;
    mat.setName(u8"steel");
    CHECK(mat.name() == u8"steel");

    Model model;
    CHECK(model.meshes().size() == 0u);
}
