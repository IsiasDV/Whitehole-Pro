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

#include "whitehole/app/application.hpp"

#include "whitehole/db/name_table.hpp"
#include "whitehole/render/viewport_scene.hpp"
#include "whitehole/render/viewport_win32.hpp"
#include "whitehole/smg/game_archive.hpp"
#include "whitehole/smg/stage_archive.hpp"

#include <windows.h>
#include <commctrl.h>
#include <commdlg.h>
#include <shellapi.h>
#include <shobjidl.h>

#include <algorithm>
#include <array>
#include <charconv>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "uuid.lib")
#pragma comment(lib, "shell32.lib")

namespace whitehole::app {
namespace {

constexpr int kIdOpenGame = 1001;
constexpr int kIdOpenMap = 1002;
constexpr int kIdSave = 1003;
constexpr int kIdExit = 1004;
constexpr int kIdGalaxies = 1101;
constexpr int kIdZones = 1102;
constexpr int kIdObjects = 1103;
constexpr int kIdName = 1104;
constexpr int kIdPosX = 1105;
constexpr int kIdPosY = 1106;
constexpr int kIdPosZ = 1107;
constexpr int kIdRotX = 1108;
constexpr int kIdRotY = 1109;
constexpr int kIdRotZ = 1110;
constexpr int kIdScaleX = 1111;
constexpr int kIdScaleY = 1112;
constexpr int kIdScaleZ = 1113;
constexpr int kIdApply = 1120;
constexpr int kIdStatus = 1121;
constexpr int kIdViewport = 1122;

std::wstring utf8ToWide(std::string_view text) {
    if (text.empty()) {
        return {};
    }
    const auto size = MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), nullptr, 0);
    std::wstring result(static_cast<std::size_t>(size), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), result.data(), size);
    return result;
}

std::string wideToUtf8(std::wstring_view text) {
    if (text.empty()) {
        return {};
    }
    const auto size = WideCharToMultiByte(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), nullptr, 0, nullptr, nullptr);
    std::string result(static_cast<std::size_t>(size), '\0');
    WideCharToMultiByte(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), result.data(), size, nullptr, nullptr);
    return result;
}

std::string windowText(HWND window) {
    const auto length = GetWindowTextLengthW(window);
    std::wstring text(static_cast<std::size_t>(length), L'\0');
    GetWindowTextW(window, text.data(), length + 1);
    return wideToUtf8(text);
}

void setWindowText(HWND window, std::string_view text) {
    SetWindowTextW(window, utf8ToWide(text).c_str());
}

float parseFloat(HWND window, float fallback) {
    try {
        return std::stof(windowText(window));
    } catch (...) {
        return fallback;
    }
}

std::string formatFloat(float value) {
    std::array<char, 32> buffer{};
    const auto converted = std::to_chars(buffer.data(), buffer.data() + buffer.size(), value);
    return std::string(buffer.data(), converted.ptr);
}

struct EditorState {
    std::filesystem::path dataRoot;
    db::NameTable galaxyNames;
    db::NameTable zoneNames;
    std::optional<smg::GameArchive> game;
    std::vector<std::string> galaxies;
    std::vector<std::string> zones;
    std::optional<smg::StageArchive> stage;
    HWND galaxiesList{nullptr};
    HWND zonesList{nullptr};
    HWND objectsList{nullptr};
    HWND nameEdit{nullptr};
    HWND posX{nullptr};
    HWND posY{nullptr};
    HWND posZ{nullptr};
    HWND rotX{nullptr};
    HWND rotY{nullptr};
    HWND rotZ{nullptr};
    HWND scaleX{nullptr};
    HWND scaleY{nullptr};
    HWND scaleZ{nullptr};
    HWND applyButton{nullptr};
    HWND galaxiesLabel{nullptr};
    HWND zonesLabel{nullptr};
    HWND objectsLabel{nullptr};
    HWND nameLabel{nullptr};
    HWND positionLabel{nullptr};
    HWND rotationLabel{nullptr};
    HWND scaleLabel{nullptr};
    HWND status{nullptr};
    HWND hintLabel{nullptr};
    render::ViewportWindow viewport;
    render::ViewportScene viewportScene;
    std::optional<std::size_t> viewportSelected;
    bool viewportReady{false};
    bool syncingSelection{false};
};

