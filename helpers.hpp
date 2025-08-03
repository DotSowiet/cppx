#pragma once

#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>
#include <vector>
#include <unordered_map>
#include <chrono>
#include <functional>
#include <thread>
#include <optional>
#include <algorithm> // Required for std::find_if

#include <fmt/color.h>
#include <fmt/core.h>
#include <fmt/format.h>
#include <fmt/ranges.h>
#include <CLI/CLI.hpp>
#include <nlohmann/json.hpp>
#include <toml++/toml.hpp>

namespace fs = std::filesystem;
using json = nlohmann::json;

inline bool verbose_output = false;

#define LOG_VERBOSE(...)                                                                                               \
    if (verbose_output)                                                                                                \
    {                                                                                                                  \
        fmt::print(fg(fmt::color::gray), "[VERBOSE] " __VA_ARGS__);                                                    \
    }

#ifndef NDEBUG
#define DebugAssert(expr) ((expr) ? (void)0 : (std::cerr << "DebugAssert failed: " #expr << '\n', std::abort()))
#else
#define DebugAssert(expr) ((void)0)
#endif

#define Assert(expr) ((expr) ? (void)0 : (std::cerr << "Assert failed: " #expr << '\n', std::abort()))

class CPPX_Exception final : public std::runtime_error
{
  public:
    using std::runtime_error::runtime_error;
};
bool isAbsolutePath(const std::string &path);
std::string replace_spaces(const std::string &input);
template <> struct fmt::formatter<std::filesystem::path> : fmt::formatter<std::string>
{
    template <typename FormatContext> auto format(const std::filesystem::path &p, FormatContext &ctx) const
    {
        return fmt::formatter<std::string>::format(p.string(), ctx);
    }
};
 std::string displayStringVector(const std::vector<std::string> &vec);
 std::string displayStringVectorPrefix(const std::vector<std::string> &vec, const std::string &prefix,
                                             const std::string &separator);

class PackageManager
{
    fs::path vendor;

  public:
    struct PackageInfo
    {
        std::string packageRef;
        std::vector<std::string> libs;
        std::vector<std::string> includePaths;
        std::vector<std::string> libPaths;
    };

    explicit PackageManager(fs::path dir = "vendor/");

    void install(const std::string &packageRef) const;
    [[nodiscard]] PackageInfo getPackageInfo(const std::string &packageRef) const;
    [[nodiscard]] std::optional<PackageInfo> remove(const std::string &packageRef) const;
    [[nodiscard]] bool checkIfInstalled(const std::string &packageRef) const;

  private:
    [[nodiscard]] json loadInstallLog() const;
    [[nodiscard]] std::vector<std::string> get_info_from_package(const std::string &packageRef,
                                                                 const std::string &infoType) const;
};

struct Toolchain
{
    std::string compilerName;
    fs::path compilerPath;
    std::string compilerVersion;
    Toolchain(std::string c, fs::path cp, std::string cv);
    Toolchain() = default;
};

struct ProjectConfig
{
    std::string path;
    std::string name;
    Toolchain toolchain;
    ProjectConfig(std::string p, std::string n, Toolchain t);
};

enum class buildType
{
    BUILD_EXECUTABLE,
    BUILD_STATICLINK,
    BUILD_DYNAMICLINK
};

struct BuildSettings
{
    std::string outputName;
    buildType btype = buildType::BUILD_EXECUTABLE;

    BuildSettings(std::string on, buildType bt);
    BuildSettings() = default;
};

struct ProjectSettings
{
    std::string name;
    std::vector<std::string> includefiles;
    std::vector<std::string> srcfiles;
    std::vector<std::string> includepaths;
    std::vector<std::string> ignoredPaths;
    std::vector<std::string> ignoredFiles;
    std::vector<std::string> staticLinkFiles;
    std::vector<std::string> LinkDirs;

    std::unordered_map<std::string, std::string> dependencies;
    std::unordered_map<std::string, std::string> extra;
    BuildSettings buildsettings;

