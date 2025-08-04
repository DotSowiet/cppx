/*

 ┌─────────────────────────────────────────────────────────────────────────────┐
 │                               ###  ###   ###  #  #                          │
 │                              #     ###   ###   #                            │
 │                               ###  #     #    # #                           │
 ├─────────────────────────────────────────────────────────────────────────────┤
 │                                CPPX - C++ Project Manager                   │
 │                                                                             │
 │ CPPX is a CLI application that provides a unified interface to manage       │
 │ C++ projects seamlessly.                                                    │
 │                                                                             │
 │ Features:                                                                   │
 │   • Create, build, and run C++ projects           🗸                         │
 │   • Manage dependencies and libraries             🗸                         │
 │   • Configure project settings and build system   🗸                         │
 │   • Handle documentation (Doxygen)                🗸                         │
 │   • Clean project artifacts                       🗸                         │
 │   • Run tests, examples, and benchmarks           🗸                         │
 │                                                                             │
 │ Streamlining your C++ project workflow in one powerful tool.                │
 └─────────────────────────────────────────────────────────────────────────────┘

*/
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
#include <csignal>

#include <fmt/color.h>
#include <fmt/core.h>
#include <fmt/format.h>

#include <CLI/CLI.hpp>
#include <nlohmann/json.hpp>
#include <toml++/toml.hpp>

#include "github.hpp"
#include "helpers.hpp"

int main(int argc, char **argv)
{
    CLI::App app{"cppx — project manager for C++"};

    // ─────────────────────────────────────────────────────────────────
    // Global flags
    bool verbose_output = false;
    app.add_flag("-v,--verbose", verbose_output, "Displays detailed logs");

    // ─────────────────────────────────────────────────────────────────
    // project subcommands
    auto project = app.add_subcommand("project", "project operations");

    // project new
    std::string projectName;
    auto projectNew = project->add_subcommand("new", "creates a new project");
    projectNew->add_option("-n,--name", projectName, "Project name")->required();

    // project set
    std::string projectNameSet;
    std::string projectPath;
    auto projectSet = project->add_subcommand("set", "sets the current project");
    projectSet->add_option("-n,--name", projectNameSet, "Project name")->required();
    projectSet->add_option("-p,--path", projectPath, "Project path")->required();

    // ─────────────────────────────────────────────────────────────────
    // build & run
    bool build_debug = false;
    std::string build_config;
    auto build = app.add_subcommand("build", "Builds the project");
    build->add_flag("-d,--debug", build_debug, "Builds the project in debug mode");
    build->add_option("-c,--config", build_config, "Build configuration (e.g., debug, release, custom)");

    auto run = app.add_subcommand("run", "Runs the project");

    // ─────────────────────────────────────────────────────────────────
    // watch
    auto watch = app.add_subcommand("watch", "Updates configuration after adding files");
    std::string dir;
    bool watchforce = false;
    watch->add_option("-d,--dir", dir, "Monitored directory")->required();
    watch->add_flag("-f,--force", watchforce, "Forces the watch command to run in the foreground");

    // ─────────────────────────────────────────────────────────────────
    // ignore
    auto ignore = app.add_subcommand("ignore", "adds file/files/directory/directories to the ignored list");
    std::vector<fs::path> directories;
    ignore->add_option("elements", directories, "Elements to ignore")->expected(-1)->required();

    // ─────────────────────────────────────────────────────────────────
    // package management
    auto package = app.add_subcommand("pkg", "Package management commands");

    // pkg install
    auto install = package->add_subcommand("install", "Installs a package");
    std::string packageName;
    std::string packageVersion;
    install->add_option("name", packageName, "Package name")->required();
    install->add_option("-v,--version", packageVersion, "Package version")->required();

    // pkg remove
    auto remove = package->add_subcommand("remove", "Removes a package");
    std::string packageToRemove;
    remove->add_option("name", packageToRemove, "Package name")->required();

    // ─────────────────────────────────────────────────────────────────
    // export
    auto export_cmd = app.add_subcommand("export", "Exports the configuration file to another format");
    std::string expr;
    export_cmd->add_option("exportTo", expr, "Format to export the configuration to");

    // config
    auto config_cmd = app.add_subcommand("config", "Configures project settings");
    std::string what;
    config_cmd->add_option("Setting", what, "Which settings to change (format: X=Y)");

    // profile, doc, clean, test, metadata, info
    auto profile = app.add_subcommand("profile", "Creates toolchain information");
    auto doc = app.add_subcommand("doc", "Generates documentation using Doxygen");
    auto clean = app.add_subcommand("clean", "Removes build artifacts");
    auto test = app.add_subcommand("test", "Runs tests");
    auto metadata = app.add_subcommand("metadata", "Adds metadata to config.toml");
    auto info = app.add_subcommand("info", "Displays project information");

    // format
    auto format = app.add_subcommand("format", "Formats the project files");

    std::vector<std::string> range{};

    format->add_option("Files", range, "Sets file to format");

    // ─────────────────────────────────────────────────────────────────
    // Parse & dispatch
    try
    {
        CLI11_PARSE(app, argc, argv);

        if (projectNew->parsed())
            handle_project_new(projectName);
        else if (projectSet->parsed())
            handle_project_set(projectNameSet, projectPath);
        else if (build->parsed())
            handle_build(build_debug, build_config);
        else if (run->parsed())
            handle_run();
        else if (watch->parsed())
            handle_watch(dir, watchforce);
        else if (ignore->parsed())
            handle_ignore(directories);
        else if (install->parsed())
            handle_pkg_install(packageName, packageVersion);
        else if (remove->parsed())
            handle_pkg_remove(packageToRemove);
        else if (export_cmd->parsed())
            handle_export(expr);
        else if (config_cmd->parsed())
            handle_config_set(what);
        else if (profile->parsed())
            handle_profile();
        else if (doc->parsed())
            handle_doc();
        else if (clean->parsed())
            handle_clean();
        else if (test->parsed())
            handle_test();
        else if (metadata->parsed())
            handle_metadata();
        else if (info->parsed())
            handle_info();
        else if (format->parsed())
            handle_fmt(range);
        else
            throw CPPX_Exception("Unknown command or missing required arguments.");
    }
    catch (const CLI::ParseError &e)
    {
        return app.exit(e);
    }
    catch (const CPPX_Exception &e)
    {
        fmt::print(stderr, fmt::emphasis::bold | fg(fmt::color::red), "[ERROR] {}\n", e.what());
        return 1;
    }
    catch (const std::exception &e)
    {
        fmt::print(stderr, fmt::emphasis::bold | fg(fmt::color::red), "[CRITICAL ERROR] {}\n", e.what());
        return 1;
    }

    return 0;
}