// Keeps every control anchored while the window is resized: three list columns
// on top, the 3D viewport in the middle, fixed transform rows at the bottom.
void layoutEditor(EditorState& state, HWND window) {
    RECT client{};
    GetClientRect(window, &client);
    const auto width = static_cast<int>(client.right);
    const auto height = static_cast<int>(client.bottom);

    constexpr int margin = 12;
    constexpr int rowHeight = 24;
    constexpr int rowGap = 36;
    constexpr int statusHeight = 22;

    const auto statusTop = height - margin - statusHeight;
    const auto editorTop = statusTop - rowGap - rowHeight - (3 * rowGap);
    const auto columnWidth = std::max(150, (width - margin * 4) / 3);
    const auto listTop = margin + 20;
    // Reserve the middle band for the 3D viewport; lists keep a usable height
    // while the viewport takes whatever vertical space is left.
    constexpr int listHeight = 148;
    const auto viewportTop = listTop + listHeight + 8;
    const auto viewportHeight = std::max(140, editorTop - 26 - viewportTop);

    const auto secondX = margin * 2 + columnWidth;
    const auto thirdX = margin * 3 + columnWidth * 2;
    const auto thirdWidth = std::max(150, width - thirdX - margin);

    MoveWindow(state.galaxiesLabel, margin, margin, columnWidth, 18, TRUE);
    MoveWindow(state.galaxiesList, margin, listTop, columnWidth, listHeight, TRUE);
    MoveWindow(state.zonesLabel, secondX, margin, columnWidth, 18, TRUE);
    MoveWindow(state.zonesList, secondX, listTop, columnWidth, listHeight, TRUE);
    MoveWindow(state.objectsLabel, thirdX, margin, thirdWidth, 18, TRUE);
    MoveWindow(state.objectsList, thirdX, listTop, thirdWidth, listHeight, TRUE);

    const auto nameWidth = std::clamp(secondX - 76, 140, 224);
    MoveWindow(state.nameLabel, margin, editorTop + 4, 50, 18, TRUE);
    MoveWindow(state.nameEdit, 64, editorTop, nameWidth, rowHeight, TRUE);
    MoveWindow(state.positionLabel, 300, editorTop + rowGap + 4, 70, 18, TRUE);
    MoveWindow(state.rotationLabel, 300, editorTop + (2 * rowGap) + 4, 70, 18, TRUE);
    MoveWindow(state.scaleLabel, 300, editorTop + (3 * rowGap) + 4, 70, 18, TRUE);

    constexpr int valueColumns[3] = {370, 456, 542};
    const HWND positionEdits[3] = {state.posX, state.posY, state.posZ};
    const HWND rotationEdits[3] = {state.rotX, state.rotY, state.rotZ};
    const HWND scaleEdits[3] = {state.scaleX, state.scaleY, state.scaleZ};
    for (int column = 0; column < 3; ++column) {
        MoveWindow(positionEdits[column], valueColumns[column], editorTop + rowGap, 80, rowHeight, TRUE);
        MoveWindow(rotationEdits[column], valueColumns[column], editorTop + (2 * rowGap), 80, rowHeight, TRUE);
        MoveWindow(scaleEdits[column], valueColumns[column], editorTop + (3 * rowGap), 80, rowHeight, TRUE);
    }
    MoveWindow(state.applyButton, 640, editorTop + rowGap, 90, 28, TRUE);
    MoveWindow(state.status, margin, statusTop, width - margin * 2, statusHeight, TRUE);
    MoveWindow(state.hintLabel, margin, viewportTop - 2, width - margin * 2, 18, TRUE);
    if (state.viewport.handle() != nullptr) {
        MoveWindow(state.viewport.handle(), margin, viewportTop + 18, width - margin * 2, viewportHeight, TRUE);
    }
}

std::optional<std::filesystem::path> pickFolder(HWND owner) {
    IFileDialog* dialog = nullptr;
    if (FAILED(CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&dialog)))) {
        return std::nullopt;
    }
    DWORD options = 0;
    dialog->GetOptions(&options);
    dialog->SetOptions(options | FOS_PICKFOLDERS | FOS_FORCEFILESYSTEM);
    std::optional<std::filesystem::path> result;
    if (SUCCEEDED(dialog->Show(owner))) {
        IShellItem* item = nullptr;
        if (SUCCEEDED(dialog->GetResult(&item))) {
            PWSTR path = nullptr;
            if (SUCCEEDED(item->GetDisplayName(SIGDN_FILESYSPATH, &path))) {
                result = std::filesystem::path(path);
                CoTaskMemFree(path);
            }
            item->Release();
        }
    }
    dialog->Release();
    return result;
}

