#pragma once

#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits> // For numeric_limits
#include <optional>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

#include <fmt/color.h>
#include <fmt/core.h>
#include <fmt/format.h>
#include <fmt/ranges.h>

#include <CLI/CLI.hpp>
#include <nlohmann/json.hpp>
#include <toml++/toml.hpp>
// Global verbose flag
bool verbose_output = false;
// Removed using namespace std::chrono_literals

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

// Assert - always active, regardless of build mode
#define Assert(expr) ((expr) ? (void)0 : (std::cerr << "Assert failed: " #expr << '\n', std::abort()))

namespace fs = std::filesystem;
using json = nlohmann::json;

// Custom exception for cleaner error handling
class CPPX_Exception : public std::runtime_error
{
  public:
    using std::runtime_error::runtime_error;
};

bool isAbsolutePath(const std::string &path)
{
    return std::filesystem::path(path).is_absolute();
}

std::string replace_spaces(const std::string &input)
{
    std::string copy = input;
    std::replace(copy.begin(), copy.end(), ' ', '_');
    return copy;
}
template <> struct fmt::formatter<std::filesystem::path> : fmt::formatter<std::string>
{
    template <typename FormatContext> auto format(const std::filesystem::path &p, FormatContext &ctx) const
    {
        return fmt::formatter<std::string>::format(p.string(), ctx);
    }
};

std::string displayStringVector(const std::vector<std::string> &vec)
{
    std::string result;
    for (const auto &str : vec)
    {
        if (!result.empty())
            result += " ";
        result += str;
    }
    return result;
}

