#pragma once

// Win32 + WGL viewport window. Depends on Win32/OpenGL by design; the pure
// camera + scene math above stays portable and unit-tested.
//
// Lifetime: create() makes a child window of the editor, destroy() tears the
// GL context down. setScene() copies oriented boxes in (cheap pointer-free
// structs). Input mirrors Java GalaxyRenderer: left-drag pan, right-drag
// orbit, wheel dolly, click select, Space frame selection.

#ifdef _WIN32

#include "whitehole/render/camera.hpp"
#include "whitehole/render/viewport_scene.hpp"

#include <functional>
#include <optional>
#include <windows.h>

namespace whitehole::render {

class ViewportWindow {
public:
    ViewportWindow();
    ~ViewportWindow();

    ViewportWindow(const ViewportWindow&) = delete;
    ViewportWindow& operator=(const ViewportWindow&) = delete;
    ViewportWindow(ViewportWindow&&) = delete;
    ViewportWindow& operator=(ViewportWindow&&) = delete;

    using SelectCallback = std::function<void(std::optional<std::size_t>)>;

    bool create(HWND parent, int controlId, HINSTANCE instance);
    void destroy() noexcept;
    [[nodiscard]] bool valid() const noexcept { return window_ != nullptr && glContext_ != nullptr; }
    [[nodiscard]] HWND handle() const noexcept { return window_; }

    void setScene(ViewportScene scene);
    void setSelected(std::optional<std::size_t> selected);
    void setHover(std::optional<std::size_t> hover);
    void frameAll();
    void frameSelection();
    void invalidate();

    void setOnSelect(SelectCallback callback) { onSelect_ = std::move(callback); }
    [[nodiscard]] ViewportCamera& camera() noexcept { return camera_; }

private:
    static LRESULT CALLBACK windowProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam);
    LRESULT handleMessage(UINT message, WPARAM wParam, LPARAM lParam);
    bool initGL();
    void shutdownGL() noexcept;
    void paint();
    void updateSize(int width, int height);
    void applyCameraToGL(int width, int height);
    void drawBox(const ViewportBox& box, bool selected, bool hovered);
    void drawGrid();
    std::optional<std::size_t> pickAt(int x, int y);

    HWND window_{nullptr};
    HDC device_{nullptr};
    HGLRC glContext_{nullptr};
    int width_{1};
    int height_{1};
    ViewportCamera camera_{};
    ViewportScene scene_{};
    std::optional<std::size_t> selected_;
    std::optional<std::size_t> hover_;
    SelectCallback onSelect_;
    bool draggingLeft_{false};
    bool draggingRight_{false};
    int lastX_{0};
    int lastY_{0};
    bool leftMoved_{false};
    int wheelAccumulator_{0};   // pending raw wheel deltas, applied in whole notches
    bool trackingMouse_{false}; // TrackMouseEvent armed so hover clears on leave
};

} // namespace whitehole::render

#endif // _WIN32
