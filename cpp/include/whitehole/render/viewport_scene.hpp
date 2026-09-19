#pragma once

// Viewport scene model: turns StageArchive placement objects into oriented
// boxes the renderer can draw, plus CPU picking. No Win32, no OpenGL here,
// so this stays unit-testable and reusable by any future renderer
// (legacy GL today, modern GL / BMD models tomorrow).

#include "whitehole/math/geometry.hpp"
#include "whitehole/render/camera.hpp"
#include "whitehole/render/object_visual.hpp"
#include "whitehole/smg/placement.hpp"

#include <cstddef>
#include <optional>
#include <string>
#include <vector>

namespace whitehole::render {

// Half-extent of a placeholder box before per-object scale is applied.
// Large enough to click at galaxy scale, small enough not to swallow maps.
inline constexpr float kPlaceholderHalfExtent = 25.0F;

// Every object is drawn with at least this scale so micro-scaled objects
// (scale 0.001 etc.) stay visible and clickable at galaxy zoom levels.
inline constexpr float kMinVisualScale = 0.35F;

struct ViewportBox {
    std::size_t objectIndex{0};
    std::string name;
    std::string kind;
    math::Matrix4 world{};
    math::Vec3f center{};
    math::Vec3f halfExtents{kPlaceholderHalfExtent, kPlaceholderHalfExtent, kPlaceholderHalfExtent};
    ObjectCategory category{ObjectCategory::Misc};
};

class ViewportScene {
public:
    void rebuild(const std::vector<smg::PlacementObject>& objects);
    void clear() noexcept;

    [[nodiscard]] const std::vector<ViewportBox>& boxes() const noexcept { return boxes_; }
    [[nodiscard]] bool empty() const noexcept { return boxes_.empty(); }

    // Closest box hit by a camera ray. Returns the object index, if any.
    // `maxDistance` keeps far-away misclicks from selecting across the map.
    [[nodiscard]] std::optional<std::size_t> pick(const ViewportCamera& camera, float screenX, float screenY,
                                                 float width, float height,
                                                 float maxDistance = 20000.0F) const noexcept;

    // Frame-all helper: scene center + suggested camera distance.
    [[nodiscard]] math::Vec3f center() const noexcept { return center_; }
    [[nodiscard]] float frameDistance() const noexcept { return frameDistance_; }

private:
    std::vector<ViewportBox> boxes_;
    math::Vec3f center_{};
    float frameDistance_{800.0F};
};

[[nodiscard]] math::Matrix4 placementWorldMatrix(const smg::PlacementObject& object) noexcept;

// Ray vs oriented box (world matrix maps unit box [-1,1]^3 * halfExtents).
// Returns distance along the ray, or nullopt on miss.
[[nodiscard]] std::optional<float> rayIntersectsBox(const Ray& ray, const ViewportBox& box) noexcept;

} // namespace whitehole::render