std::string displayStringVectorPrefix(const std::vector<std::string> &vec, const std::string &prefix = "Prefix!",
                                      const std::string &separator = " ")
{
    std::ostringstream oss;

    for (size_t i = 0; i < vec.size(); ++i)
    {
        oss << prefix << vec[i];
        if (i != vec.size() - 1)
        {
            oss << separator;
        }
    }

    return oss.str();
}

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

    PackageManager(const fs::path dir = "vendor/") : vendor(std::move(dir))
    {
        if (!fs::exists(vendor))
        {
            fs::create_directories(vendor);
        }
    }

    void install(const std::string &packageRef)
    {
        std::string cmd =
            fmt::format("conan install --requires {} --build missing -of \"{}\" -f json --out-file \"{}\"", packageRef,
                        vendor.string(), (vendor / "install_log.json").string());

        LOG_VERBOSE("Executing Conan command: {}\n", cmd);
        fmt::print(fmt::emphasis::bold | fmt::fg(fmt::color::green), "Installing {}...\n", packageRef);

        int ret = std::system(cmd.c_str());

        if (ret != 0)
        {
            throw CPPX_Exception(fmt::format("Failed to install package '{}'.", packageRef));
        }
    }

    PackageInfo getPackageInfo(const std::string &packageRef) const
    {
        PackageInfo info;
        info.packageRef = packageRef;
        info.libs = get_info_from_package(packageRef, "libs");
        info.includePaths = get_info_from_package(packageRef, "includedirs");
        info.libPaths = get_info_from_package(packageRef, "libdirs");
        return info;
    }

    std::optional<PackageInfo> remove(const std::string &packageRef)
    {
        if (!checkIfInstalled(packageRef))
        {
            return std::nullopt;
        }

        PackageInfo info = getPackageInfo(packageRef);

        std::string cmdRemove = fmt::format("conan remove {} -f", packageRef);
        LOG_VERBOSE("Removing from Conan cache: {}\n", cmdRemove);
        int ret = std::system(cmdRemove.c_str());
        if (ret != 0)
        {
            throw CPPX_Exception("Failed to remove package from Conan cache.");
        }
        fmt::print(fmt::emphasis::bold | fmt::fg(fmt::color::green),
                   "Package successfully removed from Conan cache.\n");

        fs::path localPackageDir = vendor / packageRef;
        if (fs::exists(localPackageDir))
        {
            fs::remove_all(localPackageDir);
            LOG_VERBOSE("Removed local package directory: {}\n", localPackageDir.string());
        }

        return info;
    }

    json loadInstallLog() const
    {
        auto jsonPath = vendor / "install_log.json";
        if (!fs::exists(jsonPath))
        {
            throw CPPX_Exception("install_log.json not found, install package first.");
        }

        std::ifstream file(jsonPath);
        json j;
        file >> j;
        return j;
    }

    std::vector<std::string> get_info_from_package(const std::string &packageRef, const std::string &infoType) const
    {
        json j = loadInstallLog();
        if (!j.contains("graph") || !j["graph"].contains("nodes"))
            return {};

        for (auto &[key, node] : j["graph"]["nodes"].items())
        {
            if (!node.contains("ref"))
                continue;

            std::string ref = node["ref"];
            if (ref.find(packageRef) == 0)
            {
                if (node.contains("cpp_info"))
                {
                    if (infoType == "libs")
                    {
                        if (node["cpp_info"].contains("_fmt") && node["cpp_info"]["_fmt"].contains("libs") &&
                            node["cpp_info"]["_fmt"]["libs"].is_array())
                        {
                            return node["cpp_info"]["_fmt"]["libs"].get<std::vector<std::string>>();
                        }
                        else if (node["cpp_info"].contains("root") && node["cpp_info"]["root"].contains("libs") &&
                                 node["cpp_info"]["root"]["libs"].is_array())
                        {
                            return node["cpp_info"]["root"]["libs"].get<std::vector<std::string>>();
                        }
                    }
                    else if (node["cpp_info"].contains("root") && node["cpp_info"]["root"].contains(infoType) &&
                             node["cpp_info"]["root"][infoType].is_array())
                    {
                        std::vector<std::string> paths =
                            node["cpp_info"]["root"][infoType].get<std::vector<std::string>>();
                        if (infoType == "includedirs" || infoType == "libdirs")
                        {
                            for (auto &p : paths)
                                p = fs::absolute(p).string();
                        }
                        return paths;
                    }
                }
            }
        }
        return {};
    }

    bool checkIfInstalled(const std::string &packageRef) const
    {
        try
        {
            json j = loadInstallLog();
            if (!j.contains("graph") || !j["graph"].contains("nodes"))
                return false;

            for (auto &[key, node] : j["graph"]["nodes"].items())
            {
                if (!node.contains("ref"))
                    continue;

                std::string ref = node["ref"];
                if (ref.find(packageRef) == 0)
                    return true;
            }
        }
        catch (const CPPX_Exception &)
        {
            return false;
        }
        return false;
    }
};

struct Toolchain
{
    std::string compilerName;
    fs::path compilerPath;
    std::string compilerVersion;
    Toolchain(std::string c, fs::path cp, std::string cv) : compilerName(c), compilerPath(cp), compilerVersion(cv)
    {
    }
    Toolchain() = default;
};

struct ProjectConfig
{
    std::string path;
    std::string name;
    Toolchain toolchain;
    ProjectConfig(const std::string &p, const std::string &n, Toolchain t) : path(p), name(n), toolchain(t)
    {
    }
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
    buildType btype;

    BuildSettings(std::string on, buildType bt) : outputName(on), btype(bt)
    {
    }
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
    // Metadata fields
    std::string version;
    std::vector<std::string> authors;
    std::string description;
    std::string license;
    std::string github_username;
    std::string github_repo;

    std::unordered_map<std::string, std::string> defines;

