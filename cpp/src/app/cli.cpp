#include "whitehole/app/application.hpp"
#include "whitehole/app/settings.hpp"
#include "whitehole/app/object_db_update.hpp"

#include "whitehole/db/object_db.hpp"
#include "whitehole/io/binary_file.hpp"
#include "whitehole/io/rarc.hpp"
#include "whitehole/io/yaz0.hpp"
#include "whitehole/smg/bcsv.hpp"
#include "whitehole/smg/field_hashes.hpp"
#include "whitehole/smg/game_archive.hpp"
#include "whitehole/smg/hash.hpp"
#include "whitehole/smg/stage_archive.hpp"

#include <chrono>
#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <string>

namespace whitehole::app {
namespace {

std::filesystem::path executableDirectory(const std::filesystem::path& executable) {
    auto path = executable;
    if (path.has_parent_path()) {
        return path.parent_path();
    }
    return std::filesystem::current_path();
}

void printUsage() {
    std::cout
        << "Whitehole Pro C++ 0.2.0\n"
        << "Native Super Mario Galaxy editor core\n\n"
        << "Usage:\n"
        << "  whitehole-pro-console\n"
        << "  whitehole-pro-console gui\n"
        << "  whitehole-pro-console game list <game-directory>\n"
        << "  whitehole-pro-console galaxy inspect <game-directory> <galaxy>\n"
        << "  whitehole-pro-console zone objects <game-directory> <zone>\n"
        << "  whitehole-pro-console map objects <archive.arc>\n"
        << "  whitehole-pro-console archive list <archive.arc>\n"
        << "  whitehole-pro-console archive extract <archive.arc> <directory>\n"
        << "  whitehole-pro-console archive replace <archive.arc> <entry> <input> <output.arc>\n"
        << "  whitehole-pro-console yaz0 compress <input> <output>\n"
        << "  whitehole-pro-console yaz0 decompress <input> <output>\n"
        << "  whitehole-pro-console bcsv inspect <table.bcsv> [--little]\n"
        << "  whitehole-pro-console bcsv roundtrip <input.bcsv> <output.bcsv> [--little]\n"
        << "  whitehole-pro-console hash <field-name>\n"
        << "  whitehole-pro-console objectdb check [--data <directory>] [--no-cache]\n"
        << "  whitehole-pro-console objectdb update [--data <directory>]\n";
}

int archiveCommand(int argc, char** argv) {
    if (argc < 4) {
        throw std::runtime_error("archive requires an operation and input path");
    }
    const std::string operation = argv[2];
    auto archive = whitehole::io::RarcArchive::open(argv[3]);

    if (operation == "list") {
        std::cout << "Archive: " << archive.rootName()
                  << " (" << (archive.endian() == whitehole::io::Endian::big ? "big" : "little")
                  << "-endian, " << (archive.wasCompressed() ? "Yaz0 compressed" : "uncompressed") << ")\n";
        for (const auto& entry : archive.entries()) {
            std::cout << (entry.directory ? "d " : "f ") << std::setw(10) << entry.size << "  " << entry.path << '\n';
        }
        return 0;
    }
    if (operation == "extract") {
        if (argc != 5) {
            throw std::runtime_error("archive extract requires a destination directory");
        }
        archive.extractAll(argv[4]);
        std::cout << "Extracted " << archive.entries().size() << " entries to " << argv[4] << '\n';
        return 0;
    }
    if (operation == "replace") {
        if (argc != 7) {
            throw std::runtime_error("archive replace requires an entry, replacement file, and output archive");
        }
        archive.replace(argv[4], whitehole::io::readFile(argv[5]));
        whitehole::io::writeFile(argv[6], archive.serialize(archive.wasCompressed()));
        std::cout << "Replaced " << argv[4] << " and wrote " << argv[6] << '\n';
        return 0;
    }
    throw std::runtime_error("Unknown archive operation: " + operation);
}

int yaz0Command(int argc, char** argv) {
    if (argc != 5) {
        throw std::runtime_error("yaz0 requires an operation, input path, and output path");
    }
    const std::string operation = argv[2];
    const auto input = whitehole::io::readFile(argv[3]);
    if (operation == "compress") {
        whitehole::io::writeFile(argv[4], whitehole::io::yaz0::compress(input));
    } else if (operation == "decompress") {
        whitehole::io::writeFile(argv[4], whitehole::io::yaz0::decompress(input));
    } else {
        throw std::runtime_error("Unknown Yaz0 operation: " + operation);
    }
    std::cout << operation << "ed " << argv[3] << " -> " << argv[4] << '\n';
    return 0;
}

int bcsvCommand(int argc, char** argv) {
    if (argc < 4) {
        throw std::runtime_error("bcsv requires an operation and input path");
    }
    const std::string operation = argv[2];
    const bool littleEndian = std::string(argv[argc - 1]) == "--little";
    const auto endian = littleEndian ? whitehole::io::Endian::little : whitehole::io::Endian::big;
    const auto table = whitehole::smg::BcsvTable::open(argv[3], endian);
    smg::FieldHashes hashes;
    hashes.loadFile(dataDirectory(argv[0]) / "hashlookup.txt");

    if (operation == "inspect") {
        if (argc != 4 && !(argc == 5 && littleEndian)) {
            throw std::runtime_error("bcsv inspect accepts only an optional --little flag");
        }
        std::cout << table.rows().size() << " rows, " << table.fields().size()
                  << " fields, " << table.entrySize() << " bytes per row\n";
        for (const auto& field : table.fields()) {
            std::cout << hashes.nameOf(field.hash) << '\t';
        }
        std::cout << '\n';
        for (const auto& row : table.rows()) {
            for (const auto& value : row.values) {
                std::cout << whitehole::smg::toString(value) << '\t';
            }
            std::cout << '\n';
        }
        return 0;
    }
    if (operation == "roundtrip") {
        const auto expectedArguments = littleEndian ? 6 : 5;
        if (argc != expectedArguments) {
            throw std::runtime_error("bcsv roundtrip requires an output path and optional --little flag");
        }
        whitehole::io::writeFile(argv[4], table.serialize());
        std::cout << "Rewrote " << table.rows().size() << " rows to " << argv[4] << '\n';
        return 0;
    }
    throw std::runtime_error("Unknown BCSV operation: " + operation);
}

int gameCommand(int argc, char** argv) {
    if (argc != 4 || std::string(argv[2]) != "list") {
        throw std::runtime_error("game list requires a game directory");
    }
    smg::GameArchive game(argv[3]);
    if (game.gameType() == 0) {
        throw std::runtime_error("Directory is not an SMG1 or SMG2 workspace");
    }
    std::cout << "SMG" << game.gameType() << " workspace: " << argv[3] << '\n';
    std::cout << "Galaxies (" << game.galaxies().size() << "):\n";
    for (const auto& galaxy : game.galaxies()) {
        std::cout << "  " << galaxy << '\n';
    }
    std::cout << "Zones (" << game.zones().size() << "):\n";
    for (const auto& zone : game.zones()) {
        std::cout << "  " << zone << '\n';
    }
    return 0;
}

int galaxyCommand(int argc, char** argv) {
    if (argc != 5 || std::string(argv[2]) != "inspect") {
        throw std::runtime_error("galaxy inspect requires a game directory and galaxy name");
    }
    smg::GameArchive game(argv[3]);
    const auto galaxy = game.openGalaxy(argv[4]);
    std::cout << galaxy.name() << " zones:\n";
    for (const auto& zone : galaxy.zones()) {
        std::cout << "  " << zone << '\n';
    }
    return 0;
}

void printObjects(const smg::StageArchive& stage) {
    std::cout << stage.stageName() << ": " << stage.objects().size() << " objects\n";
    for (const auto& object : stage.objects()) {
        std::cout << "  [" << object.kind << "/" << object.layer << "] " << object.name
                  << "  (" << object.position.x << ", " << object.position.y << ", " << object.position.z << ")\n";
    }
}

int zoneCommand(int argc, char** argv) {
    if (argc != 5 || std::string(argv[2]) != "objects") {
        throw std::runtime_error("zone objects requires a game directory and zone name");
    }
    smg::GameArchive game(argv[3]);
    printObjects(smg::StageArchive::open(game.filesystem(), argv[4], game.gameType()));
    return 0;
}

int mapCommand(int argc, char** argv) {
    if (argc != 4 || std::string(argv[2]) != "objects") {
        throw std::runtime_error("map objects requires an archive path");
    }
    printObjects(smg::StageArchive::openMapFile(argv[3]));
    return 0;
}

int objectDbCommand(int argc, char** argv) {
    if (argc < 3) {
        throw std::runtime_error("objectdb supports: check, update");
    }
    const std::string operation = argv[2];
    if (operation != "check" && operation != "update") {
        throw std::runtime_error("unknown objectdb operation: " + operation);
    }
    std::filesystem::path dataDir = dataDirectory(argv[0]);
    bool useCache = true;
    for (int index = 3; index < argc; ++index) {
        const std::string flag = argv[index];
        if (flag == "--no-cache") {
            useCache = false;
        } else if (flag == "--data" && index + 1 < argc) {
            dataDir = argv[++index];
        }
    }
    const std::filesystem::path jsonPath = dataDir / "objectdb.json";

    if (operation == "update") {
        std::cout << "Downloading " << kObjectDatabaseUrl << '\n';
        const std::string failure = downloadObjectDatabase(jsonPath);
        if (!failure.empty()) {
            std::cerr << "Download failed: " << failure << '\n';
            return 1;
        }
        std::cout << "Saved " << jsonPath.string() << '\n'
                  << "Run 'whitehole-pro-console objectdb check' to verify it.\n";
        return 0;
    }

    const std::filesystem::path cachePath =
        useCache ? Settings::defaultConfigPath().parent_path() / "objectdb.cache"
                 : std::filesystem::path{};

    const auto started = std::chrono::steady_clock::now();
    db::ObjectDatabase database;
    database.load(jsonPath, cachePath);
    const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                             std::chrono::steady_clock::now() - started)
                             .count();

