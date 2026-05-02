#include "hardware_detector.h"
#include <filesystem>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cstdio>
#include <memory>

namespace fs = std::filesystem;

std::string HardwareDetector::read_sys_file(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) return "";
    std::string val;
    if (std::getline(file, val)) {
        size_t last = val.find_last_not_of(" \n\r\t");
        if (last != std::string::npos) {
            val = val.substr(0, last + 1);
        }
    }
    return val;
}

std::string HardwareDetector::exec(const char* cmd) {
    char buffer[128];
    std::string result = "";
    std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(cmd, "r"), pclose);
    if (!pipe) return "";
    while (fgets(buffer, sizeof(buffer), pipe.get()) != nullptr) {
        result += buffer;
    }
    return result;
}

std::vector<DeviceInfo> HardwareDetector::detect_gpus() {
    std::vector<DeviceInfo> gpus;
    // Check NVIDIA via smi
    std::string nv = exec("nvidia-smi --query-gpu=name --format=csv,noheader");
    if (!nv.empty()) {
        std::stringstream ss(nv);
        std::string line;
        int idx = 0;
        while (std::getline(ss, line) && !line.empty()) {
            gpus.push_back({line, std::to_string(idx), "${nvidia temp}"});
            idx++;
        }
    }
    // Check DRM for AMD/Intel
    for (const auto& entry : fs::directory_iterator("/sys/class/drm")) {
        std::string name = entry.path().filename().string();
        if (name.find("card") == 0 && name.find("-") == std::string::npos) {
            gpus.push_back({"Generic GPU (" + name + ")", name, "${hwmon 0 temp 1}"});
        }
    }
    return gpus;
}

std::vector<DeviceInfo> HardwareDetector::detect_network_interfaces() {
    std::vector<DeviceInfo> ifaces;
    for (const auto& entry : fs::directory_iterator("/sys/class/net")) {
        std::string name = entry.path().filename().string();
        if (name == "lo") continue;
        ifaces.push_back({name, name, "${addr " + name + "}"});
    }
    return ifaces;
}

std::vector<DeviceInfo> HardwareDetector::detect_audio_cards() {
    std::vector<DeviceInfo> cards;
    std::ifstream file("/proc/asound/cards");
    std::string line;
    while (std::getline(file, line)) {
        if (line.empty() || line[0] == ' ') continue;
        std::stringstream ss(line);
        std::string idx, name;
        ss >> idx;
        // The rest of the line is usually " [Name ]: Desc"
        size_t start = line.find("[") + 1;
        size_t end = line.find("]");
        if (start != std::string::npos && end != std::string::npos) {
            name = line.substr(start, end - start);
            cards.push_back({name, idx, ""});
        }
    }
    return cards;
}

std::vector<DeviceInfo> HardwareDetector::detect_hwmon_sensors() {
    std::vector<DeviceInfo> sensors;
    for (const auto& entry : fs::directory_iterator("/sys/class/hwmon")) {
        std::string path = entry.path().string();
        std::string name = read_sys_file(path + "/name");
        std::string idx = entry.path().filename().string().substr(5);
        
        // Look for temp inputs
        for (const auto& s_entry : fs::directory_iterator(entry.path())) {
            std::string s_name = s_entry.path().filename().string();
            if (s_name.find("temp") == 0 && s_name.find("_input") != std::string::npos) {
                std::string base_name = s_name.substr(0, s_name.find("_"));
                std::string label_path = path + "/" + base_name + "_label";
                std::string label = fs::exists(label_path) ? read_sys_file(label_path) : "";
                if (label.empty()) label = name;
                
                std::string sensor_idx = s_name.substr(4, s_name.find("_") - 4);
                sensors.push_back({
                    label + " (" + name + ")",
                    idx + ":" + sensor_idx,
                    "${hwmon " + idx + " temp " + sensor_idx + "}"
                });
            }
        }
    }
    return sensors;
}