#pragma once

#include <cstdlib>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include "terrain_support.h"

namespace fe_map_builder {

using TerrainMap = std::vector<std::vector<int>>;

struct BuildOptions {
    std::string apiRoot;
    std::string outputDirectory;
    int scale;
    unsigned int seed;
    int maxBacktracks;
    std::string visualTheme;

    BuildOptions(
        const std::string& apiRoot_,
        const std::string& outputDirectory_,
        int scale_ = 3,
        unsigned int seed_ = 1,
        int maxBacktracks_ = 20000,
        const std::string& visualTheme_ = "auto")
        : apiRoot(apiRoot_),
          outputDirectory(outputDirectory_),
          scale(scale_),
          seed(seed_),
          maxBacktracks(maxBacktracks_),
          visualTheme(visualTheme_) {}

    // Compatibility with the initial adapter API. The former editor path and
    // chapter/theme values are intentionally ignored: assets are now bundled.
    BuildOptions(
        const std::string& apiRoot_,
        const std::string& /*legacyEditorRoot*/,
        const std::string& outputDirectory_,
        const std::string& /*legacyThemeOrigin*/,
        int scale_ = 3,
        unsigned int seed_ = 1,
        int maxBacktracks_ = 20000,
        const std::string& visualTheme_ = "auto")
        : BuildOptions(apiRoot_, outputDirectory_, scale_, seed_, maxBacktracks_, visualTheme_) {}
};

struct BuildResult {
    bool success;
    std::string terrainJson;
    std::string builderTileMapJson;
    std::string png;
    std::string message;

    BuildResult() : success(false) {}
};

inline std::string ShellQuote(const std::string& value) {
    std::string quoted = "'";
    for (char c : value)
        quoted += c == '\'' ? "'\\\"'\\\"'" : std::string(1, c);
    return quoted + "'";
}

inline bool IsRectangular(const TerrainMap& map) {
    if (map.empty() || map.front().empty()) return false;
    for (const auto& row : map)
        if (row.size() != map.front().size()) return false;
    return true;
}

inline std::string JoinPath(const std::string& directory, const std::string& filename) {
    if (directory.empty() || directory[directory.size() - 1] == '/') return directory + filename;
    return directory + "/" + filename;
}

inline bool FileExists(const std::string& path) {
    std::ifstream file(path.c_str());
    return file.good();
}

inline void WriteTerrainJson(const TerrainMap& map, const std::string& output) {
    std::ofstream file(output);
    file << "{\n  \"terrain_rows\": [\n";
    for (std::size_t y = 0; y < map.size(); ++y) {
        file << "    [";
        for (std::size_t x = 0; x < map[y].size(); ++x)
            file << (x ? ", " : "") << map[y][x];
        file << "]" << (y + 1 == map.size() ? "\n" : ",\n");
    }
    file << "  ]\n}\n";
}

inline BuildResult Build(const TerrainMap& terrainMap, const BuildOptions& options) {
    BuildResult result;
    if (!IsRectangular(terrainMap)) {
        result.message = "Terrain map must be a non-empty rectangle.";
        return result;
    }
    if (options.apiRoot.empty() || options.outputDirectory.empty()) {
        result.message = "apiRoot and outputDirectory are required.";
        return result;
    }

    const std::string makeDirectory = "mkdir -p " + ShellQuote(options.outputDirectory);
    if (std::system(makeDirectory.c_str()) != 0) {
        result.message = "Could not create the output directory.";
        return result;
    }
    result.terrainJson = JoinPath(options.outputDirectory, "terrain_input.json");
    result.builderTileMapJson = JoinPath(options.outputDirectory, "builder_tile_map.json");
    result.png = JoinPath(options.outputDirectory, "map.png");
    WriteTerrainJson(terrainMap, result.terrainJson);

    const std::string compiler = JoinPath(JoinPath(options.apiRoot, "tools"), "fe_map_builder.py");
    const std::string assetRoot = JoinPath(options.apiRoot, "assets");
    std::ostringstream command;
    command << "python3 " << ShellQuote(compiler)
            << " --map " << ShellQuote(result.terrainJson)
            << " --asset-root " << ShellQuote(assetRoot)
            << " --support " << ShellQuote(JoinPath(JoinPath(options.apiRoot, "data"), "terrain_support.json"))
            << " --profiles " << ShellQuote(JoinPath(JoinPath(options.apiRoot, "data"), "visual_profiles.json"))
            << " --output-json " << ShellQuote(result.builderTileMapJson)
            << " --output-png " << ShellQuote(result.png)
            << " --scale " << options.scale
            << " --seed " << options.seed
            << " --theme " << ShellQuote(options.visualTheme)
            << " --max-backtracks " << options.maxBacktracks;

    const int exitCode = std::system(command.str().c_str());
    result.success = exitCode == 0 && FileExists(result.png) && FileExists(result.builderTileMapJson);
    result.message = result.success ? "Builder tile map and PNG generated." : "Map compiler failed; read its terminal error.";
    return result;
}

} // namespace fe_map_builder