    ProjectSettings(const std::string &n, const std::vector<std::string> &iff, const std::vector<std::string> &s,
                    const std::vector<std::string> &ip, const std::vector<std::string> &igs,
                    const std::vector<std::string> &igf, const std::unordered_map<std::string, std::string> &d,
                    const std::vector<std::string> &staticLinkFiles, const std::vector<std::string> &LinkDirs,
                    const std::unordered_map<std::string, std::string> &extra, const BuildSettings &bset,
                    const std::string &version, const std::vector<std::string> &authors, const std::string &description,
                    const std::string &license, const std::string &github_username, const std::string &github_repo,
                    std::unordered_map<std::string, std::string> defines)
        : name(n), includefiles(iff), srcfiles(s), includepaths(ip), ignoredPaths(igs), ignoredFiles(igf),
          staticLinkFiles(staticLinkFiles), LinkDirs(LinkDirs), dependencies(d), extra(extra), buildsettings(bset),
          version(version), authors(authors), description(description), license(license),
          github_username(github_username), github_repo(github_repo), defines(defines)
    {
    }
};
// Sets metadata in the [metadata] section of config.toml

ProjectConfig getCurrentProject();

std::vector<std::string> readTomlArray(const toml::node *node, const std::string &key)
{
    std::vector<std::string> result;
    if (node && node->as_table() && (*node->as_table())[key].as_array())
    {
        for (const auto &elem : *(*node->as_table())[key].as_array())
        {
            if (auto s = elem.value<std::string>())
            {
                result.push_back(*s);
            }
        }
    }
    else
    {
        throw CPPX_Exception(fmt::format("'{}' is not an array or does not exist!", key));
    }
    return result;
}

ProjectSettings getProjectSettings()
{
    ProjectConfig proj = getCurrentProject();
    toml::table config;
    try
    {
        config = toml::parse_file(fmt::format("{}/config.toml", proj.path));
    }
    catch (const toml::parse_error &err)
    {
        throw CPPX_Exception(fmt::format("Error parsing config.toml: {}", err.description()));
    }

    std::vector<std::string> includedirs;
    std::vector<std::string> includefiles;
    std::vector<std::string> srcfiles;
    std::vector<std::string> staticLinkFiles;
    std::vector<std::string> LinkDirs;

    if (auto source = config["source"].as_table())
    {
        includedirs = readTomlArray(source, "include directories");
        includefiles = readTomlArray(source, "include files");
        srcfiles = readTomlArray(source, "src files");
        staticLinkFiles = readTomlArray(source, "static_linked");
        LinkDirs = readTomlArray(source, "static_linked_dirs");
    }
    else
    {
        throw CPPX_Exception("Invalid configuration: missing [source] section!");
    }

    std::vector<std::string> ignoredFiles;
    std::vector<std::string> ignoredDirs;

    if (auto ignore = config["ignore"].as_table())
    {
        if (ignore->contains("dirs"))
        {
            ignoredDirs = readTomlArray(ignore, "dirs");
        }
        if (ignore->contains("files"))
        {
            ignoredFiles = readTomlArray(ignore, "files");
        }
    }

    std::unordered_map<std::string, std::string> dep;
    if (auto dependencies = config["dependencies"].as_table())
    {
        for (const auto &[key, node] : *dependencies->as_table())
        {
            if (!node.is_string())
            {
                throw CPPX_Exception(
                    fmt::format("Invalid configuration: dependency value for '{}' is not a string!", key.str()));
            }
            dep[std::string(key.str())] = node.value_or("");
        }
    }

    std::unordered_map<std::string, std::string> extra;
    if (auto extras = config["extra"].as_table())
    {
        for (const auto &[key, node] : *extras->as_table())
        {
            if (!node.is_string())
            {
                throw CPPX_Exception(
                    fmt::format("Invalid configuration: extra value for '{}' is not a string!", key.str()));
            }
            extra[std::string(key.str())] = node.value_or("");
        }
    }

    // Parse metadata from [metadata] section
    std::string version;
    std::vector<std::string> authors;
    std::string description;
    std::string license;
    std::string github_username;
    std::string github_repo;

    if (auto metadata = config["metadata"].as_table())
    {
        version = (*metadata)["version"].value_or<std::string>("");
        description = (*metadata)["description"].value_or<std::string>("");
        license = (*metadata)["license"].value_or<std::string>("");
        github_username = (*metadata)["github_username"].value_or<std::string>("");
        github_repo = (*metadata)["github_repo"].value_or<std::string>("");
        // authors can be array or string
        if ((*metadata)["authors"].is_array())
        {
            authors = readTomlArray(metadata, "authors");
        }
        else if ((*metadata)["authors"].is_string())
        {
            auto author_str = (*metadata)["authors"].value_or<std::string>("");
            if (!author_str.empty())
                authors.push_back(author_str);
        }
    }

    BuildSettings bset;
    if (auto build = config["build"].as_table())
    {
        bset.outputName = (*build)["build_name"].value_or<std::string>("_default");
        std::string t = (*build)["build_type"].value_or<std::string>("_default");
        if (t == "executable" || t == "_default")
        {
            bset.btype = buildType::BUILD_EXECUTABLE;
        }
        else if (t == "shared" || t == "dynamic")
        {
            bset.btype = buildType::BUILD_DYNAMICLINK;
        }
        else if (t == "static")
        {
            bset.btype = buildType::BUILD_STATICLINK;
        }
        else
        {
            throw CPPX_Exception("Invalid configuration: Invalid build type!");
        }
        if (bset.outputName == "_default")
        {
            bset.outputName = proj.name;
        }
    }

    std::unordered_map<std::string, std::string> defines;
    if (auto define_ = config["defines"].as_table())
    {
        for (const auto &[key, node] : *define_->as_table())
        {
            if (!node.is_string())
            {
                throw CPPX_Exception(
                    fmt::format("Invalid configuration: define value for '{}' is not a string!", key.str()));
            }
            defines[std::string(key.str())] = node.value_or("");
        }
    }

    return ProjectSettings(proj.name, includefiles, srcfiles, includedirs, ignoredDirs, ignoredFiles, dep,
                           staticLinkFiles, LinkDirs, extra, bset, version, authors, description, license,
                           github_username, github_repo, defines);
}

