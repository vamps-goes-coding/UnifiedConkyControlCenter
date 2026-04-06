#include "system_detector.h"
#include <QFile>
#include <QTextStream>
#include <QProcess>

DistroInfo SystemDetector::detect() {
    DistroInfo info { LinuxDistro::Unknown, "Unknown Linux", "", "", "" };
    
    QFile file("/etc/os-release");
    if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QTextStream in(&file);
        QMap<QString, QString> osData;
        while (!in.atEnd()) {
            QString line = in.readLine();
            QStringList parts = line.split("=");
            if (parts.size() == 2) {
                osData[parts[0]] = parts[1].remove("\"");
            }
        }

        QString id = osData["ID"].toLower();
        QString idLike = osData["ID_LIKE"].toLower();

        if (id == "ubuntu" || idLike.contains("ubuntu")) {
            info = { LinuxDistro::Ubuntu, "Ubuntu/Debian", "apt", "sudo apt install -y", 
                     "build-essential cmake qt6-base-dev libqt6widgets6 nlohmann-json3-dev conky" };
        } else if (id == "fedora" || idLike.contains("fedora")) {
            info = { LinuxDistro::Fedora, "Fedora", "dnf", "sudo dnf install -y", 
                     "gcc-c++ cmake qt6-qtbase-devel nlohmann-json-devel conky" };
        } else if (id == "arch" || idLike.contains("arch")) {
            info = { LinuxDistro::Arch, "Arch Linux", "pacman", "sudo pacman -S --noconfirm", 
                     "base-devel cmake qt6-base nlohmann-json conky" };
        } else if (id == "debian") {
            info = { LinuxDistro::Debian, "Debian", "apt", "sudo apt install -y", 
                     "build-essential cmake qt6-base-dev nlohmann-json3-dev conky" };
        }
    }
    return info;
}