std::optional<std::filesystem::path> pickOpenFile(HWND owner) {
    wchar_t file[MAX_PATH]{};
    OPENFILENAMEW info{};
    info.lStructSize = sizeof(info);
    info.hwndOwner = owner;
    info.lpstrFile = file;
    info.nMaxFile = MAX_PATH;
    info.lpstrFilter = L"RARC Archives (*.arc;*.szs)\0*.arc;*.szs\0All Files\0*.*\0";
    info.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;
    if (!GetOpenFileNameW(&info)) {
        return std::nullopt;
    }
    return std::filesystem::path(file);
}

void setStatus(EditorState& state, std::string_view text) {
    setWindowText(state.status, text);
}

void clearList(HWND list) {
    SendMessageW(list, LB_RESETCONTENT, 0, 0);
}

void fillList(HWND list, const std::vector<std::string>& items, const db::NameTable* names) {
    clearList(list);
    for (const auto& item : items) {
        const auto label = names != nullptr ? names->displayName(item) + "  [" + item + "]" : item;
        SendMessageW(list, LB_ADDSTRING, 0, reinterpret_cast<LPARAM>(utf8ToWide(label).c_str()));
    }
}

void refreshObjects(EditorState& state) {
    clearList(state.objectsList);
    if (!state.stage) {
        return;
    }
    for (const auto& object : state.stage->objects()) {
        const auto label = object.kind + "/" + object.layer + "  " + object.name;
        SendMessageW(state.objectsList, LB_ADDSTRING, 0, reinterpret_cast<LPARAM>(utf8ToWide(label).c_str()));
    }
}

void showObject(EditorState& state, int index);
void syncViewportSelection(EditorState& state, std::optional<std::size_t> selected);

void refreshViewport(EditorState& state, bool frame) {
    if (!state.viewportReady) {
        return;
    }
    if (state.stage) {
        state.viewportScene.rebuild(state.stage->objects());
    } else {
        state.viewportScene.clear();
    }
    state.viewport.setScene(state.viewportScene);
    if (!state.stage || state.stage->objects().empty()) {
        state.viewportSelected.reset();
        state.viewport.setSelected(std::nullopt);
        return;
    }
    if (state.viewportSelected.has_value() && *state.viewportSelected >= state.stage->objects().size()) {
        state.viewportSelected.reset();
    }
    state.viewport.setSelected(state.viewportSelected);
    if (frame) {
        state.viewport.frameAll();
    } else {
        state.viewport.invalidate();
    }
}

void showObject(EditorState& state, int index) {
    if (!state.stage || index < 0 || static_cast<std::size_t>(index) >= state.stage->objects().size()) {
        return;
    }
    const auto& object = state.stage->objects()[static_cast<std::size_t>(index)];
    setWindowText(state.nameEdit, object.name);
    setWindowText(state.posX, formatFloat(object.position.x));
    setWindowText(state.posY, formatFloat(object.position.y));
    setWindowText(state.posZ, formatFloat(object.position.z));
    setWindowText(state.rotX, formatFloat(object.rotation.x));
    setWindowText(state.rotY, formatFloat(object.rotation.y));
    setWindowText(state.rotZ, formatFloat(object.rotation.z));
    setWindowText(state.scaleX, formatFloat(object.scale.x));
    setWindowText(state.scaleY, formatFloat(object.scale.y));
    setWindowText(state.scaleZ, formatFloat(object.scale.z));
}

