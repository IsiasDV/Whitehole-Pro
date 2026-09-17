#include "whitehole/smg/stage_archive.hpp"

#include "whitehole/io/binary_file.hpp"
#include "whitehole/smg/hash.hpp"

#include <array>
#include <stdexcept>

namespace whitehole::smg {
namespace {

struct LayeredTable {
    const char* folder;
    const char* file;
    const char* kind;
    int gameType; // 0 both, 1 SMG1, 2 SMG2
};

constexpr std::array<LayeredTable, 12> kLayeredTables{{
    {"Placement", "StageObjInfo", "stage", 0},
    {"Placement", "AreaObjInfo", "area", 0},
    {"Placement", "ObjInfo", "obj", 0},
    {"Placement", "CameraCubeInfo", "camera", 0},
    {"Placement", "PlanetObjInfo", "gravity", 0},
    {"Placement", "DemoObjInfo", "cutscene", 0},
    {"MapParts", "MapPartsInfo", "mappart", 0},
    {"Start", "StartInfo", "start", 0},
    {"GeneralPos", "GeneralPosInfo", "position", 0},
    {"Debug", "DebugMoveInfo", "debug", 0},
    {"Placement", "SoundInfo", "sound", 1},
    {"ChildObj", "ChildObjInfo", "child", 1},
}};

std::string mapFilesystemPath(std::string_view stageName, int gameType) {
    if (gameType == 1) {
        return "/StageData/" + std::string(stageName) + ".arc";
    }
    return "/StageData/" + std::string(stageName) + "/" + std::string(stageName) + "Map.arc";
}

} // namespace

void StageArchive::loadTable(std::string_view path, std::string kind, std::string layer) {
    if (!archive_ || !archive_->fileExists(path)) {
        return;
    }
    try {
        ObjectTable table;
        table.path = std::string(path);
        table.kind = std::move(kind);
        table.layer = std::move(layer);
        table.table = BcsvTable(archive_->read(path), archive_->endian());
        const auto nameHash = jmapHash("name");
        const auto posXHash = jmapHash("pos_x");
        const auto posYHash = jmapHash("pos_y");
        const auto posZHash = jmapHash("pos_z");
        const auto dirXHash = jmapHash("dir_x");
        const auto dirYHash = jmapHash("dir_y");
        const auto dirZHash = jmapHash("dir_z");
        const auto scaleXHash = jmapHash("scale_x");
        const auto scaleYHash = jmapHash("scale_y");
        const auto scaleZHash = jmapHash("scale_z");

        for (std::size_t row = 0; row < table.table.rows().size(); ++row) {
            PlacementObject object;
            object.tableIndex = tables_.size();
            object.rowIndex = row;
            object.kind = table.kind;
            object.layer = table.layer;
            const auto& entry = table.table.rows()[row];
            object.name = table.table.getStringById(entry, nameHash);
            object.position = {table.table.getFloatById(entry, posXHash), table.table.getFloatById(entry, posYHash),
                               table.table.getFloatById(entry, posZHash)};
            object.rotation = {table.table.getFloatById(entry, dirXHash), table.table.getFloatById(entry, dirYHash),
                               table.table.getFloatById(entry, dirZHash)};
            object.scale = {table.table.getFloatById(entry, scaleXHash, 1.0F), table.table.getFloatById(entry, scaleYHash, 1.0F),
                            table.table.getFloatById(entry, scaleZHash, 1.0F)};
            objects_.push_back(std::move(object));
        }
        tables_.push_back(std::move(table));
    } catch (const std::exception&) {
        // Empty or non-JMap placeholders are skipped so a zone can still open.
    }
}

void StageArchive::loadFromArchive() {
    tables_.clear();
    objects_.clear();
    if (!archive_) {
        return;
    }
    for (const auto& spec : kLayeredTables) {
        if (spec.gameType != 0 && spec.gameType != gameType_) {
            continue;
        }
        const auto folder = std::string("/Stage/jmp/") + spec.folder;
        for (const auto& layer : archive_->directories(folder)) {
            const auto path = folder + "/" + layer + "/" + spec.file;
            loadTable(path, spec.kind, layer);
        }
    }
    loadTable("/Stage/jmp/Path/CommonPathInfo", "path", "Common");
}

StageArchive StageArchive::openMapFile(const std::filesystem::path& path, int gameType) {
    StageArchive stage;
    stage.gameType_ = gameType;
    stage.stageName_ = path.stem().string();
    stage.sourcePath_ = path;
    stage.archive_ = io::RarcArchive::open(path);
    stage.loadFromArchive();
    return stage;
}

StageArchive StageArchive::open(io::DirectoryFilesystem& filesystem, std::string_view stageName, int gameType) {
    StageArchive stage;
    stage.filesystem_ = &filesystem;
    stage.gameType_ = gameType;
    stage.stageName_ = std::string(stageName);
    stage.filesystemPath_ = mapFilesystemPath(stageName, gameType);
    if (!filesystem.fileExists(stage.filesystemPath_)) {
        throw std::runtime_error("Stage map archive is missing: " + stage.filesystemPath_);
    }
    stage.sourcePath_ = filesystem.root() / stage.filesystemPath_.substr(1);
    stage.archive_ = io::RarcArchive(filesystem.read(stage.filesystemPath_));
    stage.loadFromArchive();
    return stage;
}

void StageArchive::applyEdits() {
    for (auto& object : objects_) {
        if (object.tableIndex >= tables_.size()) {
            continue;
        }
        auto& table = tables_[object.tableIndex];
        if (object.rowIndex >= table.table.rows().size()) {
            continue;
        }
        auto& row = table.table.rows()[object.rowIndex];
        table.table.setString(row, "name", object.name);
        table.table.setFloat(row, "pos_x", object.position.x);
        table.table.setFloat(row, "pos_y", object.position.y);
        table.table.setFloat(row, "pos_z", object.position.z);
        table.table.setFloat(row, "dir_x", object.rotation.x);
        table.table.setFloat(row, "dir_y", object.rotation.y);
        table.table.setFloat(row, "dir_z", object.rotation.z);
        table.table.setFloat(row, "scale_x", object.scale.x);
        table.table.setFloat(row, "scale_y", object.scale.y);
        table.table.setFloat(row, "scale_z", object.scale.z);
    }
}

void StageArchive::saveTo(const std::filesystem::path& path) {
    applyEdits();
    if (!archive_) {
        throw std::runtime_error("No map archive is loaded");
    }
    for (const auto& table : tables_) {
        archive_->replace(table.path, table.table.serialize());
    }
    io::writeFile(path, archive_->serialize(archive_->wasCompressed()));
    sourcePath_ = path;
}

void StageArchive::save() {
    if (filesystem_ != nullptr && !filesystemPath_.empty()) {
        applyEdits();
        if (!archive_) {
            throw std::runtime_error("No map archive is loaded");
        }
        for (const auto& table : tables_) {
            archive_->replace(table.path, table.table.serialize());
        }
        filesystem_->write(filesystemPath_, archive_->serialize(archive_->wasCompressed()));
        return;
    }
    if (sourcePath_.empty()) {
        throw std::runtime_error("No destination is available for this stage");
    }
    saveTo(sourcePath_);
}

} // namespace whitehole::smg
