#pragma once

#include "placement.h"

#include <QPoint>
#include <QRect>
#include <QSize>
#include <QString>
#include <QWidget>

#include <string>
#include <vector>

class QMouseEvent;
class QPaintEvent;

// A panel drawn on the map: its name and where it sits in desktop
// coordinates. Pixel size is deliberately not tracked - UCCC has never known
// how big a panel is - so the marker is a fixed box and only the position
// carries meaning.
struct PanelMarker {
    std::string name;
    int x = 0;
    int y = 0;
};

// Canvas showing the monitor layout to scale, with panels drawn at their
// desktop coordinates and draggable onto an output.
//
// Pure presentation: it knows nothing about .conf files, app_config.json or
// the display server. It emits where things were dropped and which monitor
// was clicked, and the caller decides what that means.
class MonitorMapWidget : public QWidget {
    Q_OBJECT

public:
    explicit MonitorMapWidget(QWidget* parent = nullptr);

    void setMonitors(const std::vector<MonitorRect>& monitors);
    void setPanels(const std::vector<PanelMarker>& panels);
    void setActivePanel(const std::string& panel);
    const std::string& activePanel() const { return active_panel_; }

    QSize minimumSizeHint() const override;

signals:
    // The active panel was released at a desktop coordinate. Already
    // converted out of widget space, so the caller can hand it straight to
    // monitor_for_point().
    void panelDropped(int desktop_x, int desktop_y);

    // A monitor was clicked - choose it as the active panel's output.
    void monitorClicked(const QString& output);

    // The user started dragging a different panel on the map; the caller
    // should select it in its panel list too.
    void activePanelChanged(const QString& panel);

protected:
    void paintEvent(QPaintEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;

private:
    // Desktop -> widget, via the transform recomputed on resize and on
    // setMonitors(). Scales the whole layout to fit, preserving aspect, and
    // centres it.
    void recomputeTransform();
    QPoint toWidget(int desktop_x, int desktop_y) const;
    QPoint toDesktop(int widget_x, int widget_y) const;
    QSize monitorSize(const MonitorRect& monitor) const;

    // Marker box for a panel in widget coordinates; the same box used for
    // hit-testing while dragging.
    QRect markerRect(const PanelMarker& panel) const;
    const PanelMarker* panelAt(const QPoint& pos) const;
    const MonitorRect* monitorAt(const QPoint& pos) const;

    std::vector<MonitorRect> monitors_;
    std::vector<PanelMarker> panels_;
    std::string active_panel_;

    DesktopBox bounds_;
    double scale_ = 1.0;
    double origin_x_ = 0.0;
    double origin_y_ = 0.0;

    bool dragging_ = false;
    QPoint drag_pos_;    // marker top-left in widget coords while dragging
    QPoint drag_delta_;  // cursor offset inside the marker when grabbed
    QPoint hover_pos_;   // last known cursor position, for the drop outline

    static constexpr int kMargin = 28;
    static const QSize kMarkerSize;
};