void handle_project_new(const std::string &projectName)
{
    fmt::print(fmt::emphasis::bold | fg(fmt::color::green), "\nCreating new project: {}\n\n", projectName);

    fs::create_directories(projectName);
    auto project_root = fs::path(projectName);

    toml::table config;
    config.insert_or_assign("name", projectName);

    fs::path srcpath = project_root / "src" / "main.cpp";
    fs::path includepath = project_root / "include" / "main.hpp";
    fs::path testpath = project_root / "tests" / "main.test.cpp";

    fs::create_directories(srcpath.parent_path());
    fs::create_directories(includepath.parent_path());
    fs::create_directories(testpath.parent_path());

    std::ofstream(srcpath) << R"(#include <main.hpp>

int main()
{
    printhelloworld();
    return 0;
}
)";

    std::ofstream(includepath) << R"(#pragma once
#include <iostream>

/**
 * @brief Prints "Hello, world!" to the console.
 */
void printhelloworld()
{
    std::cout << "Hello, world!\n";
}

)";

    std::ofstream(testpath) << R"(#include <cassert>
#include <main.hpp>

int main() {
    // This is a placeholder test.
    // Replace with a real testing framework like GTest or Catch2.
    printhelloworld(); 
    assert(true);
    return 0;
}
)";

    std::ofstream(project_root / ".gitignore") << R"(# Build artifacts
build/
*.o
*.a
*.so
*.dll
*.exe

# Doxygen docs
docs/

# Vendor folder for dependencies
vendor/

# IDE files
.vscode/
.idea/
*.suo
*.user
)";

    toml::table src;
    src.insert_or_assign("src files", toml::array{"src/main.cpp"});
    src.insert_or_assign("include files", toml::array{"include/main.hpp"});
    src.insert_or_assign("include directories", toml::array{"include"});
    src.insert_or_assign("static_linked", toml::array{});
    src.insert_or_assign("static_linked_dirs", toml::array{});

    config.insert_or_assign("source", src);
    config.insert_or_assign("dependencies", toml::table{});
    config.insert_or_assign("ignore", toml::table{{"files", toml::array{}}, {"dirs", toml::array{}}});
    config.insert_or_assign("build", toml::table{{"build_name", projectName}, {"build_type", "executable"}});

    std::ofstream cfg(project_root / "config.toml");
    cfg << config;

    fmt::print(fmt::emphasis::bold | fg(fmt::color::light_green), "Project '{}' successfully created.\n", projectName);
}

