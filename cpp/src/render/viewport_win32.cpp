#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include "whitehole/render/viewport_win32.hpp"

#ifdef _WIN32

#include <windowsx.h>

#include <GL/gl.h>

#include <algorithm>
#include <cmath>

#pragma comment(lib, "opengl32.lib")

namespace whitehole::render {
namespace {

constexpr wchar_t kClassName[] = L"WhiteholeProViewport";
bool classRegistered = false;

void drawUnitCube() {
    glBegin(GL_QUADS);
    glVertex3f(-1, -1, 1);
    glVertex3f(1, -1, 1);
    glVertex3f(1, 1, 1);
    glVertex3f(-1, 1, 1);
    glVertex3f(1, -1, -1);
    glVertex3f(-1, -1, -1);
    glVertex3f(-1, 1, -1);
    glVertex3f(1, 1, -1);
    glVertex3f(-1, 1, 1);
    glVertex3f(1, 1, 1);
    glVertex3f(1, 1, -1);
    glVertex3f(-1, 1, -1);
    glVertex3f(-1, -1, -1);
    glVertex3f(1, -1, -1);
    glVertex3f(1, -1, 1);
    glVertex3f(-1, -1, 1);
    glVertex3f(1, -1, 1);
    glVertex3f(1, -1, -1);
    glVertex3f(1, 1, -1);
    glVertex3f(1, 1, 1);
    glVertex3f(-1, -1, 1);
    glVertex3f(-1, -1, -1);
    glVertex3f(-1, 1, -1);
    glVertex3f(-1, 1, 1);
    glEnd();
}

} // namespace

ViewportWindow::ViewportWindow() = default;
ViewportWindow::~ViewportWindow() {
    destroy();
}

bool ViewportWindow::create(HWND parent, int controlId, HINSTANCE instance) {
    destroy();
    if (!classRegistered) {
        WNDCLASSW cls{};
        cls.lpfnWndProc = &ViewportWindow::windowProc;
        cls.hInstance = instance;
        cls.lpszClassName = kClassName;
        cls.hCursor = LoadCursorW(nullptr, IDC_ARROW);
        cls.style = CS_OWNDC;
        if (RegisterClassW(&cls) == 0) {
            return false;
        }
        classRegistered = true;
    }
    window_ = CreateWindowExW(0, kClassName, L"", WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN, 0, 0, 10,
                              10, parent, reinterpret_cast<HMENU>(static_cast<INT_PTR>(controlId)), instance, this);
    if (window_ == nullptr) {
        return false;
    }
    if (!initGL()) {
        destroy();
        return false;
    }
    return true;
}

void ViewportWindow::destroy() noexcept {
    shutdownGL();
    if (window_ != nullptr) {
        DestroyWindow(window_);
        window_ = nullptr;
    }
}

void ViewportWindow::setScene(ViewportScene scene) {
    scene_ = std::move(scene);
    if (selected_.has_value() && *selected_ >= scene_.boxes().size()) {
        selected_.reset();
    }
    if (hover_.has_value() && *hover_ >= scene_.boxes().size()) {
        hover_.reset();
    }
    invalidate();
}

void ViewportWindow::setSelected(std::optional<std::size_t> selected) {
    selected_ = selected;
    invalidate();
}

void ViewportWindow::setHover(std::optional<std::size_t> hover) {
    hover_ = hover;
    invalidate();
}

void ViewportWindow::frameAll() {
    if (scene_.empty()) {
        return;
    }
    camera_.frameTarget(scene_.center(), scene_.frameDistance());
    invalidate();
}

void ViewportWindow::frameSelection() {
    if (!selected_.has_value() || *selected_ >= scene_.boxes().size()) {
        frameAll();
        return;
    }
    camera_.frameTarget(scene_.boxes()[*selected_].center, 300.0F);
    invalidate();
}

void ViewportWindow::invalidate() {
    if (window_ != nullptr) {
        InvalidateRect(window_, nullptr, FALSE);
    }
}

LRESULT CALLBACK ViewportWindow::windowProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam) {
    auto* self = reinterpret_cast<ViewportWindow*>(GetWindowLongPtrW(window, GWLP_USERDATA));
    if (message == WM_NCCREATE) {
        const auto* create = reinterpret_cast<CREATESTRUCTW*>(lParam);
        self = static_cast<ViewportWindow*>(create->lpCreateParams);
        SetWindowLongPtrW(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
        if (self != nullptr) {
            self->window_ = window;
        }
        return DefWindowProcW(window, message, wParam, lParam);
    }
    if (self != nullptr) {
        return self->handleMessage(message, wParam, lParam);
    }
    return DefWindowProcW(window, message, wParam, lParam);
}

LRESULT ViewportWindow::handleMessage(UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
    case WM_PAINT:
        paint();
        return 0;
    case WM_SIZE:
        updateSize(LOWORD(lParam), HIWORD(lParam));
        return 0;
    case WM_ERASEBKGND:
        return 1;
    case WM_LBUTTONDOWN:
        SetFocus(window_);
        SetCapture(window_);
        draggingLeft_ = true;
        leftMoved_ = false;
        lastX_ = GET_X_LPARAM(lParam);
        lastY_ = GET_Y_LPARAM(lParam);
        return 0;
    case WM_LBUTTONUP:
        if (draggingLeft_) {
            ReleaseCapture();
            draggingLeft_ = false;
            if (!leftMoved_ && (wParam & (MK_SHIFT | MK_CONTROL)) == 0) {
                if (onSelect_) {
                    onSelect_(pickAt(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)));
                }
            }
        }
        return 0;
    case WM_RBUTTONDOWN:
        SetFocus(window_);
        SetCapture(window_);
        draggingRight_ = true;
        lastX_ = GET_X_LPARAM(lParam);
        lastY_ = GET_Y_LPARAM(lParam);
        return 0;
    case WM_RBUTTONUP:
        if (draggingRight_) {
            ReleaseCapture();
            draggingRight_ = false;
        }
        return 0;
    case WM_MOUSEMOVE: {
        // Arm hover tracking so WM_MOUSELEAVE clears the highlight when the
        // cursor leaves the viewport (without this, a hovered box stays lit).
        if (!trackingMouse_ && window_ != nullptr) {
            TRACKMOUSEEVENT track{};
            track.cbSize = sizeof(track);
            track.dwFlags = TME_LEAVE;
            track.hwndTrack = window_;
            if (TrackMouseEvent(&track) != 0) {
                trackingMouse_ = true;
            }
        }
        const int x = GET_X_LPARAM(lParam);
        const int y = GET_Y_LPARAM(lParam);
        const int dx = x - lastX_;
        const int dy = y - lastY_;
        lastX_ = x;
        lastY_ = y;
        if (draggingLeft_ && (wParam & MK_LBUTTON) != 0) {
            if (dx != 0 || dy != 0) {
                leftMoved_ = true;
            }
            camera_.pan(static_cast<float>(dx), static_cast<float>(dy));
            invalidate();
        } else if (draggingRight_ && (wParam & MK_RBUTTON) != 0) {
            camera_.orbit(static_cast<float>(dx) * 0.008F, static_cast<float>(dy) * 0.008F);
            invalidate();
        } else if (!draggingLeft_ && !draggingRight_) {
            const auto hovered = pickAt(x, y);
            if (hovered != hover_) {
                hover_ = hovered;
                invalidate();
            }
        }
        return 0;
    }
    case WM_MOUSELEAVE:
        trackingMouse_ = false;
        if (hover_.has_value()) {
            hover_.reset();
            invalidate();
        }
        return 0;
    case WM_MOUSEWHEEL: {
        // Accumulate raw deltas and apply whole notches, so high-resolution
        // wheels and multi-notch flicks zoom smoothly instead of losing input.
        wheelAccumulator_ += GET_WHEEL_DELTA_WPARAM(wParam);
        int notches = 0;
        while (wheelAccumulator_ >= WHEEL_DELTA) {
            ++notches;
            wheelAccumulator_ -= WHEEL_DELTA;
        }
        while (wheelAccumulator_ <= -WHEEL_DELTA) {
            --notches;
            wheelAccumulator_ += WHEEL_DELTA;
        }
        if (notches != 0) {
            camera_.dolly(static_cast<float>(notches));
            invalidate();
        }
        return 0;
    }
    case WM_KEYDOWN:
        if (wParam == VK_SPACE) {
            frameSelection();
            return 0;
        }
        break;
    default:
        break;
    }
    return DefWindowProcW(window_, message, wParam, lParam);
}