void applyObject(EditorState& state) {
    const auto index = static_cast<int>(SendMessageW(state.objectsList, LB_GETCURSEL, 0, 0));
    if (!state.stage || index < 0 || static_cast<std::size_t>(index) >= state.stage->objects().size()) {
        return;
    }
    auto& object = state.stage->objects()[static_cast<std::size_t>(index)];
    object.name = windowText(state.nameEdit);
    object.position.x = parseFloat(state.posX, object.position.x);
    object.position.y = parseFloat(state.posY, object.position.y);
    object.position.z = parseFloat(state.posZ, object.position.z);
    object.rotation.x = parseFloat(state.rotX, object.rotation.x);
    object.rotation.y = parseFloat(state.rotY, object.rotation.y);
    object.rotation.z = parseFloat(state.rotZ, object.rotation.z);
    object.scale.x = parseFloat(state.scaleX, object.scale.x);
    object.scale.y = parseFloat(state.scaleY, object.scale.y);
    object.scale.z = parseFloat(state.scaleZ, object.scale.z);
    setStatus(state, "Updated " + object.name + " in memory. Save the zone to write the archive.");
    refreshObjects(state);
    SendMessageW(state.objectsList, LB_SETCURSEL, static_cast<WPARAM>(index), 0);
    state.viewportSelected = static_cast<std::size_t>(index);
    if (state.viewportReady) {
        state.viewport.setSelected(state.viewportSelected);
    }
    refreshViewport(state, false);
}

void syncViewportSelection(EditorState& state, std::optional<std::size_t> selected) {
    state.viewportSelected = selected;
    if (state.viewportReady) {
        state.viewport.setSelected(selected);
    }
    if (state.syncingSelection) {
        return;
    }
    if (selected.has_value() && state.stage && *selected < state.stage->objects().size()) {
        state.syncingSelection = true;
        SendMessageW(state.objectsList, LB_SETCURSEL, static_cast<WPARAM>(*selected), 0);
        showObject(state, static_cast<int>(*selected));
        state.syncingSelection = false;
    }
}

void openMap(EditorState& state, const std::filesystem::path& path) {
    state.stage = smg::StageArchive::openMapFile(path);
    state.zones = {state.stage->stageName()};
    fillList(state.zonesList, state.zones, nullptr);
    refreshObjects(state);
    syncViewportSelection(state, std::nullopt);
    refreshViewport(state, true);
    setStatus(state, "Opened map archive with " + std::to_string(state.stage->objects().size()) + " objects.");
}

void openGame(EditorState& state, const std::filesystem::path& path) {
    state.game.emplace(path);
    if (state.game->gameType() == 0) {
        state.game.reset();
        throw std::runtime_error("That folder is not an SMG1/SMG2 workspace");
    }
    state.galaxies = state.game->galaxies();
    state.zones = state.game->zones();
    state.stage.reset();
    fillList(state.galaxiesList, state.galaxies, &state.galaxyNames);
    fillList(state.zonesList, state.zones, &state.zoneNames);
    clearList(state.objectsList);
    syncViewportSelection(state, std::nullopt);
    refreshViewport(state, false);
    setStatus(state, "Opened SMG" + std::to_string(state.game->gameType()) + " workspace with "
                         + std::to_string(state.galaxies.size()) + " galaxies.");
}

void selectGalaxy(EditorState& state) {
    const auto index = static_cast<int>(SendMessageW(state.galaxiesList, LB_GETCURSEL, 0, 0));
    if (!state.game || index < 0 || static_cast<std::size_t>(index) >= state.galaxies.size()) {
        return;
    }
    const auto galaxy = state.game->openGalaxy(state.galaxies[static_cast<std::size_t>(index)]);
    state.zones = galaxy.zones();
    fillList(state.zonesList, state.zones, &state.zoneNames);
    setStatus(state, "Galaxy " + galaxy.name() + " has " + std::to_string(state.zones.size()) + " zones.");
}

void selectZone(EditorState& state) {
    const auto index = static_cast<int>(SendMessageW(state.zonesList, LB_GETCURSEL, 0, 0));
    if (index < 0 || static_cast<std::size_t>(index) >= state.zones.size()) {
        return;
    }
    const auto& zone = state.zones[static_cast<std::size_t>(index)];
    if (state.game) {
        state.stage = smg::StageArchive::open(state.game->filesystem(), zone, state.game->gameType());
    }
    refreshObjects(state);
    syncViewportSelection(state, std::nullopt);
    refreshViewport(state, true);
    if (state.stage) {
        setStatus(state, "Loaded " + zone + " (" + std::to_string(state.stage->objects().size()) + " objects).");
    }
}

void saveStage(EditorState& state) {
    if (!state.stage) {
        throw std::runtime_error("No zone is loaded");
    }
    applyObject(state);
    state.stage->save();
    setStatus(state, "Saved " + state.stage->sourcePath().string());
}

