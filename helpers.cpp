#include "helpers.hpp"

#include <utility>
#include <cstdlib>
#include <algorithm>
#include <string>
#include <vector>
#include <sstream>

 bool isAbsolutePath(const std::string &path)
{
    return std::filesystem::path(path).is_absolute();
}

std::string replace_spaces(const std::string &input)
{
    std::string copy = input;
    std::ranges::replace(copy, ' ', '_');
    return copy;
}

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


PackageManager::PackageManager(fs::path dir) : vendor(std::move(dir))
{
    if (!fs::exists(vendor))
    {
        fs::create_directories(vendor);
    }
}

void PackageManager::install(const std::string &packageRef) const
{
    const std::string cmd =
        fmt::format(R"(conan install --requires {} --build missing -of "{}" -f json --out-file "{}")", packageRef,
                    vendor.string(), (vendor / "install_log.json").string());

    LOG_VERBOSE("Executing Conan command: {}\n", cmd);
    fmt::print(fmt::emphasis::bold | fmt::fg(fmt::color::green), "Installing {}...\n", packageRef);

    if (const int ret = std::system(cmd.c_str()); ret != 0)
    {
        throw CPPX_Exception(fmt::format("Failed to install package '{}'.", packageRef));
    }
}

PackageManager::PackageInfo PackageManager::getPackageInfo(const std::string &packageRef) const
{
    PackageInfo info;
    info.packageRef = packageRef;
    info.libs = get_info_from_package(packageRef, "libs");
    info.includePaths = get_info_from_package(packageRef, "includedirs");
    info.libPaths = get_info_from_package(packageRef, "libdirs");
    return info;
}

std::optional<PackageManager::PackageInfo> PackageManager::remove(const std::string &packageRef) const
{
    if (!checkIfInstalled(packageRef))
    {
        return std::nullopt;
    }

    PackageInfo info = getPackageInfo(packageRef);

    const std::string cmdRemove = fmt::format("conan remove {} -f", packageRef);
    LOG_VERBOSE("Removing from Conan cache: {}\n", cmdRemove);
    if (const int ret = std::system(cmdRemove.c_str()); ret != 0)
    {
        throw CPPX_Exception("Failed to remove package from Conan cache.");
    }
    fmt::print(fmt::emphasis::bold | fmt::fg(fmt::color::green),
               "Package successfully removed from Conan cache.\n");

    if (const fs::path localPackageDir = vendor / packageRef; fs::exists(localPackageDir))
    {
        fs::remove_all(localPackageDir);
        LOG_VERBOSE("Removed local package directory: {}\n", localPackageDir.string());
    }

    return info;
}

json PackageManager::loadInstallLog() const
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

std::vector<std::string> PackageManager::get_info_from_package(const std::string &packageRef, const std::string &infoType) const
{
    json j = loadInstallLog();
    if (!j.contains("graph") || !j["graph"].contains("nodes"))
        return {};

    for (auto &[key, node] : j["graph"]["nodes"].items())
    {
        if (!node.contains("ref"))
            continue;

        if (std::string ref = node["ref"]; ref.find(packageRef) == 0)
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
                    auto paths =
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

bool PackageManager::checkIfInstalled(const std::string &packageRef) const
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

            if (std::string ref = node["ref"]; ref.find(packageRef) == 0)
                return true;
        }
    }
    catch (const CPPX_Exception &)
    {
        return false;
    }
    return false;
}


Toolchain::Toolchain(std::string c, fs::path cp, std::string cv) : compilerName(std::move(c)), compilerPath(std::move(cp)), compilerVersion(std::move(cv))
{
}

ProjectConfig::ProjectConfig(std::string p, std::string n, Toolchain t) : path(std::move(p)), name(std::move(n)), toolchain(std::move(t))
{
}

BuildSettings::BuildSettings(std::string on, const buildType bt) : outputName(std::move(on)), btype(bt)
{
}

ProjectSettings::ProjectSettings(std::string n, const std::vector<std::string> &iff, const std::vector<std::string> &s,
                                 const std::vector<std::string> &ip, const std::vector<std::string> &igs,
                                 const std::vector<std::string> &igf, const std::unordered_map<std::string, std::string> &d,
                                 const std::vector<std::string> &staticLinkFiles, const std::vector<std::string> &LinkDirs,
                                 const std::unordered_map<std::string, std::string> &extra, BuildSettings bset,
                                 std::string version, const std::vector<std::string> &authors, std::string description,
                                 std::string license, std::string github_username, std::string github_repo,
                                 std::unordered_map<std::string, std::string> defines)
    : name(std::move(n)), includefiles(iff), srcfiles(s), includepaths(ip), ignoredPaths(igs), ignoredFiles(igf),
      staticLinkFiles(staticLinkFiles), LinkDirs(LinkDirs), dependencies(d), extra(extra), buildsettings(std::move(bset)),
      version(std::move(version)), authors(authors), description(std::move(description)), license(std::move(license)),
      github_username(std::move(github_username)), github_repo(std::move(github_repo)), defines(std::move(defines))
{
}