bool ViewportWindow::initGL() {
    device_ = GetDC(window_);
    if (device_ == nullptr) {
        return false;
    }
    PIXELFORMATDESCRIPTOR format{};
    format.nSize = sizeof(format);
    format.nVersion = 1;
    format.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
    format.iPixelType = PFD_TYPE_RGBA;
    format.cColorBits = 32;
    format.cDepthBits = 24;
    format.iLayerType = PFD_MAIN_PLANE;
    const int pixel = ChoosePixelFormat(device_, &format);
    if (pixel == 0 || SetPixelFormat(device_, pixel, &format) == FALSE) {
        ReleaseDC(window_, device_);
        device_ = nullptr;
        return false;
    }
    glContext_ = wglCreateContext(device_);
    if (glContext_ == nullptr) {
        ReleaseDC(window_, device_);
        device_ = nullptr;
        return false;
    }
    RECT rect{};
    GetClientRect(window_, &rect);
    width_ = rect.right - rect.left > 0 ? rect.right - rect.left : 1;
    height_ = rect.bottom - rect.top > 0 ? rect.bottom - rect.top : 1;
    return true;
}

void ViewportWindow::shutdownGL() noexcept {
    if (glContext_ != nullptr) {
        wglMakeCurrent(nullptr, nullptr);
        wglDeleteContext(glContext_);
        glContext_ = nullptr;
    }
    if (device_ != nullptr && window_ != nullptr) {
        ReleaseDC(window_, device_);
        device_ = nullptr;
    }
}

