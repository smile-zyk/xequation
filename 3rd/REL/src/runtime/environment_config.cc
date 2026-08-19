#include "environment_config.h"

#include <fstream>
#include <sstream>
#include <stdexcept>

namespace rel {

DatasetConfig EnvironmentConfig::ParseDataset(const rapidjson::Value& v)
{
    DatasetConfig ds;
    ds.name   = v["name"].GetString();
    ds.format = v["format"].GetString();
    ds.path   = v["path"].GetString();
    return ds;
}

EnvironmentConfig EnvironmentConfig::Parse(const rapidjson::Document& doc)
{
    EnvironmentConfig cfg;

    if (doc.HasMember("datasets") && doc["datasets"].IsArray()) {
        for (const auto& ds : doc["datasets"].GetArray())
            cfg.datasets.push_back(ParseDataset(ds));
    }
    if (doc.HasMember("default_dataset") && doc["default_dataset"].IsString())
        cfg.default_dataset = doc["default_dataset"].GetString();

    if (doc.HasMember("python_plugins") && doc["python_plugins"].IsArray()) {
        for (const auto& p : doc["python_plugins"].GetArray()) {
            if (p.IsString())
                cfg.python_plugins.push_back(p.GetString());
        }
    }

    return cfg;
}

EnvironmentConfig EnvironmentConfig::Load(const std::string& config_path)
{
    std::ifstream in(config_path);
    if (!in)
        throw std::runtime_error("cannot open environment config: " + config_path);

    std::stringstream buf;
    buf << in.rdbuf();
    std::string content = buf.str();

    rapidjson::Document doc;
    doc.Parse(content.c_str());
    if (doc.HasParseError()) {
        throw std::runtime_error(
            "JSON parse error in " + config_path
            + " at offset " + std::to_string(doc.GetErrorOffset()));
    }

    return Parse(doc);
}

}  // namespace rel