LRESULT CALLBACK editorProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam) {
    auto* state = reinterpret_cast<EditorState*>(GetWindowLongPtrW(window, GWLP_USERDATA));
    switch (message) {
    case WM_CREATE: {
        auto* created = new EditorState();
        SetWindowLongPtrW(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(created));
        state = created;
        created->galaxiesLabel = CreateWindowW(L"STATIC", L"Galaxies", WS_CHILD | WS_VISIBLE, 12, 12, 240, 18, window, nullptr, nullptr, nullptr);
        created->galaxiesList = CreateWindowW(L"LISTBOX", L"", WS_CHILD | WS_VISIBLE | WS_BORDER | WS_VSCROLL | LBS_NOTIFY,
                                              12, 32, 240, 360, window, reinterpret_cast<HMENU>(static_cast<INT_PTR>(kIdGalaxies)), nullptr, nullptr);
        created->zonesLabel = CreateWindowW(L"STATIC", L"Zones", WS_CHILD | WS_VISIBLE, 264, 12, 240, 18, window, nullptr, nullptr, nullptr);
        created->zonesList = CreateWindowW(L"LISTBOX", L"", WS_CHILD | WS_VISIBLE | WS_BORDER | WS_VSCROLL | LBS_NOTIFY,
                                           264, 32, 240, 360, window, reinterpret_cast<HMENU>(static_cast<INT_PTR>(kIdZones)), nullptr, nullptr);
        created->objectsLabel = CreateWindowW(L"STATIC", L"Objects", WS_CHILD | WS_VISIBLE, 516, 12, 360, 18, window, nullptr, nullptr, nullptr);
        created->objectsList = CreateWindowW(L"LISTBOX", L"", WS_CHILD | WS_VISIBLE | WS_BORDER | WS_VSCROLL | LBS_NOTIFY,
                                             516, 32, 360, 360, window, reinterpret_cast<HMENU>(static_cast<INT_PTR>(kIdObjects)), nullptr, nullptr);

        created->nameLabel = CreateWindowW(L"STATIC", L"Name", WS_CHILD | WS_VISIBLE, 12, 404, 50, 18, window, nullptr, nullptr, nullptr);
        created->nameEdit = CreateWindowW(L"EDIT", L"", WS_CHILD | WS_VISIBLE | WS_BORDER, 64, 400, 220, 24, window,
                                          reinterpret_cast<HMENU>(static_cast<INT_PTR>(kIdName)), nullptr, nullptr);
        created->positionLabel = CreateWindowW(L"STATIC", L"Position", WS_CHILD | WS_VISIBLE, 300, 404, 70, 18, window, nullptr, nullptr, nullptr);
        created->posX = CreateWindowW(L"EDIT", L"0", WS_CHILD | WS_VISIBLE | WS_BORDER, 370, 400, 80, 24, window,
                                      reinterpret_cast<HMENU>(static_cast<INT_PTR>(kIdPosX)), nullptr, nullptr);
        created->posY = CreateWindowW(L"EDIT", L"0", WS_CHILD | WS_VISIBLE | WS_BORDER, 456, 400, 80, 24, window,
                                      reinterpret_cast<HMENU>(static_cast<INT_PTR>(kIdPosY)), nullptr, nullptr);
        created->posZ = CreateWindowW(L"EDIT", L"0", WS_CHILD | WS_VISIBLE | WS_BORDER, 542, 400, 80, 24, window,
                                      reinterpret_cast<HMENU>(static_cast<INT_PTR>(kIdPosZ)), nullptr, nullptr);
        created->rotationLabel = CreateWindowW(L"STATIC", L"Rotation", WS_CHILD | WS_VISIBLE, 300, 436, 70, 18, window, nullptr, nullptr, nullptr);
        created->rotX = CreateWindowW(L"EDIT", L"0", WS_CHILD | WS_VISIBLE | WS_BORDER, 370, 432, 80, 24, window,
                                      reinterpret_cast<HMENU>(static_cast<INT_PTR>(kIdRotX)), nullptr, nullptr);
        created->rotY = CreateWindowW(L"EDIT", L"0", WS_CHILD | WS_VISIBLE | WS_BORDER, 456, 432, 80, 24, window,
                                      reinterpret_cast<HMENU>(static_cast<INT_PTR>(kIdRotY)), nullptr, nullptr);
        created->rotZ = CreateWindowW(L"EDIT", L"0", WS_CHILD | WS_VISIBLE | WS_BORDER, 542, 432, 80, 24, window,
                                      reinterpret_cast<HMENU>(static_cast<INT_PTR>(kIdRotZ)), nullptr, nullptr);
        created->scaleLabel = CreateWindowW(L"STATIC", L"Scale", WS_CHILD | WS_VISIBLE, 300, 468, 70, 18, window, nullptr, nullptr, nullptr);
        created->scaleX = CreateWindowW(L"EDIT", L"1", WS_CHILD | WS_VISIBLE | WS_BORDER, 370, 464, 80, 24, window,
                                        reinterpret_cast<HMENU>(static_cast<INT_PTR>(kIdScaleX)), nullptr, nullptr);
        created->scaleY = CreateWindowW(L"EDIT", L"1", WS_CHILD | WS_VISIBLE | WS_BORDER, 456, 464, 80, 24, window,
                                        reinterpret_cast<HMENU>(static_cast<INT_PTR>(kIdScaleY)), nullptr, nullptr);
        created->scaleZ = CreateWindowW(L"EDIT", L"1", WS_CHILD | WS_VISIBLE | WS_BORDER, 542, 464, 80, 24, window,
                                        reinterpret_cast<HMENU>(static_cast<INT_PTR>(kIdScaleZ)), nullptr, nullptr);
        created->applyButton = CreateWindowW(L"BUTTON", L"Apply", WS_CHILD | WS_VISIBLE, 640, 400, 90, 28, window,
                      reinterpret_cast<HMENU>(static_cast<INT_PTR>(kIdApply)), nullptr, nullptr);
        created->hintLabel =
            CreateWindowW(L"STATIC", L"3D view: left-drag pans, right-drag orbits, wheel zooms, click selects, Space frames.",
                          WS_CHILD | WS_VISIBLE, 12, 200, 860, 18, window, nullptr, nullptr, nullptr);
        created->status = CreateWindowW(L"STATIC", L"Open a game folder or a map archive to begin.",
                                        WS_CHILD | WS_VISIBLE, 12, 504, 860, 22, window,
                                        reinterpret_cast<HMENU>(static_cast<INT_PTR>(kIdStatus)), nullptr, nullptr);
        created->viewportReady =
            created->viewport.create(window, kIdViewport, GetModuleHandleW(nullptr));
        if (created->viewportReady) {
            created->viewport.setOnSelect([created](std::optional<std::size_t> selected) {
                syncViewportSelection(*created, selected);
            });
            refreshViewport(*created, false);
        } else {
            setStatus(*created, "3D viewport unavailable (OpenGL init failed); list editing still works.");
        }
        DragAcceptFiles(window, TRUE);
        layoutEditor(*created, window);
        return 0;
    }
    case WM_SIZE:
        if (state != nullptr && wParam != SIZE_MINIMIZED) {
            layoutEditor(*state, window);
        }
        return 0;
    case WM_GETMINMAXINFO: {
        auto* limits = reinterpret_cast<MINMAXINFO*>(lParam);
        limits->ptMinTrackSize.x = 780;
        limits->ptMinTrackSize.y = 720;
        return 0;
    }
    case WM_COMMAND:
        if (state == nullptr) {
            break;
        }
        try {
            switch (LOWORD(wParam)) {
            case kIdOpenGame: {
                const auto folder = pickFolder(window);
                if (folder) {
                    openGame(*state, *folder);
                }
                break;
            }
            case kIdOpenMap: {
                const auto file = pickOpenFile(window);
                if (file) {
                    openMap(*state, *file);
                }
                break;
            }
            case kIdSave:
                saveStage(*state);
                break;
            case kIdApply:
                applyObject(*state);
                break;
            case kIdExit:
                DestroyWindow(window);
                break;
            case kIdGalaxies:
                if (HIWORD(wParam) == LBN_SELCHANGE) {
                    selectGalaxy(*state);
                }
                break;
            case kIdZones:
                if (HIWORD(wParam) == LBN_SELCHANGE) {
                    selectZone(*state);
                }
                break;
            case kIdObjects:
                if (HIWORD(wParam) == LBN_SELCHANGE && !state->syncingSelection) {
                    const auto selected = static_cast<int>(SendMessageW(state->objectsList, LB_GETCURSEL, 0, 0));
                    showObject(*state, selected);
                    if (selected >= 0) {
                        syncViewportSelection(*state, static_cast<std::size_t>(selected));
                    } else {
                        syncViewportSelection(*state, std::nullopt);
                    }
                }
                break;
            default:
                break;
            }
        } catch (const std::exception& error) {
            setStatus(*state, error.what());
            MessageBoxW(window, utf8ToWide(error.what()).c_str(), L"Whitehole Pro", MB_ICONERROR);
        }
        return 0;
    case WM_DROPFILES: {
        const auto drop = reinterpret_cast<HDROP>(wParam);
        if (state != nullptr && DragQueryFileW(drop, 0xFFFFFFFFU, nullptr, 0) > 0) {
            wchar_t dropped[MAX_PATH]{};
            if (DragQueryFileW(drop, 0, dropped, MAX_PATH) > 0) {
                try {
                    const std::filesystem::path path(dropped);
                    if (std::filesystem::is_directory(path)) {
                        openGame(*state, path);
                    } else {
                        openMap(*state, path);
                    }
                } catch (const std::exception& error) {
                    setStatus(*state, error.what());
                    MessageBoxW(window, utf8ToWide(error.what()).c_str(), L"Whitehole Pro", MB_ICONERROR);
                }
            }
        }
        DragFinish(drop);
        return 0;
    }
    case WM_DESTROY:
        if (state != nullptr) {
            state->viewport.destroy();
            delete state;
            SetWindowLongPtrW(window, GWLP_USERDATA, 0);
        }
        PostQuitMessage(0);
        return 0;
    default:
        break;
    }
    return DefWindowProcW(window, message, wParam, lParam);
}

} // namespace

