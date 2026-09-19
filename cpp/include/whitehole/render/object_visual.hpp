#pragma once

// Object visual system: one consistent "category -> color + shape" language
// shared by the 3D viewport, the object list and any future surface. Pure
// data + math only (no Win32, no OpenGL) so it stays unit-testable and
// reusable by every renderer.
//
// The goal is that a beginner can tell *what an object is* at a glance:
// each placement object falls into exactly one category, each category has
// one color and one silhouette, and every surface of the app uses the same
// mapping.

#include "whitehole/math/geometry.hpp"

#include <string>
#include <vector>

namespace whitehole::render {

enum class ObjectCategory {
    Player,
    Enemy,
    Item,
    Terrain,
    Camera,
    Gravity,
    Zone,
    MapPart,
    Cutscene,
    Misc,
};

// Visual properties shared by every surface (viewport meshes, list chips,
// legends). Colors are linear RGB 0..1; the viewport and GUI both use them.
struct CategoryStyle {
    ObjectCategory category;
    const char* label;      // human-readable, e.g. "Enemy"
    const char* shapeName;  // human-readable silhouette, e.g. "pyramid"
    float color[3];         // linear RGB
    enum class Shape { Cube, Sphere, Pyramid, Octahedron, Cylinder } shape;
};

// The single source of truth for category visuals. Index by static_cast<int>.
[[nodiscard]] const CategoryStyle& categoryStyle(ObjectCategory category);

// Classify a placement object. Kind comes from the stage table ("obj",
// "camera", "area", ...); the object name refines it (Mario -> Player,
// Bullet Bill-like names -> Enemy, ...). Never throws; unknowns are Misc.
[[nodiscard]] ObjectCategory classifyObject(const std::string& kind, const std::string& name);

// Convenience: classify + style in one call.
[[nodiscard]] const CategoryStyle& objectStyle(const std::string& kind, const std::string& name);

// Unit mesh generators: triangles of a shape with radius 1, centred on the
// origin. The renderer scales these by the object's visual size.
[[nodiscard]] std::vector<math::Vec3f> shapeTriangles(CategoryStyle::Shape shape);

[[nodiscard]] std::size_t categoryCount() noexcept;

} // namespace whitehole::render
