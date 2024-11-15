// Copyright (c) 2019-present Anonymous275.
// BeamMP Launcher code is not in the public domain and is not free software.
// One must be granted explicit permission by the copyright holder in order to modify or distribute any part of the source or binaries.
// Anything else is prohibited. Modified works may not be published and have be upstreamed to the official repository.
///
/// Created by Anonymous275 on 7/18/2020
///

#include <filesystem>
#if defined(_WIN32)
#include <windows.h>
#elif defined(__linux__) || defined(__APPLE__)
#include "vdf_parser.hpp"
#include <pwd.h>
#include <unistd.h>
#include <vector>
#endif
#if defined(__APPLE__)
#include "Utils.h"
#include "Options.h"
#include <cstdio>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <array>
#endif
#include <algorithm>
#include "Logger.h"
#include <fstream>
#include <string>
#include <thread>

#define MAX_KEY_LENGTH 255
#define MAX_VALUE_NAME 16383

int TraceBack = 0;
std::string GameDir;
std::string BottlePath;

void lowExit(int code) {
    TraceBack = 0;
    std::string msg = "Failed to find the game please launch it. Report this if the issue persists code ";
    error(msg + std::to_string(code));
    std::this_thread::sleep_for(std::chrono::seconds(10));
    exit(2);
}

#if defined(__APPLE__)
std::string GetBottlePath() {
    return BottlePath;
}

std::string GetBottleName() {
    std::filesystem::path bottlePath(BottlePath);
    return bottlePath.filename().string();
}
#endif

std::string GetGameDir() {
#if defined(_WIN32)
    return GameDir.substr(0, GameDir.find_last_of('\\'));
#elif defined(__linux__)
    return GameDir.substr(0, GameDir.find_last_of('/'));
#elif defined(__APPLE__)
    return GameDir;
#endif
}
#ifdef _WIN32
LONG OpenKey(HKEY root, const char* path, PHKEY hKey) {
    return RegOpenKeyEx(root, reinterpret_cast<LPCSTR>(path), 0, KEY_READ, hKey);
}
std::string QueryKey(HKEY hKey, int ID) {
    TCHAR achKey[MAX_KEY_LENGTH]; // buffer for subkey name
    DWORD cbName; // size of name string
    TCHAR achClass[MAX_PATH] = TEXT(""); // buffer for class name
    DWORD cchClassName = MAX_PATH; // size of class string
    DWORD cSubKeys = 0; // number of subkeys
    DWORD cbMaxSubKey; // longest subkey size
    DWORD cchMaxClass; // longest class string
    DWORD cValues; // number of values for key
    DWORD cchMaxValue; // longest value name
    DWORD cbMaxValueData; // longest value data
    DWORD cbSecurityDescriptor; // size of security descriptor
    FILETIME ftLastWriteTime; // last write time

    DWORD i, retCode;

    TCHAR achValue[MAX_VALUE_NAME];
    DWORD cchValue = MAX_VALUE_NAME;

    retCode = RegQueryInfoKey(
        hKey, // key handle
        achClass, // buffer for class name
        &cchClassName, // size of class string
        nullptr, // reserved
        &cSubKeys, // number of subkeys
        &cbMaxSubKey, // longest subkey size
        &cchMaxClass, // longest class string
        &cValues, // number of values for this key
        &cchMaxValue, // longest value name
        &cbMaxValueData, // longest value data
        &cbSecurityDescriptor, // security descriptor
        &ftLastWriteTime); // last write time

    BYTE* buffer = new BYTE[cbMaxValueData];
    ZeroMemory(buffer, cbMaxValueData);
    if (cSubKeys) {
        for (i = 0; i < cSubKeys; i++) {
            cbName = MAX_KEY_LENGTH;
            retCode = RegEnumKeyEx(hKey, i, achKey, &cbName, nullptr, nullptr, nullptr, &ftLastWriteTime);
            if (retCode == ERROR_SUCCESS) {
                if (strcmp(achKey, "Steam App 284160") == 0) {
                    return achKey;
                }
            }
        }
    }
    if (cValues) {
        for (i = 0, retCode = ERROR_SUCCESS; i < cValues; i++) {
            cchValue = MAX_VALUE_NAME;
            achValue[0] = '\0';
            retCode = RegEnumValue(hKey, i, achValue, &cchValue, nullptr, nullptr, nullptr, nullptr);
            if (retCode == ERROR_SUCCESS) {
                DWORD lpData = cbMaxValueData;
                buffer[0] = '\0';
                LONG dwRes = RegQueryValueEx(hKey, achValue, nullptr, nullptr, buffer, &lpData);
                std::string data = (char*)(buffer);
                std::string key = achValue;

                switch (ID) {
                case 1:
                    if (key == "SteamExe") {
                        auto p = data.find_last_of("/\\");
                        if (p != std::string::npos) {
                            return data.substr(0, p);
                        }
                    }
                    break;
                case 2:
                    if (key == "Name" && data == "BeamNG.drive")
                        return data;
                    break;
                case 3:
                    if (key == "rootpath")
                        return data;
                    break;
                case 4:
                    if (key == "userpath_override")
                        return data;
                case 5:
                    if (key == "Local AppData")
                        return data;
                default:
                    break;
                }
            }
        }
    }
    delete[] buffer;
    return "";
}
#endif