int runGui(const std::filesystem::path& executable, const std::filesystem::path& initialFile) {
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    INITCOMMONCONTROLSEX controls{sizeof(INITCOMMONCONTROLSEX), ICC_STANDARD_CLASSES};
    InitCommonControlsEx(&controls);

    WNDCLASSW windowClass{};
    windowClass.lpfnWndProc = editorProc;
    windowClass.hInstance = GetModuleHandleW(nullptr);
    windowClass.lpszClassName = L"WhiteholeProEditor";
    windowClass.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    windowClass.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
    RegisterClassW(&windowClass);

    HMENU menu = CreateMenu();
    HMENU fileMenu = CreatePopupMenu();
    AppendMenuW(fileMenu, MF_STRING, kIdOpenGame, L"Open Game Directory...");
    AppendMenuW(fileMenu, MF_STRING, kIdOpenMap, L"Open Map Archive...");
    AppendMenuW(fileMenu, MF_STRING, kIdSave, L"Save Zone");
    AppendMenuW(fileMenu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(fileMenu, MF_STRING, kIdExit, L"Exit");
    AppendMenuW(menu, MF_POPUP, reinterpret_cast<UINT_PTR>(fileMenu), L"File");

    HWND window = CreateWindowW(L"WhiteholeProEditor", L"Whitehole Pro", WS_OVERLAPPEDWINDOW,
                                CW_USEDEFAULT, CW_USEDEFAULT, 980, 800, nullptr, menu, GetModuleHandleW(nullptr), nullptr);
    auto* state = reinterpret_cast<EditorState*>(GetWindowLongPtrW(window, GWLP_USERDATA));
    if (state != nullptr) {
        state->dataRoot = dataDirectory(executable);
        state->galaxyNames.loadJson(state->dataRoot / "galaxies.json");
        state->zoneNames.loadJson(state->dataRoot / "zones.json");

        bool opened = false;
        if (!initialFile.empty() && std::filesystem::exists(initialFile)) {
            try {
                if (std::filesystem::is_directory(initialFile)) {
                    openGame(*state, initialFile);
                } else {
                    openMap(*state, initialFile);
                }
                opened = true;
            } catch (const std::exception& error) {
                setStatus(*state, "Could not open " + initialFile.string() + ": " + error.what());
            }
        }
        if (!opened) {
            setStatus(*state, "Drag a map archive onto the window, or use File > Open Game Directory.");
        }
    }
    ShowWindow(window, SW_SHOW);
    UpdateWindow(window);

    MSG message;
    while (GetMessageW(&message, nullptr, 0, 0) > 0) {
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }
    CoUninitialize();
    return static_cast<int>(message.wParam);
}

} // namespace whitehole::app