    std::string version;
    std::vector<std::string> authors;
    std::string description;
    std::string license;
    std::string github_username;
    std::string github_repo;
    std::unordered_map<std::string, std::string> defines;

    ProjectSettings(std::string n, const std::vector<std::string> &iff, const std::vector<std::string> &s,
                    const std::vector<std::string> &ip, const std::vector<std::string> &igs,
                    const std::vector<std::string> &igf, const std::unordered_map<std::string, std::string> &d,
                    const std::vector<std::string> &staticLinkFiles, const std::vector<std::string> &LinkDirs,
                    const std::unordered_map<std::string, std::string> &extra, BuildSettings bset,
                    std::string version, const std::vector<std::string> &authors, std::string description,
                    std::string license, std::string github_username, std::string github_repo,
                    std::unordered_map<std::string, std::string> defines);
};

ProjectConfig getCurrentProject();
std::vector<std::string> readTomlArray(const toml::node *node, const std::string &key);
ProjectSettings getProjectSettings();
std::string pickCompiler();
void setExtra(const std::string &name, const std::string &value);
void setMetadata(const std::string &key, const std::string &value);
void setMetadata(const std::string &key, const std::vector<std::string> &values);

// TEMPLATE FUNCTION DEFINITIONS
// These must be in the header file for the compiler to instantiate them correctly.

template <typename T>
void updateTomlArray(toml::array &arr, const T &value, const std::string &typeName)
{
    const bool already_present = std::find_if(arr.begin(), arr.end(), [&](const toml::v3::node &node) {
                               return node.is_string() && node.as_string() && *(node.as_string()) == value;
                           }) != arr.end();

    if (!already_present)
    {
        LOG_VERBOSE("Adding new {}: {}\n", typeName, value);
        arr.push_back(value);
    }
    else
    {
        LOG_VERBOSE("{} already present: {}\n", typeName, value);
    }
}

template <typename T>
void removeFromTomlArray(toml::v3::array &toml_array, const T &values_to_remove, const std::string &description)
{
    toml::v3::array new_toml_array;

    for (const auto &node : toml_array)
    {
        if (node.is_string())
        {
            bool found_match = false;
            for (const auto &value_to_remove : values_to_remove)
            {
                if (*(node.as_string()) == value_to_remove)
                {
                    found_match = true;
                    LOG_VERBOSE("Removed {}: {}\n", description, value_to_remove);
                    break;
                }
            }
            if (!found_match)
            {
                new_toml_array.push_back(node);
            }
        }
        else
        {
            new_toml_array.push_back(node);
        }
    }
    toml_array = std::move(new_toml_array);
}


void handle_project_new(const std::string &projectName);
void handle_project_set(const std::string &projectName, const std::string &projectPath);
void handle_build(bool debug, const std::string &config);
void handle_run();
void handle_watch(const std::string &dir, bool force);
void handle_ignore(const std::vector<fs::path> &directories);
void handle_pkg_install(const std::string &packageName, const std::string &packageVersion);
void handle_pkg_remove(const std::string &packageToRemove);
void handle_export(const std::string &format);
void handle_config_set(const std::string &what);
void handle_profile();
void handle_doc();
void handle_clean();
void handle_test();
void handle_metadata();
void handle_info();

class FileWatcher
{
  public:
    using Callback = std::function<void(fs::path, bool created)>;

    FileWatcher(fs::path dir, Callback cb, std::chrono::milliseconds interval = std::chrono::milliseconds(500));
    void run(const std::stop_token &stoken);

  private:
    fs::path _dir;
    Callback _cb;
    std::chrono::milliseconds _interval;
    std::unordered_map<fs::path, fs::file_time_type> _snapshot;

    static std::unordered_map<fs::path, fs::file_time_type> snapshot_dir(fs::path const &dir);
    [[nodiscard]] std::unordered_map<fs::path, fs::file_time_type> snapshot_dir() const;
};