    if (database.empty()) {
        std::cout << "No object database found at " << jsonPath.string() << '\n'
                  << "Run 'whitehole-pro-console objectdb update' to download the community database.\n";
        return 1;
    }

    std::size_t withClass = 0;
    std::size_t withParameters = 0;
    for (const auto& name : database.names()) {
        const auto* info = database.classForObject(name, 2);
        if (info == nullptr) continue;
        ++withClass;
        if (!info->properties.empty()) ++withParameters;
    }

    std::cout << "Object database: " << jsonPath.string() << '\n'
              << "  objects:       " << database.size() << '\n'
              << "  classes:       " << database.classCount() << '\n'
              << "  categories:    " << database.categoryCount() << '\n'
              << "  timestamp:     " << database.timestamp() << '\n'
              << "  loaded from:   "
              << (database.cacheLoaded() ? "compiled cache" : "objectdb.json") << '\n'
              << "  load time:     " << elapsed << " ms\n"
              << "  SMG2 coverage: " << withClass << " objects resolve a class, "
              << withParameters << " expose parameters\n";
    return 0;
}

} // namespace

std::filesystem::path dataDirectory(const std::filesystem::path& executable) {
    const auto current = std::filesystem::current_path() / "data";
    if (std::filesystem::exists(current)) {
        return current;
    }
    const auto beside = executableDirectory(executable) / "data";
    if (std::filesystem::exists(beside)) {
        return beside;
    }
#ifdef WHITEHOLE_SOURCE_DIR
    return std::filesystem::path(WHITEHOLE_SOURCE_DIR) / "data";
#else
    return current;
#endif
}

