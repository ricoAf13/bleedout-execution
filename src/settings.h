#pragma once

#include <nlohmann/json.hpp>
#include <filesystem>
#include <fstream>

namespace bleedout_exec
{
using json = nlohmann::json;

inline void logParseError(const json::parse_error& e)
{
    logger::warn("Parse error at input byte {}\n"
                 "\t{}",
                 e.byte, e.what());
}

inline void logJsonException(std::string_view context, const json::exception& e)
{
    logger::warn("Error while deserializing {}\n"
                 "\t{}",
                 context, e.what());
}

struct Settings
{
    bool enabled = true;

    bool  enable_bleedout_execution = true;
    bool  enable_getup_execution    = false;
    float execution_probability     = 100.0f;

    // Precondition
    bool                 essential_protection = true;
    bool                 protected_protection = true;
    std::array<float, 2> height_diff_range    = {-35.f, 35.f};
    bool                 last_hostile_check   = true;
    float                last_hostile_range   = 1024.f;
    StrSet               skipped_race         = {"FrostbiteSpiderRaceGiant",
                                                  "SprigganMatronRace",
                                                  "SprigganEarthMotherRace",
                                                  "DLC2SprigganBurntRace",
                                                  "DLC1LD_ForgemasterRace",
                                                  "DLC2GhostFrostGiantRace"};

    // Tags
    StrSet banned_tags = {"adv", "sneak"};

    bool enable_debug_log = false;

    // Animation directory
    std::string anim_dir = std::string{def_anim_dir};

    static Settings* getSingleton()
    {
        static Settings settings;
        return std::addressof(settings);
    }

    // Declaration only — implementation after NLOHMANN macro
    bool load(std::string_view path);
};

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(
    Settings,
    enabled,
    enable_bleedout_execution,
    enable_getup_execution,
    execution_probability,
    essential_protection,
    protected_protection,
    height_diff_range,
    last_hostile_check,
    last_hostile_range,
    skipped_race,
    banned_tags,
    enable_debug_log,
    anim_dir)

// Implementation AFTER macro so from_json is visible
inline bool Settings::load(std::string_view path)
{
    namespace fs = std::filesystem;

    logger::info("Loading config from {}...", path);

    fs::path file_path{path};
    if (!fs::exists(file_path))
    {
        logger::warn("Config file '{}' not found. Using defaults.", path);
        return false;
    }

    std::ifstream istream{file_path};
    if (!istream.is_open())
    {
        logger::warn("Failed to open config file '{}'.", path);
        return false;
    }

    json j;
    try
    {
        j = json::parse(istream);
    }
    catch (json::parse_error& e)
    {
        logParseError(e);
        return false;
    }

    try
    {
        from_json(j, *this);
    }
    catch (json::exception& e)
    {
        logJsonException("Settings", e);
        return false;
    }

    logger::info("Config loaded successfully.");
    return true;
}

} // namespace bleedout_exec