void ViewportWindow::paint() {
    PAINTSTRUCT paintInfo{};
    BeginPaint(window_, &paintInfo);
    if (glContext_ != nullptr && device_ != nullptr) {
        wglMakeCurrent(device_, glContext_);
        applyCameraToGL(width_, height_);
        drawGrid();
        glEnable(GL_DEPTH_TEST);
        for (const auto& box : scene_.boxes()) {
            const bool selected = selected_.has_value() && *selected_ == box.objectIndex;
            const bool hovered = !selected && hover_.has_value() && *hover_ == box.objectIndex;
            drawBox(box, selected, hovered);
        }
        if (selected_.has_value() && *selected_ < scene_.boxes().size()) {
            const auto& box = scene_.boxes()[*selected_];
            glDisable(GL_DEPTH_TEST);
            glLineWidth(2.0F);
            glBegin(GL_LINES);
            glColor3f(1, 0.2F, 0.2F);
            glVertex3f(box.center.x - 60, box.center.y, box.center.z);
            glVertex3f(box.center.x + 60, box.center.y, box.center.z);
            glColor3f(0.2F, 1, 0.2F);
            glVertex3f(box.center.x, box.center.y - 60, box.center.z);
            glVertex3f(box.center.x, box.center.y + 60, box.center.z);
            glColor3f(0.3F, 0.5F, 1);
            glVertex3f(box.center.x, box.center.y, box.center.z - 60);
            glVertex3f(box.center.x, box.center.y, box.center.z + 60);
            glEnd();
            glLineWidth(1.0F);
            glEnable(GL_DEPTH_TEST);
        }
        SwapBuffers(device_);
        wglMakeCurrent(nullptr, nullptr);
    }
    EndPaint(window_, &paintInfo);
}

void ViewportWindow::updateSize(int width, int height) {
    width_ = width > 0 ? width : 1;
    height_ = height > 0 ? height : 1;
    if (window_ != nullptr) {
        invalidate();
    }
}

