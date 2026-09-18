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

math::Vec3f ViewportCamera::forward() const noexcept {
    const math::Vec3f position = eye();
    const math::Vec3f toTarget{target.x - position.x, target.y - position.y, target.z - position.z};
    return toTarget.normalized();
}

math::Vec3f ViewportCamera::up() const noexcept {
    // Java parity: the editor's up vector flips only once the camera passes
    // straight up/down. Clamping the pitch inside +/-kMaxPitch keeps this
    // continuous, so the view never rolls or flips near the poles the way the
    // old "|forward.y| > 0.999 -> +/-Z up" special case did.
    return std::cos(pitchRadians) >= 0.0F ? math::Vec3f{0.0F, 1.0F, 0.0F} : math::Vec3f{0.0F, -1.0F, 0.0F};
}

math::Vec3f ViewportCamera::right() const noexcept {
    const math::Vec3f facing = forward();
    math::Vec3f sideways = math::Vec3f::cross(facing, up());
    if (sideways.length() < 0.000001F) {
        // Degenerate only when looking exactly along the up axis; nudge to a
        // stable fallback instead of returning a zero basis.
        sideways = math::Vec3f::cross(facing, {0.0F, 0.0F, 1.0F});
        if (sideways.length() < 0.000001F) {
            return {1.0F, 0.0F, 0.0F};
        }
    }
    return sideways.normalized();
}

math::Matrix4 ViewportCamera::viewMatrix() const noexcept {
    const math::Vec3f position = eye();
    const math::Vec3f facing = forward();
    if (facing.length() < 0.000001F) {
        return math::Matrix4{};
    }
    const math::Vec3f sideways = right();
    const math::Vec3f upwards = math::Vec3f::cross(sideways, facing);

    // Row-major view matrix (matches Matrix4::transformPoint convention).
    math::Matrix4 view;
    view.values = {sideways.x, upwards.x, -facing.x, 0.0F, sideways.y, upwards.y, -facing.y, 0.0F, sideways.z,
                   upwards.z, -facing.z, 0.0F, 0.0F, 0.0F, 0.0F, 1.0F};
    view.values[12] = -(sideways.x * position.x + sideways.y * position.y + sideways.z * position.z);
    view.values[13] = -(upwards.x * position.x + upwards.y * position.y + upwards.z * position.z);
    view.values[14] = (facing.x * position.x + facing.y * position.y + facing.z * position.z);
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

    // Same basis the view matrix (and therefore the renderer) uses, so a click
    // always ray-casts through exactly the pixel it points at.
    const math::Vec3f facing = forward();
    const math::Vec3f sideways = right();
    const math::Vec3f upwards = math::Vec3f::cross(sideways, facing);
    ray.direction = {facing.x + sideways.x * ndcX + upwards.x * ndcY, facing.y + sideways.y * ndcX + upwards.y * ndcY,
                     facing.z + sideways.z * ndcX + upwards.z * ndcY};
    ray.direction = ray.direction.normalized();
    return ray;
}

void ViewportCamera::orbit(float deltaYaw, float deltaPitch) noexcept {
    yawRadians += deltaYaw;
    pitchRadians = std::clamp(pitchRadians + deltaPitch, -kMaxPitch, kMaxPitch);
}

void ViewportCamera::pan(float deltaX, float deltaY) noexcept {
    // Pan in the camera plane, scaled by distance so far scenes move faster.
    const math::Vec3f sideways = right();
    const math::Vec3f upwards = math::Vec3f::cross(sideways, forward());
    const float scale = distance * 0.0016F;
    target.x -= (sideways.x * deltaX - upwards.x * deltaY) * scale;
    target.y -= (sideways.y * deltaX - upwards.y * deltaY) * scale;
    target.z -= (sideways.z * deltaX - upwards.z * deltaY) * scale;
}

void ViewportCamera::dolly(float wheelDelta) noexcept {
    // wheelDelta is measured in wheel notches (1.0 per notch, negative when
    // scrolling back), so a fast flick zooms proportionally instead of one step.
    const float notches = std::clamp(wheelDelta, -4.0F, 4.0F);
    distance = std::clamp(distance * std::pow(0.9F, notches), 5.0F, 20000.0F);
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