ProjectConfig getCurrentProject()
{
    fs::path pathToGlobal = std::getenv("HOME");
    pathToGlobal /= ".cppxglobal.toml";

    toml::table file;

    if (std::filesystem::exists(pathToGlobal))
    {
        try
        {
            file = toml::parse_file(pathToGlobal.string());
        }
        catch (const toml::parse_error &err)
        {
            throw CPPX_Exception(fmt::format("Failed to parse TOML file: {}", err.description()));
        }
    }

    std::string name;
    std::string path;
    Toolchain tc;
    if (auto project = file["project"].as_table())
    {
        name = (*project)["name"].value_or<std::string>("no name");
        path = (*project)["path"].value_or<std::string>("no path");
    }
    else
    {
        throw CPPX_Exception("No project set! Use 'cppx project set'.");
    }
    if (auto toolchain = file["toolchain"].as_table())
    {
        tc.compilerName = (*toolchain)["compiler"].value_or<std::string>("no compiler");
        tc.compilerPath = (*toolchain)["path"].value_or<std::string>("no path");
        tc.compilerVersion = (*toolchain)["version"].value_or<std::string>("no version");
    }
    else
    {
        fmt::print(stderr, fmt::emphasis::bold | fg(fmt::color::yellow),
                   "No toolchain set, run 'cppx profile' to create toolchain info\n");
        throw CPPX_Exception("Toolchain not configured.");
    }

    if (name == "no name" || path == "no path")
    {
        throw CPPX_Exception("No project set! Use 'cppx project set'.");
    }
    if (tc.compilerName == "no compiler" || tc.compilerPath == "no path" || tc.compilerVersion == "no version")
    {
        throw CPPX_Exception("No toolchain set, run 'cppx profile'.");
    }
    return ProjectConfig(path, name, tc);
}

constexpr std::size_t eventBufLen = 1024 * (sizeof(inotify_event) + 16);