void ViewportWindow::applyCameraToGL(int width, int height) {
    const float safeHeight = static_cast<float>(height > 0 ? height : 1);
    const float aspect = static_cast<float>(width) / safeHeight;
    glViewport(0, 0, width, height);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    const float half = ViewportCamera::kFieldOfView * 0.5F;
    const float tanHalf = static_cast<float>(std::tan(static_cast<double>(half)));
    const float nearPlane = ViewportCamera::kNearPlane;
    const float farPlane = ViewportCamera::kFarPlane;
    const float top = tanHalf * nearPlane;
    glFrustum(-top * aspect, top * aspect, -top, top, nearPlane, farPlane);
    glMatrixMode(GL_MODELVIEW);
    // Matrix4 keeps element (row, column) at values[4 * row + column], which is
    // exactly the column-major layout glLoadMatrixf expects, so the rendered
    // view is literally the CPU camera: screen picks can never drift from what
    // is drawn (the previous hand-built look matrix was a second copy of this
    // maths that had to be kept in sync by hand).
    const math::Matrix4 view = camera_.viewMatrix();
    glLoadMatrixf(view.values.data());
    glClearColor(0.09F, 0.11F, 0.15F, 1.0F);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}

void ViewportWindow::drawBox(const ViewportBox& box, bool selected, bool hovered) {
    glPushMatrix();
    glMultMatrixf(box.world.values.data());
    if (selected) {
        glColor3f(1.0F, 0.85F, 0.2F);
    } else if (hovered) {
        glColor3f(0.4F, 0.8F, 1.0F);
    } else {
        glColor3f(0.55F, 0.65F, 0.8F);
    }
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    glEnable(GL_POLYGON_OFFSET_FILL);
    glPolygonOffset(1.0F, 1.0F);
    drawUnitCube();
    glDisable(GL_POLYGON_OFFSET_FILL);
    if (selected || hovered) {
        glColor3f(1, 1, 1);
    } else {
        glColor3f(0.15F, 0.2F, 0.3F);
    }
    glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
    drawUnitCube();
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    glPopMatrix();
}

void ViewportWindow::drawGrid() {
    // The grid follows the camera: the step scales with the orbit distance and
    // the patch stays centred (and snapped) on the orbit target, so panning
    // across a galaxy never scrolls the grid out of view.
    const float step = std::clamp(camera_.distance / 20.0F, 25.0F, 2000.0F);
    constexpr int halfLines = 20;
    const float extent = step * static_cast<float>(halfLines);
    const float centerX = std::floor(camera_.target.x / step + 0.5F) * step;
    const float centerZ = std::floor(camera_.target.z / step + 0.5F) * step;

    glDisable(GL_DEPTH_TEST);
    glBegin(GL_LINES);
    glColor3f(0.22F, 0.26F, 0.33F);
    for (int i = -halfLines; i <= halfLines; ++i) {
        const float offset = static_cast<float>(i) * step;
        glVertex3f(centerX + offset, 0.0F, centerZ - extent);
        glVertex3f(centerX + offset, 0.0F, centerZ + extent);
        glVertex3f(centerX - extent, 0.0F, centerZ + offset);
        glVertex3f(centerX + extent, 0.0F, centerZ + offset);
    }
    // World axes, drawn across the visible patch so they stay readable.
    glColor3f(0.9F, 0.25F, 0.25F);
    glVertex3f(centerX - extent, 0.0F, 0.0F);
    glVertex3f(centerX + extent, 0.0F, 0.0F);
    glColor3f(0.3F, 0.9F, 0.3F);
    glVertex3f(0.0F, -extent, 0.0F);
    glVertex3f(0.0F, extent, 0.0F);
    glColor3f(0.3F, 0.5F, 1.0F);
    glVertex3f(0.0F, 0.0F, centerZ - extent);
    glVertex3f(0.0F, 0.0F, centerZ + extent);
    glEnd();
    glEnable(GL_DEPTH_TEST);
}

std::optional<std::size_t> ViewportWindow::pickAt(int x, int y) {
    RECT rect{};
    GetClientRect(window_, &rect);
    const float width = static_cast<float>(rect.right - rect.left);
    const float height = static_cast<float>(rect.bottom - rect.top);
    return scene_.pick(camera_, static_cast<float>(x), static_cast<float>(y), width, height);
}
} // namespace whitehole::render
#endif // _WIN32