ProjectConfig getCurrentProject()
{
    fs::path pathToGlobal;
#if defined(__linux__) || defined(__APPLE__)
    const char* homeDir = std::getenv("HOME");
    if (!homeDir) {
        throw CPPX_Exception("Failed to get HOME directory.");
    }
    pathToGlobal = homeDir;
    pathToGlobal /= ".cppxglobal.toml";
#elif defined(_WIN32)
    const char* userProfile = std::getenv("USERPROFILE");
    if (!userProfile) {
        throw CPPX_Exception("Failed to get USERPROFILE directory.");
    }
    pathToGlobal = userProfile;
    pathToGlobal /= ".cppxglobal.toml";
#else
    throw CPPX_Exception("Unsupported OS.");
#endif

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
    if (const auto project = file["project"].as_table())
    {
        name = (*project)["name"].value_or<std::string>("no name");
        path = (*project)["path"].value_or<std::string>("no path");
    }
    else
    {
        throw CPPX_Exception("No project set! Use 'cppx project set'.");
    }
    if (const auto toolchain = file["toolchain"].as_table())
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
    return {path, name, tc};
}

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
            if (auto author_str = (*metadata)["authors"].value_or<std::string>(""); !author_str.empty())
                authors.push_back(author_str);
        }
    }

    BuildSettings bset;
    if (auto build = config["build"].as_table())
    {
        bset.outputName = (*build)["build_name"].value_or<std::string>("_default");
        if (auto t = (*build)["build_type"].value_or<std::string>("_default"); t == "executable" || t == "_default")
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

    return {proj.name, includefiles, srcfiles, includedirs, ignoredDirs, ignoredFiles, dep,
                           staticLinkFiles, LinkDirs, extra, bset, version, authors, description, license,
                           github_username, github_repo, defines};
}

std::string pickCompiler()
{
    const ProjectConfig pc = getCurrentProject();

    if (const ProjectSettings ps = getProjectSettings(); ps.extra.contains("compiler"))
    {
        return ps.extra.at("compiler");
    }
    else
    {
        return pc.toolchain.compilerPath;
    }
}

void setExtra(const std::string &name, const std::string &value)
{
    ProjectConfig pc = getCurrentProject();
    toml::table table = toml::parse_file(pc.path + "/config.toml");

    if (auto *extra = table["extra"].as_table())
    {
        extra->insert_or_assign(name, value);
    }
    else
    {
        table.insert("extra", toml::table{{name, value}});
    }

    std::ofstream ofs(pc.path + "/config.toml");
    ofs << table;
}

void setMetadata(const std::string &key, const std::string &value)
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
void setMetadata(const std::string &key, const std::vector<std::string> &values)
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


FileWatcher::FileWatcher(fs::path dir, Callback cb, const std::chrono::milliseconds interval)
    : _dir(std::move(dir)), _cb(std::move(cb)), _interval(interval)
{
    _snapshot = snapshot_dir();
}

void FileWatcher::run(const std::stop_token &stoken)
{
    while (!stoken.stop_requested())
    {
        std::this_thread::sleep_for(_interval);
        std::unordered_map<fs::path, fs::file_time_type> current;
        try
        {
            current = snapshot_dir();
        }
        catch (const std::filesystem::filesystem_error &e)
        {
            fmt::print(stderr, fmt::emphasis::bold | fg(fmt::color::red),
                       "[FileWatcher] Filesystem error: {}\n", e.what());
            continue;
        }
        catch (const std::exception &e)
        {
            fmt::print(stderr, fmt::emphasis::bold | fg(fmt::color::red),
                       "[FileWatcher] Unexpected error: {}\n", e.what());
            continue;
        }


        for (const auto &p : current | std::views::keys)
        {
            if (!_snapshot.contains(p))
            {
                _cb(p, true);
            }
        }
        for (const auto &p : _snapshot | std::views::keys)
        {
            if (!current.contains(p))
            {
                _cb(p, false);
            }
        }

        _snapshot = std::move(current);
    }
}

std::unordered_map<fs::path, fs::file_time_type> FileWatcher::snapshot_dir(fs::path const &dir)
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

std::unordered_map<fs::path, fs::file_time_type> FileWatcher::snapshot_dir() const
{
    return snapshot_dir(_dir);
}