int runCli(int argc, char** argv) {
    try {
        if (argc == 1) {
#ifdef _WIN32
            return runGui(argv[0]);
#else
            printUsage();
            return 0;
#endif
        }
        const std::string command = argv[1];
        if (command == "--help" || command == "-h") {
            printUsage();
            return 0;
        }
        if (command == "gui") {
            return runGui(argv[0]);
        }
        if (command == "archive") {
            return archiveCommand(argc, argv);
        }
        if (command == "yaz0") {
            return yaz0Command(argc, argv);
        }
        if (command == "bcsv") {
            return bcsvCommand(argc, argv);
        }
        if (command == "game") {
            return gameCommand(argc, argv);
        }
        if (command == "galaxy") {
            return galaxyCommand(argc, argv);
        }
        if (command == "zone") {
            return zoneCommand(argc, argv);
        }
        if (command == "map") {
            return mapCommand(argc, argv);
        }
        if (command == "objectdb") {
            return objectDbCommand(argc, argv);
        }
        if (command == "hash") {
            if (argc != 3) {
                throw std::runtime_error("hash requires one field name");
            }
            std::cout << "JMap:      0x" << std::hex << std::uppercase << std::setw(8) << std::setfill('0')
                      << whitehole::smg::jmapHash(argv[2]) << '\n'
                      << "SuperFast: 0x" << std::setw(8) << whitehole::smg::superFastHash(argv[2]) << '\n';
            return 0;
        }
        throw std::runtime_error("Unknown command: " + command);
    } catch (const std::exception& error) {
        std::cerr << "whitehole-pro-console: " << error.what() << '\n';
        return 1;
    }
}

} // namespace whitehole::app