namespace fs = std::filesystem;

bool NameValid(const std::string& N) {
    if (N == "config" || N == "librarycache") {
        return true;
    }
    if (N.find_first_not_of("0123456789") == std::string::npos) {
        return true;
    }
    return false;
}
void FileList(std::vector<std::string>& a, const std::string& Path) {
    for (const auto& entry : fs::directory_iterator(Path)) {
        const auto& DPath = entry.path();
        if (!entry.is_directory()) {
            a.emplace_back(DPath.string());
        } else if (NameValid(DPath.filename().string())) {
            FileList(a, DPath.string());
        }
    }
}

#if defined(__APPLE__)
std::map<std::string, std::string> GetDriveMappings(const std::string& bottlePath) {
    std::map<std::string, std::string> driveMappings;
    std::string dosDevicesPath = bottlePath + "/dosdevices/";

    if (std::filesystem::exists(dosDevicesPath)) {
        for (const auto& entry : std::filesystem::directory_iterator(dosDevicesPath)) {
            if (entry.is_symlink()) {
                std::string driveName = Utils::ToLower(entry.path().filename().string());
                driveName.erase(std::remove(driveName.begin(), driveName.end(), ':'), driveName.end());
                std::string macPath = std::filesystem::read_symlink(entry.path()).string();
                if (!std::filesystem::path(macPath).is_absolute()) {
                    macPath = dosDevicesPath + macPath;
                }
                driveMappings[driveName] = macPath;
            }
        }
    } else {
        error("Failed to find dosdevices directory for bottle '" + bottlePath + "'");
    }
    return driveMappings;
}
#endif

