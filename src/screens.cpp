#include "screens.h"
#include "config_manager.h"

#include <QCoreApplication>
#include <QGuiApplication>
#include <QRect>
#include <QScreen>

std::vector<MonitorRect> Screens::monitors() {
    std::vector<MonitorRect> out;

    auto* app = qobject_cast<QGuiApplication*>(QCoreApplication::instance());
    if (app == nullptr) {
        return out;
    }

    const QScreen* primary = app->primaryScreen();
    for (QScreen* screen : app->screens()) {
        if (screen == nullptr) continue;
        const QRect geometry = screen->geometry();

        MonitorRect monitor;
        monitor.name = screen->name().toStdString();
        monitor.x = geometry.x();
        monitor.y = geometry.y();
        monitor.w = geometry.width();
        monitor.h = geometry.height();
        monitor.primary = (screen == primary);
        out.push_back(monitor);
    }
    return out;
}

std::pair<int, int> Screens::output_origin(const std::string& name) {
    // X11 lays every window out in one global space: no origin, whatever
    // output was asked for.
    const auto& config = ConfigManager::instance();
    if (config.get_active_display_server_key() != "wayland") {
        return {0, 0};
    }

    const std::vector<MonitorRect> screens = monitors();
    if (screens.empty()) {
        return {0, 0};
    }

    const MonitorRect* target = monitor_by_name(screens, name);
    if (target == nullptr) target = primary_monitor(screens);
    if (target == nullptr) target = &screens.front();

    return {target->x, target->y};
}
