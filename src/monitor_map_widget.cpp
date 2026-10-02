#include "monitor_map_widget.h"

#include <QFontMetrics>
#include <QMouseEvent>
#include <QPainter>
#include <QPaintEvent>

#include <algorithm>
#include <cmath>

const QSize MonitorMapWidget::kMarkerSize(116, 20);

MonitorMapWidget::MonitorMapWidget(QWidget* parent) : QWidget(parent) {
    setMouseTracking(true);
    setMinimumHeight(240);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
}

void MonitorMapWidget::setMonitors(const std::vector<MonitorRect>& monitors) {
    monitors_ = monitors;
    bounds_ = desktop_bounds(monitors_);
    recomputeTransform();
    update();
}

void MonitorMapWidget::setPanels(const std::vector<PanelMarker>& panels) {
    panels_ = panels;
    update();
}

void MonitorMapWidget::setActivePanel(const std::string& panel) {
    if (active_panel_ == panel) return;
    active_panel_ = panel;
    update();
}

QSize MonitorMapWidget::minimumSizeHint() const {
    return QSize(360, 240);
}

// ── Transform ────────────────────────────────────────────────────────────────

void MonitorMapWidget::recomputeTransform() {
    const int avail_w = std::max(1, width() - 2 * kMargin);
    const int avail_h = std::max(1, height() - 2 * kMargin);

    if (monitors_.empty() || bounds_.w <= 0 || bounds_.h <= 0) {
        scale_ = 1.0;
        origin_x_ = kMargin;
        origin_y_ = kMargin;
        return;
    }

    // Fit the whole layout, preserving aspect, so a monitor's shape on screen
    // matches its real shape.
    const double sx = static_cast<double>(avail_w) / bounds_.w;
    const double sy = static_cast<double>(avail_h) / bounds_.h;
    scale_ = std::min(sx, sy);

    // Centre on whichever axis has spare room. bounds_.x/y are folded in so
    // layouts starting left of or above the origin still fit.
    origin_x_ = kMargin + (avail_w - bounds_.w * scale_) / 2.0 - bounds_.x * scale_;
    origin_y_ = kMargin + (avail_h - bounds_.h * scale_) / 2.0 - bounds_.y * scale_;
}

QPoint MonitorMapWidget::toWidget(int desktop_x, int desktop_y) const {
    return QPoint(static_cast<int>(std::lround(origin_x_ + desktop_x * scale_)),
                  static_cast<int>(std::lround(origin_y_ + desktop_y * scale_)));
}

QPoint MonitorMapWidget::toDesktop(int widget_x, int widget_y) const {
    if (scale_ <= 0.0) return {0, 0};
    return QPoint(static_cast<int>(std::lround((widget_x - origin_x_) / scale_)),
                  static_cast<int>(std::lround((widget_y - origin_y_) / scale_)));
}

QSize MonitorMapWidget::monitorSize(const MonitorRect& monitor) const {
    return QSize(std::max(1, static_cast<int>(std::lround(monitor.w * scale_))),
                 std::max(1, static_cast<int>(std::lround(monitor.h * scale_))));
}

QRect MonitorMapWidget::markerRect(const PanelMarker& panel) const {
    const QPoint anchor = toWidget(panel.x, panel.y);
    QRect area(anchor, kMarkerSize);

    // Slide the box back inside the widget rather than let it run off an
    // edge: a panel parked at the far right of a wide layout would otherwise
    // be drawn half off screen and only half grabbable. Drawing and
    // hit-testing share this rect, so what is visible is exactly what
    // responds to a click - no ghost region to miss.
    const int max_left = std::max(0, width() - kMarkerSize.width());
    const int max_top = std::max(0, height() - kMarkerSize.height());
    area.moveLeft(std::clamp(anchor.x(), 0, max_left));
    area.moveTop(std::clamp(anchor.y(), 0, max_top));
    return area;
}

const PanelMarker* MonitorMapWidget::panelAt(const QPoint& pos) const {
    // The active panel wins an overlap: it is what the user is working on, so
    // it should be the easiest to grab.
    const PanelMarker* other = nullptr;
    for (const auto& panel : panels_) {
        if (panel.name == active_panel_) continue;
        if (markerRect(panel).contains(pos)) other = &panel;
    }
    for (const auto& panel : panels_) {
        if (panel.name != active_panel_) continue;
        if (markerRect(panel).contains(pos)) return &panel;
    }
    return other;
}

const MonitorRect* MonitorMapWidget::monitorAt(const QPoint& pos) const {
    const QPoint desktop = toDesktop(pos.x(), pos.y());
    return monitor_for_point(monitors_, desktop.x(), desktop.y());
}

// ── Mouse ────────────────────────────────────────────────────────────────────

