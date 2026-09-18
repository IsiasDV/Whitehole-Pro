#pragma once

// Viewport camera: pure math, no Win32, no OpenGL.
// Mirrors the Java GalaxyRenderer defaults (FOV 70deg, near 0.01, far 1000)
// so future BMD/model renderers can reuse the exact same camera.

#include "whitehole/math/geometry.hpp"

namespace whitehole::render {

struct Ray {
    math::Vec3f origin{};
    math::Vec3f direction{0.0F, 0.0F, -1.0F};
};

class ViewportCamera {
public:
    ViewportCamera();

    // Orbit target (world units are raw SMG units / SCALE_DOWN like Java).
    math::Vec3f target{0.0F, 0.0F, 0.0F};
    float yawRadians{0.7853982F};   // 45 deg, matches Java start view
    float pitchRadians{0.5F};
    float distance{800.0F};

    static constexpr float kFieldOfView = 1.2217305F; // 70 deg in radians
    static constexpr float kNearPlane = 1.0F;
    static constexpr float kFarPlane = 60000.0F;

    [[nodiscard]] math::Vec3f eye() const noexcept;
    [[nodiscard]] math::Matrix4 viewMatrix() const noexcept;
    [[nodiscard]] math::Matrix4 projectionMatrix(float aspect) const noexcept;

    // Screen pixel -> world ray (origin at eye, direction normalized).
    // screenY grows downward like Win32/Java; height must be > 0.
    [[nodiscard]] Ray screenToRay(float screenX, float screenY, float width, float height) const noexcept;

    // Java parity controls: left-drag pans, right-drag orbits, wheel dollies.
    void orbit(float deltaYaw, float deltaPitch) noexcept;
    void pan(float deltaX, float deltaY) noexcept;
    void dolly(float wheelDelta) noexcept;
    void frameTarget(const math::Vec3f& point, float framedDistance = 300.0F) noexcept;

    // World -> screen pixel (for labels/overlays). Returns false when behind camera.
    [[nodiscard]] bool worldToScreen(const math::Vec3f& point, float width, float height, float& outX,
                                     float& outY) const noexcept;
};

} // namespace whitehole::render
