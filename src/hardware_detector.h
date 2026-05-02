#ifndef HARDWARE_DETECTOR_H
#define HARDWARE_DETECTOR_H

#include <string>
#include <vector>
#include <map>

struct DeviceInfo {
    std::string name;
    std::string id;
    std::string conky_variable;
};

class HardwareDetector {
public:
    static std::vector<DeviceInfo> detect_gpus();
    static std::vector<DeviceInfo> detect_network_interfaces();
    static std::vector<DeviceInfo> detect_audio_cards();
    static std::vector<DeviceInfo> detect_hwmon_sensors();

private:
    static std::string read_sys_file(const std::string& path);
    static std::string exec(const char* cmd);
};

#endif // HARDWARE_DETECTOR_H