void handle_project_set(const std::string &projectName, const std::string &projectPath)
{
    fmt::print(fmt::emphasis::bold | fg(fmt::color::green), "\nSetting current project to: {}\n\n", projectName);

    fs::path pathToGlobal;
#if defined(__linux__) || defined(__APPLE__)
    const char *homeDir = std::getenv("HOME");
    if (!homeDir)
    {
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

    toml::table projectconfig;
    if (file.contains("project"))
    {
        projectconfig = *file["project"].as_table();
    }

    fs::path p = fs::canonical(fs::absolute(fs::path(projectPath)));
    projectconfig.insert_or_assign("path", p.string());
    projectconfig.insert_or_assign("name", projectName); // BUG FIX: Save the project name
    file.insert_or_assign("project", projectconfig);

    if (std::ofstream cfg(pathToGlobal); cfg.is_open())
    {
        cfg << file;
        fmt::print(fg(fmt::color::light_green) | fmt::emphasis::bold, "Updated global project configuration in: {}\n\n",
                   pathToGlobal.string());
    }
    else
    {
        throw CPPX_Exception(fmt::format("Failed to open file for writing: {}", pathToGlobal.string()));
    }
}

void handle_build(bool debug, const std::string &build_config)
{
    auto start = std::chrono::high_resolution_clock::now();
    ProjectConfig proj = getCurrentProject();
    ProjectSettings ps = getProjectSettings();
    std::string compiler = pickCompiler();

    fs::path build_dir = fs::path(proj.path) / "build";
    fs::create_directories(build_dir);

    std::string command = compiler + " ";

    toml::table config = toml::parse_file(fmt::format("{}/config.toml", proj.path));

    std::string output_name = ps.buildsettings.outputName;
    std::vector<std::string> extra_flags;
    if (!build_config.empty() && config.contains("configurations"))
    {
        if (auto configs = config["configurations"].as_table(); configs && configs->contains(build_config))
        {
            if (auto conf = (*configs)[build_config].as_table())
            {
                if (conf->contains("flags"))
                {
                    if (auto arr = (*conf)["flags"].as_array())
                    {
                        for (const auto &f : *arr)
                        {
                            if (auto s = f.value<std::string>())
                                extra_flags.push_back(*s);
                        }
                    }
                }
                if (conf->contains("output"))
                {
                    output_name = (*conf)["output"].value_or(output_name);
                }
            }
        }
        else
        {
            fmt::print(stderr, fmt::emphasis::bold | fg(fmt::color::yellow),
                       "[WARNING] Configuration '{}' not found. Using default.\n", build_config);
        }
    }

    for (const auto &flag : extra_flags)
    {
        command += flag + " ";
    }

    for (const auto &inc : ps.includepaths)
    {
        if (fs::path incPath = inc; incPath.is_absolute())
            command += fmt::format("-I\"{}\" ", incPath.string());
        else
            command += fmt::format("-I\"{}\" ", (fs::path(proj.path) / incPath).string());
    }

    for (const auto &src : ps.srcfiles)
    {
        command += fmt::format("\"{}\" ", (fs::path(proj.path) / src).string());
    }

    for (const auto &lib : ps.staticLinkFiles)
    {
        if (fs::path libPath = lib; libPath.has_extension() &&
                                    (libPath.extension() == ".a" || libPath.extension() == ".so" || libPath.extension()
                                     == ".lib"))
        {
            command += fmt::format("\"{}\" ", libPath.string());
        }
        else
        {
            command += "-l" + lib + " ";
        }
    }

    for (const auto &libpath : ps.LinkDirs)
    {
        command += fmt::format("-L\"{}\" ", libpath);
    }

    for (const auto &[name, value] : ps.defines)
    {
        command += fmt::format("-D{}=\"{}\" ", name, value);
    }

    switch (ps.buildsettings.btype)
    {
    case buildType::BUILD_EXECUTABLE:
        command += "-o ";
        command += fmt::format("\"{}/{}\"", build_dir.string(), output_name);
        if (debug)
            command += " -g";
        break;

    case buildType::BUILD_DYNAMICLINK: {
        command += "-shared -fPIC ";
        std::string so_name = fmt::format("lib{}.so", output_name);
        command += "-o ";
        command += fmt::format("\"{}/{}\"", build_dir.string(), so_name);
        break;
    }

    case buildType::BUILD_STATICLINK: {
        std::vector<std::string> object_files;
        for (const auto &src : ps.srcfiles)
        {
            fs::path srcPath = fs::path(proj.path) / src;
            std::string obj_path = (build_dir / (srcPath.stem().string() + ".o")).string();
            std::string compile_cmd = compiler + " ";
            for (const auto &flag : extra_flags)
                compile_cmd += flag + " ";
            for (const auto &inc : ps.includepaths)
            {
                auto incPath = fs::path(inc);
                std::string incDir =
                    incPath.is_absolute() ? incPath.string() : (fs::path(proj.path) / incPath).string();
                compile_cmd += fmt::format("-I\"{}\" ", incDir);
            }
            compile_cmd += fmt::format("-c \"{}\" -o {}", srcPath.string(), obj_path);
            LOG_VERBOSE("Compiling: {}\n", compile_cmd);
            if (std::system(compile_cmd.c_str()) != 0)
                throw CPPX_Exception(fmt::format("Compilation of {} failed.", srcPath.string()));
            object_files.push_back(obj_path);
        }
        std::string a_name = fmt::format("lib{}.a", output_name);
        std::string ar_cmd = fmt::format("ar rcs {} {}", (build_dir / a_name).string(), fmt::join(object_files, " "));
        LOG_VERBOSE("Creating static library: {}\n", ar_cmd);
        if (std::system(ar_cmd.c_str()) != 0)
            throw CPPX_Exception("Static archive creation failed.");
        fmt::print(fmt::emphasis::bold | fg(fmt::color::green), "Successfully built static library: {}/{}\n",
                   build_dir.string(), a_name);
        return;
    }

    default:
        throw CPPX_Exception("Unsupported buildType!");
    }

    fmt::print(fmt::emphasis::bold | fg(fmt::color::yellow), "Building project...\n");
    LOG_VERBOSE("Executing command: {}\n", command);
    if (std::system(command.c_str()) != 0)
        throw CPPX_Exception("Build failed.");

    std::string final_name =
        (ps.buildsettings.btype == buildType::BUILD_DYNAMICLINK) ? fmt::format("lib{}.so", output_name) : output_name;

    auto end = std::chrono::high_resolution_clock::now();

    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    fmt::print(fmt::emphasis::bold | fg(fmt::color::green), "Successfully built: {}/{} in {}ms\n\n", build_dir.string(),
               final_name, duration.count());
}

void handle_run()
{
    const ProjectConfig proj = getCurrentProject();
    const ProjectSettings ps = getProjectSettings();
    const fs::path executable_path = fs::path(proj.path) / "build" / ps.buildsettings.outputName;

    if (!fs::exists(executable_path))
    {
        fmt::print(fg(fmt::color::yellow), "Executable file does not exist. Starting compilation...\n");
        handle_build(false, "");
    }

    const std::string command = executable_path.string();
    fmt::print(fmt::emphasis::bold | fg(fmt::color::green), "\nRunning project: {}\n\n", command);
    if (const int ret = std::system(command.c_str()); ret != 0)
    {
        throw CPPX_Exception("Failed to run project.");
    }
}

void handle_watch(const std::string &dir, const bool force)
{
    using namespace std::chrono_literals;
    if (tcgetpgrp(STDIN_FILENO) == getpgrp() && !force)
    {
        throw CPPX_Exception("The watch command must be run in the background (with &).\n"
            "Use -f (--force) to run it in the foreground.");
    }

    ProjectConfig pc = getCurrentProject();
    fs::path watch_dir;
    if (dir == "src")
    {
        watch_dir = fs::path(pc.path) / "src";
    }
    else
    {
        throw CPPX_Exception(fmt::format("Invalid directory: {}, available: src", dir));
    }

    if (!fs::exists(watch_dir) || !fs::is_directory(watch_dir))
    {
        throw CPPX_Exception(fmt::format("Directory does not exist: {}", watch_dir.string()));
    }

    fmt::print(fmt::emphasis::bold | fg(fmt::color::green), "\nMonitoring directory: {}\n\n", watch_dir.string());

    auto cb = [&](const fs::path &filename, bool created) {
        auto name = filename.string();
        try
        {
            const std::string configpath = fmt::format("{}/config.toml", pc.path);
            toml::table tbl = toml::parse_file(configpath);

            ProjectSettings projset = getProjectSettings();
            if (created)
            {
                fmt::print(fmt::emphasis::bold | fg(fmt::color::green), "File added: {}\n", name);
                if (std::ranges::find(projset.ignoredFiles, name) !=
                    projset.ignoredFiles.end())
                {
                    fmt::print(fmt::emphasis::bold | fg(fmt::color::yellow), "Ignoring file: {}\n\n", name);
                    return;
                }
                if (auto *arr = tbl["source"]["src files"].as_array())
                {
                    arr->push_back(fmt::format("src/{}", name));
                }
            }
            else
            {
                fmt::print(fmt::emphasis::bold | fg(fmt::color::red), "File removed: {}\n", name);
                if (auto *arr = tbl["source"]["src files"].as_array())
                {
                    toml::array new_arr;
                    for (auto const &node : *arr)
                    {
                        if (auto const *str = node.as_string())
                        {
                            if (*str == fmt::format("src/{}", name))
                                continue;
                        }
                        new_arr.push_back(node);
                    }
                    *arr = std::move(new_arr);
                }
            }

            std::ofstream out(configpath);
            out << tbl;
            fmt::print(fmt::emphasis::bold | fg(fmt::color::green), "Configuration updated: {}\n\n", configpath);
        }
        catch (std::exception const &e)
        {
            fmt::print(stderr, fg(fmt::color::red), "Error during watch callback: {}\n", e.what());
        }
    };

    // Signal handling for graceful shutdown
    static std::atomic_bool stop_requested{false};
    auto signal_handler = [](int) {
        stop_requested = true;
    };
    std::signal(SIGINT, signal_handler);

    std::jthread watcher_thread([&](const std::stop_token &st) {
        FileWatcher fw(watch_dir, cb, std::chrono::seconds(1));
        while (!st.stop_requested() && !stop_requested)
        {
            fw.run(st);
        }
    });
    watcher_thread.join();
}

void handle_ignore(const std::vector<fs::path> &directories)
{
    ProjectConfig pc = getCurrentProject();
    std::string config_path = fmt::format("{}/config.toml", pc.path);

    toml::table tbl;
    try
    {
        tbl = toml::parse_file(config_path);
    }
    catch (const toml::parse_error &err)
    {
        throw CPPX_Exception(fmt::format("Failed to parse TOML file: {}", err.description()));
    }

    if (!tbl.contains("ignore"))
    {
        tbl.insert("ignore", toml::table{});
    }
    toml::table &ignore_tbl = *tbl["ignore"].as_table();

    if (!ignore_tbl.contains("dirs"))
    {
        ignore_tbl.insert("dirs", toml::array{});
    }
    if (!ignore_tbl.contains("files"))
    {
        ignore_tbl.insert("files", toml::array{});
    }

    toml::array &dirs_arr = *ignore_tbl["dirs"].as_array();
    toml::array &files_arr = *ignore_tbl["files"].as_array();

    for (auto &path : directories)
    {
        if (fs::exists(path))
        {
            fmt::print(fmt::emphasis::bold | fg(fmt::color::green), "Ignoring: {}\n", path.string());

            if (fs::is_directory(path))
            {
                updateTomlArray(dirs_arr, path.string(), "ignored directory");
            }
            else
            {
                updateTomlArray(files_arr, path.string(), "ignored file");
            }
        }
        else
        {
            fmt::print(fmt::emphasis::bold | fg(fmt::color::red), "Path does not exist: {}\n", path.string());
        }
    }

    std::ofstream out(config_path);
    if (!out)
    {
        throw CPPX_Exception(fmt::format("Failed to open {} for writing", config_path));
    }
    out << tbl;
    fmt::print(fmt::emphasis::bold | fg(fmt::color::green), "Successfully updated config.toml with ignored list.\n");
}

void handle_pkg_install(const std::string &packageName, const std::string &packageVersion)
{
    ProjectConfig pc = getCurrentProject();
    PackageManager pkg(fmt::format("{}/vendor/", pc.path));
    std::string FullName = fmt::format("{}/{}", packageName, packageVersion);

    pkg.install(FullName);

    auto pkgInfo = pkg.getPackageInfo(FullName);

    LOG_VERBOSE("Retrieved headers: {}\n", pkgInfo.includePaths);
    LOG_VERBOSE("Retrieved library paths: {}\n", pkgInfo.libPaths);
    LOG_VERBOSE("Retrieved libraries: {}\n", pkgInfo.libs);

    std::string config_path = fmt::format("{}/config.toml", pc.path);
    toml::table tbl = toml::parse_file(config_path);

    if (!tbl.contains("dependencies"))
        tbl.insert("dependencies", toml::table{});
    toml::table &dependencies_tbl = *tbl["dependencies"].as_table();
    dependencies_tbl.insert_or_assign(packageName, packageVersion);

    if (!tbl.contains("source"))
        tbl.insert("source", toml::table{});
    toml::table &source_tbl = *tbl["source"].as_table();

    if (!source_tbl.contains("include directories"))
        source_tbl.insert("include directories", toml::array{});
    if (!source_tbl.contains("static_linked"))
        source_tbl.insert("static_linked", toml::array{});
    if (!source_tbl.contains("static_linked_dirs"))
        source_tbl.insert("static_linked_dirs", toml::array{});

    toml::array &include_dirs_arr = *source_tbl["include directories"].as_array();
    toml::array &static_linked_arr = *source_tbl["static_linked"].as_array();
    toml::array &static_linked_dirs_arr = *source_tbl["static_linked_dirs"].as_array();

    for (const auto &include : pkgInfo.includePaths)
        updateTomlArray(include_dirs_arr, include, "include directory");
    for (const auto &lib : pkgInfo.libs)
        updateTomlArray(static_linked_arr, lib, "library");
    for (const auto &libpath : pkgInfo.libPaths)
        updateTomlArray(static_linked_dirs_arr, libpath, "library directory");

    std::ofstream out(config_path);
    out << tbl;

    fmt::print(fmt::emphasis::bold | fg(fmt::color::green), "Successfully updated config.toml.\n");
}

void handle_pkg_remove(const std::string &packageToRemove)
{
    const ProjectConfig pc = getCurrentProject();

    if (PackageManager pkg(fmt::format("{}/vendor/", pc.path)); pkg.checkIfInstalled(packageToRemove))
    {
        fmt::print(fmt::emphasis::bold | fg(fmt::color::yellow),
                   "Are you sure you want to remove package '{}'? [Y/n]: ", packageToRemove);
        std::string confirmation;
        std::getline(std::cin, confirmation);
        if (confirmation != "Y" && confirmation != "y")
        {
            fmt::print(fmt::emphasis::bold | fg(fmt::color::red), "Package removal cancelled.\n");
            return;
        }

        auto package_info = pkg.remove(packageToRemove);
        if (!package_info)
        {
            throw CPPX_Exception(fmt::format("Could not get package info for '{}' to remove.", packageToRemove));
        }

        std::string config_path = fmt::format("{}/config.toml", pc.path);
        toml::table tbl = toml::parse_file(config_path);

        if (tbl.contains("dependencies"))
        {
            if (toml::table &dependencies_tbl = *tbl["dependencies"].as_table(); dependencies_tbl.contains(
                packageToRemove))
            {
                dependencies_tbl.erase(packageToRemove);
            }
        }

        if (tbl.contains("source"))
        {
            toml::table &source_tbl = *tbl["source"].as_table();
            if (source_tbl.contains("include directories"))
            {
                removeFromTomlArray(*source_tbl["include directories"].as_array(), package_info->includePaths,
                                    "include directory");
            }
            if (source_tbl.contains("static_linked"))
            {
                removeFromTomlArray(*source_tbl["static_linked"].as_array(), package_info->libs, "library");
            }
            if (source_tbl.contains("static_linked_dirs"))
            {
                removeFromTomlArray(*source_tbl["static_linked_dirs"].as_array(), package_info->libPaths,
                                    "library directory");
            }
        }
        std::ofstream out(config_path);
        out << tbl;
        fmt::print(fmt::emphasis::bold | fg(fmt::color::green),
                   "Successfully removed package and updated configuration.\n");
    }
    else
    {
        throw CPPX_Exception(fmt::format("Package '{}' is not installed.", packageToRemove));
    }
}

void handle_export(const std::string &format)
{
    ProjectConfig pc = getCurrentProject();
    ProjectSettings ps = getProjectSettings();
    std::string name = replace_spaces(ps.name);
    if (format == "cmake")
    {
        std::ostringstream file;
        file << "cmake_minimum_required(VERSION 3.10)\n";
        file << "project(" << name << ")\n";
        file << "add_executable(" << name << " ";
        for (const auto &src : ps.srcfiles)
            file << src << " ";
        file << ")\n";

        if (!ps.includepaths.empty())
        {
            file << "target_include_directories(" << name << " PRIVATE ";
            for (size_t i = 0; i < ps.includepaths.size(); ++i)
            {
                if (isAbsolutePath(ps.includepaths[i]))
                    file << ps.includepaths[i];
                else
                    file << "${CMAKE_CURRENT_SOURCE_DIR}/" << ps.includepaths[i];

                if (i != ps.includepaths.size() - 1)
                    file << " ";
            }
            file << ")\n";
        }

        if (!ps.LinkDirs.empty())
        {
            file << "target_link_directories(" << name << " PRIVATE ";
            for (size_t i = 0; i < ps.LinkDirs.size(); ++i)
            {
                file << ps.LinkDirs[i];
                if (i != ps.LinkDirs.size() - 1)
                    file << " ";
            }
            file << ")\n";
        }

        if (!ps.staticLinkFiles.empty())
        {
            file << "target_link_libraries(" << name << " PRIVATE ";
            for (size_t i = 0; i < ps.staticLinkFiles.size(); ++i)
            {
                file << ps.staticLinkFiles[i];
                if (i != ps.staticLinkFiles.size() - 1)
                    file << " ";
            }
            file << ")\n";
        }

        if (!ps.dependencies.empty())
        {
            file << "# Dependencies:\n";
            for (const auto &[fst, snd] : ps.dependencies)
            {
                file << "# " << fst << " " << snd << "\n";
            }
        }

        std::string outPath = pc.path + "/CMakeLists.txt";

        if (std::ofstream out(outPath); out.is_open())
        {
            out << file.str();
            fmt::print(fmt::emphasis::bold | fmt::fg(fmt::color::green), "Generated {}\n", outPath);
        }
        else
        {
            throw CPPX_Exception(fmt::format("Failed to save CMakeLists to {}", outPath));
        }
    }
    else
    {
        throw CPPX_Exception(fmt::format("Unsupported export format: {}", format));
    }
}

void handle_config_set(const std::string &what)
{
    const size_t pos = what.find('=');
    if (pos == std::string::npos)
    {
        throw CPPX_Exception("Invalid configuration, use format XYZ=ZYX");
    }
    const std::string left = what.substr(0, pos);
    const std::string right = what.substr(pos + 1);
    fmt::print(fmt::emphasis::bold | fg(fmt::color::green), "\nSetting configuration: {}={}\n", left, right);

    if (left == "compiler")
    {
        if (right != "clang" && right != "g++" && right != "gcc")
        {
            throw CPPX_Exception("cppx does not support compilers other than clang or gcc");
        }
        setExtra(left, right);
    }
    else
    {
        throw CPPX_Exception(fmt::format("Unknown setting: {}", left));
    }
}

void handle_profile()
{
    std::vector<std::string> compilers = {"gcc", "g++", "clang", "clang++"};
    struct CompilerInfo
    {
        std::string name;
        std::string version;
        std::string path;
    };
    std::vector<CompilerInfo> found;
    for (const auto &compiler : compilers)
    {
        std::string which_cmd = fmt::format("which {} 2>/dev/null", compiler);
        std::string path; {
            std::array<char, 128> buffer{};
            std::string result;
            if (FILE *pipe = popen(which_cmd.c_str(), "r"))
            {
                while (fgets(buffer.data(), buffer.size(), pipe) != nullptr)
                {
                    result += buffer.data();
                }
                pclose(pipe);
            }
            path = result;
            if (!path.empty() && path.back() == '\n')
                path.pop_back();
        }
        if (!path.empty())
        {
            std::string version_cmd = fmt::format("{} --version 2>/dev/null", compiler);
            std::string version; {
                std::array<char, 256> buffer{};
                std::string result;
                if (FILE *pipe = popen(version_cmd.c_str(), "r"))
                {
                    while (fgets(buffer.data(), buffer.size(), pipe) != nullptr)
                    {
                        result += buffer.data();
                    }
                    pclose(pipe);
                }
                if (size_t pos = result.find('\n'); pos != std::string::npos)
                    version = result.substr(0, pos);
                else
                    version = result;
            }
            found.push_back({compiler, version, path});
        }
    }
    if (found.empty())
    {
        throw CPPX_Exception("No compilers found!");
    }
    fmt::print(fmt::emphasis::bold | fg(fmt::color::green), "\nFound compilers:\n");
    for (size_t i = 0; i < found.size(); ++i)
    {
        fmt::print(fmt::emphasis::bold | fg(fmt::color::green), "  [{}] {} ({})\n", i + 1, found[i].name,
                   found[i].version);
    }
    size_t chosen = 0;
    if (found.size() > 1)
    {
        fmt::print(fmt::emphasis::bold | fg(fmt::color::yellow), "\nChoose compiler [1-{}]: ", found.size());
        std::string input;
        std::getline(std::cin, input);
        try
        {
            chosen = std::stoul(input);
            if (chosen < 1 || chosen > found.size())
                throw std::out_of_range("bad");
            chosen -= 1;
        }
        catch (...)
        {
            throw CPPX_Exception("Invalid selection!");
        }
    }
    fmt::print(fmt::emphasis::bold | fg(fmt::color::green), "\nChosen: {} ({})\n", found[chosen].name,
               found[chosen].version);
    fs::path pathToGlobal;
#if defined(__linux__) || defined(__APPLE__)
    const char *homeDir = std::getenv("HOME");
    if (!homeDir)
    {
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
    if (fs::exists(pathToGlobal))
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
    toml::table toolchain;
    toolchain.insert_or_assign("compiler", found[chosen].name);
    toolchain.insert_or_assign("version", found[chosen].version);
    toolchain.insert_or_assign("path", found[chosen].path);
    file.insert_or_assign("toolchain", toolchain);
    if (std::ofstream cfg(pathToGlobal); cfg.is_open())
    {
        cfg << file;
        fmt::print(fg(fmt::color::light_green) | fmt::emphasis::bold, "Updated global toolchain in: {}\n",
                   pathToGlobal.string());
    }
    else
    {
        throw CPPX_Exception(fmt::format("Failed to save to file: {}", pathToGlobal.string()));
    }
}

void handle_doc()
{
    ProjectConfig proj = getCurrentProject();

    if (fs::path doxyfile_path = fs::path(proj.path) / "Doxyfile"; !fs::exists(doxyfile_path))
    {
        fmt::print(fmt::emphasis::bold | fg(fmt::color::yellow), "Doxyfile does not exist. Creating default...\n");

        ProjectSettings ps = getProjectSettings();

        if (std::ofstream doxyfile(doxyfile_path); doxyfile.is_open())
        {
            doxyfile << "PROJECT_NAME           = \"" << ps.name << "\"\n";
            doxyfile << "OUTPUT_DIRECTORY       = docs\n";
            doxyfile << "INPUT                  = ./src ./include\n";
            doxyfile << "RECURSIVE              = YES\n";
            doxyfile << "GENERATE_LATEX         = NO\n";
            doxyfile << "EXTRACT_ALL            = YES\n";
            doxyfile << "EXTRACT_PRIVATE        = YES\n";
            doxyfile << "EXTRACT_STATIC         = YES\n";
            doxyfile.close();
            fmt::print(fmt::emphasis::bold | fg(fmt::color::green), "Default Doxyfile created.\n");
        }
        else
        {
            throw CPPX_Exception(fmt::format("Failed to create Doxyfile in '{}'", doxyfile_path.string()));
        }
    }

    fmt::print(fmt::emphasis::bold | fg(fmt::color::green), "Generating Doxygen documentation...\n");

    fs::path original_path = fs::current_path();
    fs::current_path(proj.path);

    std::string command = "doxygen Doxyfile";
    LOG_VERBOSE("Executing Doxygen: {}\n", command);
    int ret = std::system(command.c_str());

    fs::current_path(original_path);

    if (ret == 0)
    {
        fmt::print(fmt::emphasis::bold | fg(fmt::color::green),
                   "Documentation successfully generated in 'docs' directory.\n");
    }
    else
    {
        throw CPPX_Exception("An error occurred while generating Doxygen documentation.");
    }
}

void handle_clean()
{
    const ProjectConfig proj = getCurrentProject();
    const fs::path build_dir = fs::path(proj.path) / "build";
    const fs::path docs_dir = fs::path(proj.path) / "docs";

    if (fs::exists(build_dir))
    {
        fs::remove_all(build_dir);
        fmt::print(fmt::emphasis::bold | fg(fmt::color::green), "Removed 'build' directory.\n");
    }
    else
    {
        fmt::print(fg(fmt::color::yellow), "'build' directory does not exist.\n");
    }

    if (fs::exists(docs_dir))
    {
        fs::remove_all(docs_dir);
        fmt::print(fmt::emphasis::bold | fg(fmt::color::green), "Removed 'docs' directory.\n");
    }
    else
    {
        fmt::print(fg(fmt::color::yellow), "'docs' directory does not exist.\n");
    }
}

void handle_test()
{
    ProjectConfig proj = getCurrentProject();
    fs::path test_dir = fs::path(proj.path) / "tests";
    fs::path build_dir = fs::path(proj.path) / "build";
    fs::create_directories(build_dir);

    if (!fs::exists(test_dir) || fs::is_empty(test_dir))
    {
        throw CPPX_Exception("The 'tests' directory does not exist or is empty. No tests to run.");
    }

    std::string compiler = pickCompiler();
    ProjectSettings ps = getProjectSettings();

    for (const auto &entry : fs::directory_iterator(test_dir))
    {
        if (entry.is_regular_file() && (entry.path().extension() == ".cpp" || entry.path().extension() == ".cc"))
        {
            const fs::path &test_file = entry.path();
            std::string test_name = test_file.stem().string();
            fs::path executable_path = build_dir / test_name;

            std::string command = compiler + " ";
            command += fmt::format("\"{}\" ", test_file.string());

            for (const auto &inc : ps.includepaths)
            {
                command += fmt::format("-I\"{}\" ", (fs::path(proj.path) / inc).string());
            }
            for (const auto &src : ps.srcfiles)
            {
                fs::path srcPath = fs::path(proj.path) / src;
                if (srcPath.filename() == "main.cpp")
                    continue;
                command += fmt::format("\"{}\" ", srcPath.string());
            }

            command += fmt::format("-o \"{}\" -g", executable_path.string());

            fmt::print(fmt::emphasis::bold | fg(fmt::color::cyan), "Compiling test: {}\n", test_name);
            LOG_VERBOSE("Compilation command: {}\n", command);
            if (std::system(command.c_str()) != 0)
            {
                fmt::print(stderr, fg(fmt::color::red), "Compilation of test {} failed.\n", test_name);
                continue; // Proceed to the next test
            }

            fmt::print(fmt::emphasis::bold | fg(fmt::color::green), "Running test: {}\n", test_name);
            if (std::system(executable_path.string().c_str()) != 0)
            {
                fmt::print(stderr, fg(fmt::color::red), "Test {} failed.\n", test_name);
            }
            else
            {
                fmt::print(fg(fmt::color::light_green), "Test {} completed successfully.\n", test_name);
            }
        }
    }
}

void handle_metadata()
{
    fmt::println("Settings metadata...");

    fmt::print("[project version] >> ");

    std::string version;

    std::cin >> version;
    std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');

    if (version.empty())
    {
        throw CPPX_Exception("Version cannot be empty.");
    }

    fmt::print("[author] >> ");
    std::string author;
    std::getline(std::cin, author);
    if (author.empty())
    {
        throw CPPX_Exception("Author cannot be empty.");
    }
    fmt::print("[ more than one author? (y/n) ] >> ");
    std::vector<std::string> authors;
    char Yn;
    std::cin >> Yn;
    std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
    if (Yn == 'y' || Yn == 'Y')
    {
        fmt::print("[authors] >> ");
        std::string authors_input;
        std::getline(std::cin, authors_input);
        std::istringstream iss(authors_input);
        std::string author_name;
        while (std::getline(iss, author_name, ','))
        {
            if (!author_name.empty())
                authors.push_back(author_name);
        }
    }
    else
    {
        authors.push_back(author);
    }

    fmt::print("[description] >> ");
    std::string description;
    std::getline(std::cin, description);
    if (description.empty())
    {
        throw CPPX_Exception("Description cannot be empty.");
    }
    fmt::print("[license] >> ");
    std::string license;
    std::getline(std::cin, license);
    if (license.empty())
    {
        throw CPPX_Exception("License cannot be empty.");
    }

    fmt::print("[github username] >> "); // github username/repo can be empty
    std::string github_username;
    std::getline(std::cin, github_username);
    fmt::print("[github repository] (Just the name, not URL) >> ");
    std::string github_repo;
    std::getline(std::cin, github_repo);
    if (github_username.empty() && !github_repo.empty())
    {
        throw CPPX_Exception("GitHub username cannot be empty if repository is provided.");
    }

    fmt::print("All done!\n");

    setMetadata("version", version);
    setMetadata("description", description);
    setMetadata("license", license);
    setMetadata("github_username", github_username);
    setMetadata("github_repo", github_repo);
    setMetadata("authors", authors);
}

void handle_info()
{
    ProjectConfig proj = getCurrentProject();
    ProjectSettings ps = getProjectSettings();

    std::vector<std::pair<std::string, std::string> > info_items = {
        {"Project Name", ps.name},
        {"Version", ps.version},
        {"Authors", fmt::format("{}", fmt::join(ps.authors, ", "))},
        {"Description", ps.description},
        {"License", ps.license},
        {"Project Path", proj.path},
        {"Build Dir", (fs::path(proj.path) / "build").string()},
        {"Docs Dir", (fs::path(proj.path) / "docs").string()},
        {"Tests Dir", (fs::path(proj.path) / "tests").string()},
        {"Src Dir", (fs::path(proj.path) / "src").string()},
        {"Include Dir", (fs::path(proj.path) / "include").string()},
        {"Vendor Dir", (fs::path(proj.path) / "vendor").string()}};

    size_t max_label_main = 0;
    for (const auto &key : info_items | std::views::keys)
        max_label_main = std::max(max_label_main, key.size());
    max_label_main += 2;

    constexpr size_t display_width = 70;
    const std::string horizontal_line(display_width - 2, '-');

    // Header
    fmt::print(fmt::emphasis::bold | fg(fmt::color::green), "\n┌{}┐\n", horizontal_line);
    fmt::print(fmt::emphasis::bold | fg(fmt::color::green), "│ {}{:<{}} │\n",
               fmt::styled("✨ Project Information", fmt::emphasis::bold), "",
               display_width - 4 - std::string("✨ Project Information").length());
    fmt::print(fmt::emphasis::bold | fg(fmt::color::green), "└{}┘\n", horizontal_line);
    fmt::print("\n");

    // Main info
    for (const auto &[fst, snd] : info_items)
    {
        fmt::print("  {} {:{}} : {}\n", fmt::styled("•", fg(fmt::color::blue_violet)),
                   fmt::styled(fst, fg(fmt::color::light_blue) | fmt::emphasis::bold), max_label_main,
                   fmt::styled(snd, fg(fmt::color::white)));
    }

    // Dependencies
    fmt::print("\n");
    fmt::print(fmt::emphasis::bold | fg(fmt::color::yellow), "- Dependencies {}-\n",
               std::string(display_width - std::string("- Dependencies ").length() - 1, '-'));
    fmt::println("");

    if (!ps.dependencies.empty())
    {
        size_t max_label_deps = 0;
        for (const auto &key : ps.dependencies | std::views::keys)
            max_label_deps = std::max(max_label_deps, key.size());
        max_label_deps += 2;

        for (const auto &[name, version] : ps.dependencies)
        {
            fmt::print("  {} {:{}} : {}\n", fmt::styled("→", fg(fmt::color::orange)),
                       fmt::styled(name, fg(fmt::color::lime_green)), max_label_deps,
                       fmt::styled(version, fg(fmt::color::white)));
        }
    }
    else
    {
        fmt::print(fg(fmt::color::gray), "  No dependencies found.\n");
    }

    // GitHub section
    fmt::print(fmt::emphasis::bold | fg(fmt::color::cyan), "- GitHub Repository {}-\n",
               std::string(display_width - std::string("- GitHub Repository ").length() - 1, '-'));

    if (!ps.github_username.empty() && !ps.github_repo.empty())
    {
        try
        {
            GithubInfo gi = getRepoInfo(ps.github_username, ps.github_repo);

            std::vector<std::pair<std::string, std::string> > gh_items = {
                {"Name", gi.name},
                {"Description", gi.description},
                {"Stars", fmt::format("{} {}", std::to_string(gi.stars), fmt::styled("⭐", fg(fmt::color::gold)))},
                {"Forks", std::to_string(gi.forks)},
                {"Open Issues", std::to_string(gi.open_issues)},
                {"Last Commit", gi.last_commit_date},
                {"URL", fmt::format("{} {}", gi.html_url, fmt::styled("🔗", fg(fmt::color::blue)))}};

            size_t max_label_gh = 0;
            for (const auto &key : gh_items | std::views::keys)
                max_label_gh = std::max(max_label_gh, key.size());
            max_label_gh += 2;

            for (const auto &[fst, snd] : gh_items)
            {
                fmt::print("  {} {:{}} : {}\n", fmt::styled("•", fg(fmt::color::light_cyan)),
                           fmt::styled(fst, fg(fmt::color::light_cyan) | fmt::emphasis::bold), max_label_gh,
                           fmt::styled(snd, fg(fmt::color::white)));
            }
        }
        catch (const std::exception &e)
        {
            fmt::print(fg(fmt::color::red) | fmt::emphasis::bold, "  Error fetching GitHub info: {}\n", e.what());
            fmt::print(fg(fmt::color::gray),
                       "  Please ensure your network connection is stable or check GitHub API rate limits.\n");
        }
    }
    else
    {
        fmt::print(fg(fmt::color::gray), "  No GitHub repository information available. Set 'github_username' and "
                   "'github_repo' in your project settings to display this section.\n");
    }

    // Footer
    fmt::print(fmt::emphasis::bold | fg(fmt::color::green), "\n┌{}┐\n", horizontal_line);
    fmt::print(fmt::emphasis::bold | fg(fmt::color::green), "│ {}{:<{}} │\n",
               fmt::styled("✔ Project information displayed!", fmt::emphasis::bold), "",
               display_width - 4 - std::string("✔ Project information displayed!").length());
    fmt::print(fmt::emphasis::bold | fg(fmt::color::green), "└{}┘\n", horizontal_line);
    fmt::print("\n");
}

void handle_fmt(const std::vector<std::string> &range)
{
    const ProjectSettings ps = getProjectSettings();
    const ProjectConfig pc = getCurrentProject();
    std::vector<fs::path> files;

    // Check if the current project path is a valid directory
    if (!fs::exists(pc.path) || !fs::is_directory(pc.path)) {
        fmt::print(fmt::emphasis::bold | fg(fmt::color::red), "Error: Invalid project directory '{}'\n", pc.path);
        return;
    }

    for (const auto &file : range)
    {
        fs::path p = fs::path(pc.path) / fs::path(file);
        // Check if the file is a glob pattern
        if (is_glob(file))
        {
            // Expand the glob pattern and add files to the list
            for (const auto &t : glob(pc.path, file))
            {
                // Check if the glob result is a valid file
                if (fs::exists(t) && fs::is_regular_file(t)) {
                    files.push_back(t);
                } else {
                    fmt::print(fg(fmt::color::yellow), "Warning: Glob pattern '{}' matched a non-existent or non-regular file: '{}'\n", file, t.string());
                }
            }
        }
        else // Treat as a direct file path
        {
            // Check if the provided path is a valid file
            if (fs::exists(p) && fs::is_regular_file(p)) {
                files.push_back(p);
            }
            // Check if the path is a directory
            else if (fs::exists(p) && fs::is_directory(p)) {
                fmt::print(fmt::emphasis::bold | fg(fmt::color::yellow), "Warning: Skipping directory '{}', only files can be formatted.\n", p.string());
            }
            // If the path doesn't exist at all
            else {
                fmt::print(fmt::emphasis::bold | fg(fmt::color::red), "Error: File not found or invalid path: '{}'\n", p.string());
            }
        }
    }

    if (files.empty()) {
        fmt::print(fg(fmt::color::yellow), "No valid files found to format. Exiting.\n");
        return;
    }

    for (const auto &file : files)
    {
        fmt::print(fmt::text_style(fmt::emphasis::bold) | fg(fmt::color::green),
                   "Formatting: {}\n", file.string());

        std::string command;
        if (ps.format.clangFormatFile)
        {
            if (ps.format.clangFormatFilepath == "!")
            {
                fmt::print(fmt::emphasis::bold | fg(fmt::color::red), "Invalid Format Configuration!\n");
                return;
            }

            fs::path configPath = ps.format.clangFormatFilepath;
            if (!fs::exists(configPath) || !fs::is_regular_file(configPath)) {
                fmt::print(fmt::emphasis::bold | fg(fmt::color::red), "Error: Clang-format configuration file not found or is invalid: '{}'\n", configPath.string());
                return;
            }

            command = "cd " + configPath.parent_path().string() +
                      " && clang-format -style=file -i \"" + file.string() + "\"";
        }
        else
        {
            std::string style = ps.format.formatBase.empty() ? "Microsoft" : ps.format.formatBase;
            command = "clang-format -style=" + style + " -i \"" + file.string() + "\"";
        }

//        fmt::print("executing: {}\n", command);
        if (const int result = std::system(command.c_str()); result != 0) {
            fmt::print(fmt::emphasis::bold | fg(fmt::color::red), "Error: Clang-format command failed for file '{}'\n", file.string());
        }
    }
}