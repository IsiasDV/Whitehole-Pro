#pragma once

#include "whitehole/math/geometry.hpp"

#include <cstddef>
#include <string>

namespace whitehole::smg {

struct PlacementObject {
    std::size_t tableIndex{0};
    std::size_t rowIndex{0};
    std::string kind;
    std::string layer;
    std::string name;
    math::Vec3f position;
    math::Vec3f rotation;
    math::Vec3f scale{1.0F, 1.0F, 1.0F};
};

} // namespace whitehole::smg