void MonitorMapWidget::mousePressEvent(QMouseEvent* event) {
    if (event->button() != Qt::LeftButton) return;
    hover_pos_ = event->position().toPoint();

    if (const PanelMarker* hit = panelAt(hover_pos_)) {
        // Copy before emitting: the handler may replace panels_ outright and
        // leave `hit` dangling.
        const std::string name = hit->name;

        if (name != active_panel_) {
            active_panel_ = name;
            emit activePanelChanged(QString::fromStdString(name));
        }
        dragging_ = true;
        const auto it = std::find_if(panels_.begin(), panels_.end(),
                                     [&name](const PanelMarker& p) { return p.name == name; });
        if (it != panels_.end()) {
            drag_pos_ = markerRect(*it).topLeft();
            drag_delta_ = hover_pos_ - drag_pos_;
        } else {
            drag_pos_ = hover_pos_;
            drag_delta_ = {0, 0};
        }
        update();
        return;
    }

    if (const MonitorRect* monitor = monitorAt(hover_pos_)) {
        emit monitorClicked(QString::fromStdString(monitor->name));
    }
}

void MonitorMapWidget::mouseMoveEvent(QMouseEvent* event) {
    hover_pos_ = event->position().toPoint();
    if (dragging_) {
        drag_pos_ = hover_pos_ - drag_delta_;
    }
    update();
}

void MonitorMapWidget::mouseReleaseEvent(QMouseEvent* event) {
    if (event->button() != Qt::LeftButton || !dragging_) return;
    dragging_ = false;

    const QPoint desktop = toDesktop(drag_pos_.x(), drag_pos_.y());
    emit panelDropped(desktop.x(), desktop.y());
    update();
}

// ── Painting ─────────────────────────────────────────────────────────────────

void MonitorMapWidget::paintEvent(QPaintEvent*) {
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.fillRect(rect(), QColor("#1e1e1e"));

    if (monitors_.empty()) {
        painter.setPen(QColor("#888888"));
        painter.drawText(rect(), Qt::AlignCenter,
                         "No monitor layout available");
        return;
    }

    for (const auto& monitor : monitors_) {
        const QRect area(toWidget(monitor.x, monitor.y), monitorSize(monitor));

        painter.setPen(QPen(QColor(monitor.primary ? "#4a9eff" : "#5a5a5a"),
                            monitor.primary ? 2 : 1));
        painter.setBrush(QColor(monitor.primary ? "#242a33" : "#212121"));
        painter.drawRect(area);

        QFont font = painter.font();
        font.setBold(true);
        painter.setFont(font);
        painter.setPen(QColor("#d0d0d0"));
        painter.drawText(area.adjusted(8, 6, -8, -6),
                         Qt::AlignTop | Qt::AlignLeft,
                         QString::fromStdString(monitor.name) +
                             (monitor.primary ? "  (primary)" : ""));

        font.setBold(false);
        painter.setFont(font);
        painter.setPen(QColor("#8a8a8a"));
        painter.drawText(area.adjusted(8, 6, -8, -6),
                         Qt::AlignBottom | Qt::AlignLeft,
                         QString("%1x%2  @ %3,%4")
                             .arg(monitor.w).arg(monitor.h)
                             .arg(monitor.x).arg(monitor.y));
    }

    // While dragging, outline the monitor under the cursor so it is obvious
    // which output the drop will resolve to.
    if (dragging_) {
        if (const MonitorRect* target = monitorAt(hover_pos_)) {
            const QRect area(toWidget(target->x, target->y), monitorSize(*target));
            painter.setPen(QPen(QColor("#4a9eff"), 2, Qt::DashLine));
            painter.setBrush(Qt::NoBrush);
            painter.drawRect(area);
        }
    }

    const QFont font = painter.font();
    const QFontMetrics metrics(font);
    for (const auto& panel : panels_) {
        QRect area = markerRect(panel);
        if (dragging_ && panel.name == active_panel_) {
            area.moveTo(drag_pos_);
        }
        const bool active = (panel.name == active_panel_);

        painter.setPen(QPen(QColor(active ? "#ffffff" : "#777777"), 1));
        painter.setBrush(QColor(active ? "#2196F3" : "#3c3c3c"));
        painter.drawRoundedRect(area, 4, 4);

        painter.setPen(QColor(active ? "#ffffff" : "#cfcfcf"));
        painter.drawText(area, Qt::AlignCenter,
                         metrics.elidedText(QString::fromStdString(panel.name),
                                            Qt::ElideRight, area.width() - 8));
    }
}

void MonitorMapWidget::resizeEvent(QResizeEvent* event) {
    QWidget::resizeEvent(event);
    recomputeTransform();
}
