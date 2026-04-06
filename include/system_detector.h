#pragma once

#include <QString>
#include <QMap>

enum class LinuxDistro {
    Ubuntu,
    Fedora,
    Arch,
    Debian,
    Unknown
};

struct DistroInfo {
    LinuxDistro distro;
    QString name;
    QString packageManager;
    QString installCommand;
    QString dependencies;
};

class SystemDetector {
public:
    static DistroInfo detect();
};