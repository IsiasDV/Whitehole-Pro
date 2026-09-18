#include "whitehole/render/camera.hpp"

#include <algorithm>
#include <cmath>

namespace whitehole::render {

ViewportCamera::ViewportCamera() = default;

math::Vec3f ViewportCamera::eye() const noexcept {
    const float cosPitch = std::cos(pitchRadians);
    const float sinPitch = std::sin(pitchRadians);
    const float cosYaw = std::cos(yawRadians);
    const float sinYaw = std::sin(yawRadians);
    return {target.x + distance * cosPitch * cosYaw, target.y + distance * sinPitch,
            target.z + distance * cosPitch * sinYaw};
}

math::Matrix4 ViewportCamera::viewMatrix() const noexcept {
    const math::Vec3f position = eye();
    math::Vec3f forward = {target.x - position.x, target.y - position.y, target.z - position.z};
    const float length = forward.length();
    if (length < 0.000001F) {
        return math::Matrix4{};
    }
    forward = {forward.x / length, forward.y / length, forward.z / length};

    math::Vec3f worldUp{0.0F, 1.0F, 0.0F};
    if (std::abs(forward.y) > 0.999F) {
        worldUp = {0.0F, 0.0F, forward.y > 0.0F ? -1.0F : 1.0F};
    }
    const math::Vec3f right = math::Vec3f::cross(forward, worldUp).normalized();
    const math::Vec3f up = math::Vec3f::cross(right, forward);

    // Row-major view matrix (matches Matrix4::transformPoint convention).
    math::Matrix4 view;
    view.values = {right.x, up.x, -forward.x, 0.0F, right.y, up.y, -forward.y, 0.0F, right.z, up.z,
                   -forward.z, 0.0F, 0.0F, 0.0F, 0.0F, 1.0F};
    view.values[12] = -(right.x * position.x + right.y * position.y + right.z * position.z);
    view.values[13] = -(up.x * position.x + up.y * position.y + up.z * position.z);
    view.values[14] = (forward.x * position.x + forward.y * position.y + forward.z * position.z);
    return view;
}

math::Matrix4 ViewportCamera::projectionMatrix(float aspect) const noexcept {
    const float safeAspect = aspect > 0.000001F ? aspect : 1.0F;
    const float half = kFieldOfView * 0.5F;
    const float f = 1.0F / std::tan(half);
    const float nearPlane = kNearPlane;
    const float farPlane = kFarPlane;
    math::Matrix4 projection;
    projection.values.fill(0.0F);
    projection.values[0] = f / safeAspect;
    projection.values[5] = f;
    projection.values[10] = (farPlane + nearPlane) / (nearPlane - farPlane);
    projection.values[11] = -1.0F;
    projection.values[14] = (2.0F * farPlane * nearPlane) / (nearPlane - farPlane);
    return projection;
}

Ray ViewportCamera::screenToRay(float screenX, float screenY, float width, float height) const noexcept {
    Ray ray;
    ray.origin = eye();
    if (width <= 0.0F || height <= 0.0F) {
        return ray;
    }
    const float aspect = width / height;
    const float half = kFieldOfView * 0.5F;
    const float tanHalf = std::tan(half);
    const float ndcX = (2.0F * screenX / width - 1.0F) * aspect * tanHalf;
    const float ndcY = (1.0F - 2.0F * screenY / height) * tanHalf;

    const math::Vec3f position = ray.origin;
    math::Vec3f forward = {target.x - position.x, target.y - position.y, target.z - position.z};
    forward = forward.normalized();
    math::Vec3f worldUp{0.0F, 1.0F, 0.0F};
    if (std::abs(forward.y) > 0.999F) {
        worldUp = {0.0F, 0.0F, forward.y > 0.0F ? -1.0F : 1.0F};
    }
    const math::Vec3f right = math::Vec3f::cross(forward, worldUp).normalized();
    const math::Vec3f up = math::Vec3f::cross(right, forward);
    ray.direction = {forward.x + right.x * ndcX + up.x * ndcY, forward.y + right.y * ndcX + up.y * ndcY,
                     forward.z + right.z * ndcX + up.z * ndcY};
    ray.direction = ray.direction.normalized();
    return ray;
}

void ViewportCamera::orbit(float deltaYaw, float deltaPitch) noexcept {
    yawRadians += deltaYaw;
    pitchRadians = std::clamp(pitchRadians + deltaPitch, -1.55F, 1.55F);
}

void ViewportCamera::pan(float deltaX, float deltaY) noexcept {
    // Pan in camera plane, scaled by distance so far scenes move faster.
    const math::Vec3f position = eye();
    math::Vec3f forward = {target.x - position.x, target.y - position.y, target.z - position.z};
    forward = forward.normalized();
    math::Vec3f worldUp{0.0F, 1.0F, 0.0F};
    if (std::abs(forward.y) > 0.999F) {
        worldUp = {0.0F, 0.0F, forward.y > 0.0F ? -1.0F : 1.0F};
    }
    const math::Vec3f right = math::Vec3f::cross(forward, worldUp).normalized();
    const math::Vec3f up = math::Vec3f::cross(right, forward);
    const float scale = distance * 0.0016F;
    target.x -= (right.x * deltaX - up.x * deltaY) * scale;
    target.y -= (right.y * deltaX - up.y * deltaY) * scale;
    target.z -= (right.z * deltaX - up.z * deltaY) * scale;
}

void ViewportCamera::dolly(float wheelDelta) noexcept {
    distance = std::clamp(distance * (wheelDelta > 0.0F ? 0.9F : 1.1111111F), 5.0F, 20000.0F);
}

void ViewportCamera::frameTarget(const math::Vec3f& point, float framedDistance) noexcept {
    target = point;
    distance = std::clamp(framedDistance, 5.0F, 20000.0F);
}

bool ViewportCamera::worldToScreen(const math::Vec3f& point, float width, float height, float& outX,
                                   float& outY) const noexcept {
    if (width <= 0.0F || height <= 0.0F) {
        return false;
    }
    const math::Matrix4 view = viewMatrix();
    const math::Vec3f viewPoint = view.transformPoint(point);
    if (viewPoint.z >= -kNearPlane) {
        return false;
    }
    const float aspect = width / height;
    const float half = kFieldOfView * 0.5F;
    const float f = 1.0F / std::tan(half);
    outX = width * 0.5F * (1.0F + (viewPoint.x * f / aspect) / -viewPoint.z);
    outY = height * 0.5F * (1.0F - (viewPoint.y * f) / -viewPoint.z);
    return true;
}

} // namespace whitehole::render
