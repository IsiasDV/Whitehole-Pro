#pragma once

// Viewport camera: pure math, no Win32, no OpenGL.
// Mirrors the Java GalaxyRenderer camera (FOV 70deg, orbit eye/target maths,
// pan and orbit directions) so BMD/model renderers can reuse it unchanged.
// Java renders at 1/SCALE_DOWN with Z_NEAR 0.01 / Z_FAR 1000; here the camera
// works in raw SMG units with the equivalent near/far below.

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
    // Raw-unit equivalents of the Java frustum (0.01 / 1000 in SCALE_DOWN
    // space). 1.0 near keeps close objects clickable, 60000 far covers the
    // largest galaxies plus their frame distances with 24-bit depth.
    static constexpr float kNearPlane = 1.0F;
    static constexpr float kFarPlane = 60000.0F;

    // Pitch is clamped just short of straight up/down so up()/right() stay
    // well conditioned (matches the Java editor, which never rolls).
    static constexpr float kMaxPitch = 1.55F;

    [[nodiscard]] math::Vec3f eye() const noexcept;
    [[nodiscard]] math::Matrix4 viewMatrix() const noexcept;
    [[nodiscard]] math::Matrix4 projectionMatrix(float aspect) const noexcept;

    // Camera basis. Single source of truth for the view matrix, the picking
    // ray, panning and the OpenGL renderer, so screen picks always agree with
    // what is drawn. `up` mirrors the Java renderer: it flips to -Y once the
    // pitch passes straight down/up (cos(pitch) < 0), and pitch is clamped
    // before that so the basis never degenerates or rolls.
    [[nodiscard]] math::Vec3f forward() const noexcept;
    [[nodiscard]] math::Vec3f up() const noexcept;
    [[nodiscard]] math::Vec3f right() const noexcept;

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
