#include "whitehole/render/viewport_scene.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

namespace whitehole::render {
namespace {

// Java stores rotation in degrees (dir_x/dir_y/dir_z). Keep the same
// XYZ euler order the Java renderer applies.
constexpr float kDegreesToRadians = 3.141592653589793F / 180.0F;

// Inverse of T * R * S (no shear): transpose rotation, divide out scale.
math::Matrix4 placementWorldInverse(const math::Matrix4& world, const math::Vec3f& scale) noexcept {
    math::Matrix4 inverse;
    inverse.values.fill(0.0F);
    const float safeX = std::abs(scale.x) > 0.000001F ? scale.x : 1.0F;
    const float safeY = std::abs(scale.y) > 0.000001F ? scale.y : 1.0F;
    const float safeZ = std::abs(scale.z) > 0.000001F ? scale.z : 1.0F;
    inverse.values[0] = world.values[0] / (safeX * safeX);
    inverse.values[1] = world.values[4] / (safeX * safeX);
    inverse.values[2] = world.values[8] / (safeX * safeX);
    inverse.values[4] = world.values[1] / (safeY * safeY);
    inverse.values[5] = world.values[5] / (safeY * safeY);
    inverse.values[6] = world.values[9] / (safeY * safeY);
    inverse.values[8] = world.values[2] / (safeZ * safeZ);
    inverse.values[9] = world.values[6] / (safeZ * safeZ);
    inverse.values[10] = world.values[10] / (safeZ * safeZ);
    inverse.values[15] = 1.0F;
    const math::Vec3f translation{world.values[12], world.values[13], world.values[14]};
    inverse.values[12] =
        -(inverse.values[0] * translation.x + inverse.values[4] * translation.y + inverse.values[8] * translation.z);
    inverse.values[13] =
        -(inverse.values[1] * translation.x + inverse.values[5] * translation.y + inverse.values[9] * translation.z);
    inverse.values[14] =
        -(inverse.values[2] * translation.x + inverse.values[6] * translation.y + inverse.values[10] * translation.z);
    return inverse;
}

} // namespace

math::Matrix4 placementWorldMatrix(const smg::PlacementObject& object) noexcept {
    const math::Vec3f safeScale{std::abs(object.scale.x) > 0.000001F ? object.scale.x : 1.0F,
                                std::abs(object.scale.y) > 0.000001F ? object.scale.y : 1.0F,
                                std::abs(object.scale.z) > 0.000001F ? object.scale.z : 1.0F};
    const math::Matrix4 scaled = math::Matrix4::scale(
        {safeScale.x * kPlaceholderHalfExtent, safeScale.y * kPlaceholderHalfExtent, safeScale.z * kPlaceholderHalfExtent});
    const math::Matrix4 rotation = math::Matrix4::rotationZ(object.rotation.z * kDegreesToRadians) *
                                   math::Matrix4::rotationY(object.rotation.y * kDegreesToRadians) *
                                   math::Matrix4::rotationX(object.rotation.x * kDegreesToRadians);
    return math::Matrix4::translation(object.position) * rotation * scaled;
}

std::optional<float> rayIntersectsBox(const Ray& ray, const ViewportBox& box) noexcept {

    const math::Vec3f scale{math::Vec3f{box.world.values[0], box.world.values[1], box.world.values[2]}.length(),
                            math::Vec3f{box.world.values[4], box.world.values[5], box.world.values[6]}.length(),
                            math::Vec3f{box.world.values[8], box.world.values[9], box.world.values[10]}.length()};
    const math::Matrix4 inverse = placementWorldInverse(box.world, scale);
    const math::Vec3f localOrigin = inverse.transformPoint(ray.origin);
    const math::Vec3f localDirection{ray.direction.x * inverse.values[0] + ray.direction.y * inverse.values[4] +
                                         ray.direction.z * inverse.values[8],
                                     ray.direction.x * inverse.values[1] + ray.direction.y * inverse.values[5] +
                                         ray.direction.z * inverse.values[9],
                                     ray.direction.x * inverse.values[2] + ray.direction.y * inverse.values[6] +
                                         ray.direction.z * inverse.values[10]};

    // Slab test against unit box [-1,1]^3 (extents baked into world matrix).
    float tMin = 0.0F;
    float tMax = std::numeric_limits<float>::infinity();
    const float origins[3] = {localOrigin.x, localOrigin.y, localOrigin.z};
    const float directions[3] = {localDirection.x, localDirection.y, localDirection.z};
    for (int axis = 0; axis < 3; ++axis) {
        const float origin = origins[axis];
        const float direction = directions[axis];
        if (std::abs(direction) < 0.0000001F) {
            if (origin < -1.0F || origin > 1.0F) {
                return std::nullopt;
            }
            continue;
        }
        float entry = (-1.0F - origin) / direction;
        float exit = (1.0F - origin) / direction;
        if (entry > exit) {
            std::swap(entry, exit);
        }
        tMin = std::max(tMin, entry);
        tMax = std::min(tMax, exit);
        if (tMin > tMax) {
            return std::nullopt;
        }
    }
    return tMin;
}

void ViewportScene::rebuild(const std::vector<smg::PlacementObject>& objects) {
    boxes_.clear();
    boxes_.reserve(objects.size());
    math::Vec3f sum{0.0F, 0.0F, 0.0F};
    for (std::size_t index = 0; index < objects.size(); ++index) {
        const auto& object = objects[index];
        ViewportBox box;
        box.objectIndex = index;
        box.name = object.name;
        box.kind = object.kind;
        box.world = placementWorldMatrix(object);
        box.center = object.position;
        const float extent = kPlaceholderHalfExtent *
                             std::max({std::abs(object.scale.x), std::abs(object.scale.y), std::abs(object.scale.z)});
        box.halfExtents = {extent, extent, extent};
        boxes_.push_back(box);
        sum = sum + object.position;
    }
    if (!boxes_.empty()) {
        const float count = static_cast<float>(boxes_.size());
        center_ = {sum.x / count, sum.y / count, sum.z / count};
        float maxRadius = 0.0F;
        for (const auto& box : boxes_) {
            maxRadius = std::max(maxRadius, (box.center - center_).length() + box.halfExtents.length());
        }
        frameDistance_ = std::clamp(maxRadius * 1.6F, 200.0F, 12000.0F);
    } else {
        center_ = {};
        frameDistance_ = 800.0F;
    }
}

void ViewportScene::clear() noexcept {
    boxes_.clear();
    center_ = {};
    frameDistance_ = 800.0F;
}

std::optional<std::size_t> ViewportScene::pick(const ViewportCamera& camera, float screenX, float screenY, float width,
                                               float height, float maxDistance) const noexcept {
    if (boxes_.empty() || width <= 0.0F || height <= 0.0F) {
        return std::nullopt;
    }
    const Ray ray = camera.screenToRay(screenX, screenY, width, height);
    std::optional<std::size_t> best;
    float bestDistance = maxDistance;
    for (const auto& box : boxes_) {
        const auto distance = rayIntersectsBox(ray, box);
        if (distance.has_value() && *distance < bestDistance) {
            bestDistance = *distance;
            best = box.objectIndex;
        }
    }
    return best;
}

} // namespace whitehole::render
