#pragma once

// Java parity: ObjectDB.init() downloads the community object database from
// SMGCommunity/galaxydatabase when it is missing or stale. The native build
// keeps that behaviour in an explicit, non-fatal helper so the editor and the
// CLI can fetch the database on first run (data/objectdb.json is gitignored).

#include <filesystem>
#include <string>

namespace whitehole::app {

// Official community object database (Java ObjectDB.SOURCE_URL).
inline constexpr const char* kObjectDatabaseUrl =
    "https://raw.githubusercontent.com/SMGCommunity/galaxydatabase/main/objectdb.json";

// True when this platform has a downloader implementation.
[[nodiscard]] bool objectDatabaseDownloadAvailable() noexcept;

// Downloads the object database to `destination` and returns an empty string on
// success, otherwise a human-readable message. Never throws, and writes
// atomically (temporary file then rename) so a failed download cannot leave a
// truncated database behind.
[[nodiscard]] std::string downloadObjectDatabase(const std::filesystem::path& destination);

} // namespace whitehole::app