template <typename T> void updateTomlArray(toml::array &arr, const T &value, const std::string &typeName)
{
    bool already_present = std::find_if(arr.begin(), arr.end(), [&](const toml::v3::node &node) {
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
void setExtra(const std::string &name, const std::string &value)
{
    ProjectConfig pc = getCurrentProject();
    toml::table table = toml::parse_file(pc.path + "/config.toml");

    if (auto *extra = table["extra"].as_table())
    {
        (*extra).insert_or_assign(name, value);
    }
    else
    {
        table.insert("extra", toml::table{{name, value}});
    }

    std::ofstream ofs(pc.path + "/config.toml");
    ofs << table;
}

std::string pickCompiler()
{
    const ProjectConfig pc = getCurrentProject();
    const ProjectSettings ps = getProjectSettings();

    if (ps.extra.find("compiler") != ps.extra.end())
    {
        return ps.extra.at("compiler");
    }
    else
    {
        return pc.toolchain.compilerPath;
    }
}

// Command handlers
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

inline void setMetadata(const std::string &key, const std::string &value)
{
    ProjectConfig pc = getCurrentProject();
    toml::table table = toml::parse_file(pc.path + "/config.toml");
    toml::table metadata;
    if (table.contains("metadata") && table["metadata"].is_table())
        metadata = *table["metadata"].as_table();
    metadata.insert_or_assign(key, value);
    table.insert_or_assign("metadata", metadata);
    std::ofstream ofs(pc.path + "/config.toml");
    ofs << table;
}
inline void setMetadata(const std::string &key, const std::vector<std::string> &values)
{
    ProjectConfig pc = getCurrentProject();
    toml::table table = toml::parse_file(pc.path + "/config.toml");
    toml::table metadata;
    if (table.contains("metadata") && table["metadata"].is_table())
        metadata = *table["metadata"].as_table();

    toml::array arr;
    for (const auto &value : values)
    {
        arr.push_back(value);
    }
    metadata.insert_or_assign(key, arr);
    table.insert_or_assign("metadata", metadata);
    std::ofstream ofs(pc.path + "/config.toml");
    ofs << table;
}
class FileWatcher
{
  public:
    using Callback = std::function<void(fs::path, bool created)>;

    // Constructor for FileWatcher
    FileWatcher(fs::path dir, Callback cb, std::chrono::milliseconds interval = std::chrono::milliseconds(500))
        : _dir(std::move(dir)), _cb(std::move(cb)), _interval(interval)
    {
        _snapshot = snapshot_dir();
    }

    void run(std::stop_token stoken)
    {
        while (!stoken.stop_requested())
        {
            std::this_thread::sleep_for(_interval);
            std::unordered_map<fs::path, fs::file_time_type> current;
            try {
                current = snapshot_dir();
            } catch (const std::filesystem::filesystem_error &e) {
                fmt::print(stderr, fmt::emphasis::bold | fg(fmt::color::red),
                           "[FileWatcher] Filesystem error: {}\n", e.what());
                continue;
            } catch (const std::exception &e) {
                fmt::print(stderr, fmt::emphasis::bold | fg(fmt::color::red),
                           "[FileWatcher] Unexpected error: {}\n", e.what());
                continue;
            }

            // Detect new files
            for (auto const &[p, time] : current)
            {
                if (!_snapshot.contains(p))
                {
                    _cb(p, true);
                }
            }
            // Detect removed files
            for (auto const &[p, time] : _snapshot)
            {
                if (!current.contains(p))
                {
                    _cb(p, false);
                }
            }

            _snapshot = std::move(current);
        }
    }

  private:
    fs::path _dir;
    Callback _cb;
    std::chrono::milliseconds _interval;
    std::unordered_map<fs::path, fs::file_time_type> _snapshot;

    static std::unordered_map<fs::path, fs::file_time_type> snapshot_dir(fs::path const &dir)
    {
        std::unordered_map<fs::path, fs::file_time_type> m;
        for (auto const &entry : fs::directory_iterator(dir))
        {
            if (entry.is_regular_file())
            {
                m[entry.path().filename()] = entry.last_write_time();
            }
        }
        return m;
    }
    std::unordered_map<fs::path, fs::file_time_type> snapshot_dir() const
    {
        return snapshot_dir(_dir);
    }
};
