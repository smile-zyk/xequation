#pragma once

#include <string>
#include <vector>
#include <rapidjson/document.h>

namespace rel {

// =========================================================================
//  EnvironmentConfig — persistent dataset + plugin context (JSON)
// =========================================================================
//
//  JSON schema:
//  {
//    "datasets": [
//      { "name": "noise",    "format": "hdf5", "path": "/data/noise.xdataset" },
//      { "name": "amplifier","format": "hdf5", "path": "/data/amp.xdataset" }
//    ],
//    "default_dataset": "noise",
//    "python_plugins": ["plugins/snr.py", "plugins/eye.py"]
//  }
//
//  "default_dataset" is optional; if omitted, the first dataset in "datasets"
//  becomes the default.
//  "python_plugins" is optional; each entry is a .py plugin path resolved
//  relative to the config file's directory (requires BUILD_PYTHON=ON).

struct DatasetConfig {
    std::string name;
    std::string format;
    std::string path;
};

struct EnvironmentConfig {
    std::vector<DatasetConfig> datasets;
    std::string               default_dataset;
    std::vector<std::string>  python_plugins;

    /// Load from a JSON file.
    static EnvironmentConfig Load(const std::string& config_path);

private:
    static DatasetConfig     ParseDataset(const rapidjson::Value& v);
    static EnvironmentConfig Parse(const rapidjson::Document& doc);
};

}  // namespace rel