void LegitimacyCheck() {
#if defined(_WIN32)
    std::string Result;
    std::string K3 = R"(Software\BeamNG\BeamNG.drive)";
    HKEY hKey;
    LONG dwRegOPenKey = OpenKey(HKEY_CURRENT_USER, K3.c_str(), &hKey);
    if (dwRegOPenKey == ERROR_SUCCESS) {
        Result = QueryKey(hKey, 3);
        if (Result.empty()) {
            debug("Failed to QUERY key HKEY_CURRENT_USER\\Software\\BeamNG\\BeamNG.drive");
            lowExit(3);
        }
        GameDir = Result;
    } else {
        debug("Failed to OPEN key HKEY_CURRENT_USER\\Software\\BeamNG\\BeamNG.drive");
        lowExit(4);
    }
    K3.clear();
    Result.clear();
    RegCloseKey(hKey);
#elif defined(__linux__)
    struct passwd* pw = getpwuid(getuid());
    std::string homeDir = pw->pw_dir;
    // Right now only steam is supported
    std::ifstream libraryFolders(homeDir + "/.steam/root/steamapps/libraryfolders.vdf");
    auto root = tyti::vdf::read(libraryFolders);

    for (auto folderInfo : root.childs) {
        if (std::filesystem::exists(folderInfo.second->attribs["path"] + "/steamapps/common/BeamNG.drive/")) {
            GameDir = folderInfo.second->attribs["path"] + "/steamapps/common/BeamNG.drive/";
            break;
        }
    }

#elif defined(__APPLE__)
    struct passwd* pw = getpwuid(getuid());
    std::string homeDir = pw->pw_dir;
    std::string bottlePath;

    std::pair<std::string, int> bottlesCmd = Utils::runCommand("defaults read com.codeweavers.CrossOver.plist BottleDir");
    std::string crossoverBottlesPath = bottlesCmd.first;
    int statusCode = bottlesCmd.second;

    if (statusCode != 0) {
        std::filesystem::path bottlesFolder(homeDir + "/Library/Application Support/CrossOver/Bottles");
        if (std::filesystem::exists(bottlesFolder)) {
            info("Using the default bottles path");
            crossoverBottlesPath = homeDir + "/Library/Application Support/CrossOver/Bottles";
        } else {
            error("Failed to detect Crossover, please make sure you have it installed.");
            exit(1);
        }
    } else {
        crossoverBottlesPath.pop_back(); // Remove newline character from the path
        crossoverBottlesPath += "/";
    }

    info("Crossover bottles path: " + crossoverBottlesPath);

    if (!empty(options.bottle)) {
        std::filesystem::path bottleFolder(crossoverBottlesPath + options.bottle);
        if (std::filesystem::exists(bottleFolder)) {
            info("Checking bottle: " + bottleFolder.filename().string());
            auto driveMappings = GetDriveMappings(bottleFolder.string());

            std::string libraryFilePath = bottleFolder.string() + "/drive_c/Program Files (x86)/Steam/config/libraryfolders.vdf";
            std::ifstream libraryFile(libraryFilePath);
            if (!libraryFile.is_open()) {
                error("Failed to open libraryfolders.vdf in bottle '" + bottleFolder.filename().string() + "'");
                exit(1);
            }

            auto root = tyti::vdf::read(libraryFile);
            libraryFile.close();

            for (const auto& [key, folderInfo] : root.childs) {
                auto pathIter = folderInfo->attribs.find("path");
                if (pathIter == folderInfo->attribs.end())
                    continue;

                std::string path = pathIter->second;
                info("Found Steam library path: " + path);

                std::string driveLetter = Utils::ToLower(path.substr(0, path.find(":")));
                driveLetter.erase(std::remove(driveLetter.begin(), driveLetter.end(), ':'), driveLetter.end());

                if (driveMappings.find(driveLetter) == driveMappings.end()) {
                    warn("Drive letter " + driveLetter + " not found in mappings.");
                    continue;
                }

                std::string basePath = driveMappings[driveLetter];
                if (!basePath.empty() && basePath.back() == '/') {
                    basePath.pop_back();
                }

                // Normalize the path using std::filesystem
                std::filesystem::path additionalPath = std::filesystem::path(path.substr(2)).make_preferred();
                if (!additionalPath.empty() && additionalPath.is_absolute()) {
                    additionalPath = additionalPath.relative_path();
                }

                std::filesystem::path beamngPath = std::filesystem::path(basePath) / additionalPath / "steamapps/common/BeamNG.drive";
                info("Checking for BeamNG.drive in: " + beamngPath.string());

                if (std::filesystem::exists(beamngPath)) {
                    info("BeamNG.drive found in bottle '" + bottleFolder.filename().string() + "' at: " + beamngPath.string());
                    GameDir = beamngPath.string();
                    BottlePath = bottleFolder.string();
                    info("GameDir: " + GameDir);
                    info("BottlePath: " + BottlePath);
                    return;
                }
            }
        } else {
            error("Bottle '" + options.bottle + "' doesn't exist");
            exit(1);
        }
    }
    for (const auto& bottle : std::filesystem::directory_iterator(crossoverBottlesPath)) {
        if (!bottle.is_directory())
            continue;

        info("Checking bottle: " + bottle.path().filename().string());
        auto driveMappings = GetDriveMappings(bottle.path().string());

        std::string libraryFilePath = bottle.path().string() + "/drive_c/Program Files (x86)/Steam/config/libraryfolders.vdf";
        std::ifstream libraryFile(libraryFilePath);
        if (!libraryFile.is_open()) {
            error("Failed to open libraryfolders.vdf in bottle '" + bottle.path().filename().string() + "'");
            continue;
        }

        auto root = tyti::vdf::read(libraryFile);
        libraryFile.close();

        for (const auto& [key, folderInfo] : root.childs) {
            auto pathIter = folderInfo->attribs.find("path");
            if (pathIter == folderInfo->attribs.end())
                continue;

            std::string path = pathIter->second;
            info("Found Steam library path: " + path);

            std::string driveLetter = Utils::ToLower(path.substr(0, path.find(":")));
            driveLetter.erase(std::remove(driveLetter.begin(), driveLetter.end(), ':'), driveLetter.end());

            if (!driveMappings.contains(driveLetter)) {
                warn("Drive letter " + driveLetter + " not found in mappings.");
                continue;
            }

            std::string basePath = driveMappings[driveLetter];
            if (!basePath.empty() && basePath.back() == '/') {
                basePath.pop_back();
            }

            std::string additionalPath = path.substr(2);
            std::replace(additionalPath.begin(), additionalPath.end(), '\\', '/');
            if (!additionalPath.empty() && additionalPath.front() == '/') {
                additionalPath.erase(0, 1);
            }

            std::filesystem::path beamngPath = std::filesystem::path(basePath) / additionalPath / "steamapps/common/BeamNG.drive";
            info("Checking for BeamNG.drive in: " + beamngPath.string());

            if (std::filesystem::exists(beamngPath)) {
                info("BeamNG.drive found in bottle '" + bottle.path().filename().string() + "' at: " + beamngPath.string());
                GameDir = beamngPath.string();
                BottlePath = bottle.path().string();
                info("GameDir: " + GameDir);
                info("BottlePath: " + BottlePath);
                return;
            }
        }
    }

    error("Failed to find BeamNG.drive installation in any CrossOver bottle. Make sure BeamNG.drive is installed in a CrossOver bottle, or set it with the --bottle argument.");
    exit(1);

#endif
}
std::string CheckVer(const std::string& dir) {
#if defined(_WIN32)
    std::string temp, Path = dir + "\\integrity.json";
#elif defined(__linux__) || defined(__APPLE__)
    std::string temp, Path = dir + "/integrity.json";
#endif
    std::ifstream f(Path.c_str(), std::ios::binary);
    int Size = int(std::filesystem::file_size(Path));
    std::string vec(Size, 0);
    f.read(&vec[0], Size);
    f.close();

    vec = vec.substr(vec.find_last_of("version"), vec.find_last_of('"'));
    for (const char& a : vec) {
        if (isdigit(a) || a == '.')
            temp += a;
    }
    return temp;
}
