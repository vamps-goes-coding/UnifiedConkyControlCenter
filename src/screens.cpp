#include "screens.h"
#include "config_manager.h"

#include <QCoreApplication>
#include <QGuiApplication>
#include <QScreen>
#include <cstdlib>

std::pair<int, int> Screens::active_output_origin() {
    const auto& config = ConfigManager::instance();
    if (config.get_active_display_server_key() != "wayland") {
        return {0, 0};
    }
    auto* app = qobject_cast<QGuiApplication*>(QCoreApplication::instance());
    if (app == nullptr) {
        return {0, 0};
    }
    const QList<QScreen*> screens = app->screens();
    if (screens.isEmpty()) {
        return {0, 0};
    }
    const char* wanted_env = std::getenv("CONKY_WAYLAND_OUTPUT");
    const std::string wanted = wanted_env ? wanted_env : "";
    const QScreen* target = nullptr;
    if (!wanted.empty()) {
        for (QScreen* s : screens) {
            if (s != nullptr && s->name().toStdString() == wanted) {
                target = s;
                break;
            }
        }
    }
    if (target == nullptr) {
        target = app->primaryScreen();
    }
    if (target == nullptr) {
        target = screens.front();
    }
    const QPoint origin = target->geometry().topLeft();
    return {origin.x(), origin.y()};
}
