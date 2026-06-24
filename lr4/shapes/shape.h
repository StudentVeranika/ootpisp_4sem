#ifndef SHAPE_H
#define SHAPE_H

#include <QPoint>
#include <QColor>
#include <QList>
#include <QPainter>
#include <QRect>
#include <QVector>
#include <QString>
#include <optional>
#include <cmath>
#include <QJsonObject>
#include <QJsonArray>

class Shape
{
public:
    Shape() 
        : m_id(0)
        , m_name("Фигура")
        , m_isSelected(false)
        , m_isVisible(true)
        , m_fill(true)
        , m_fillColor(QColor(173, 216, 230))
        , m_anchorPoint(0, 0)
    {
    }
    
    virtual ~Shape() = default;
    
    int getId() const { return m_id; }
    void setId(int id) { m_id = id; }
    
    QString getName() const { return m_name; }
    void setName(const QString& name) { m_name = name; }
    
    bool isSelected() const { return m_isSelected; }
    void setSelected(bool selected) { m_isSelected = selected; }
    
    bool isVisible() const { return m_isVisible; }
    void setVisible(bool visible) { m_isVisible = visible; }
    
    bool hasFill() const { return m_fill; }
    void setFill(bool fill) { m_fill = fill; }
    
    QColor getFillColor() const { return m_fillColor; }
    void setFillColor(const QColor& color) { m_fillColor = color; }
    
    QPoint getAnchorPoint() const { return m_anchorPoint; }
    void setAnchorPoint(const QPoint& point) { m_anchorPoint = point; }
    
    virtual QString getType() const = 0;
    virtual void draw(QPainter& painter) = 0;
    virtual void drawVirtualFigure(QPainter& painter) = 0;
    virtual bool hitTest(const QPoint& point) = 0;
    virtual bool isPointInside(const QPoint& point) = 0;
    
    virtual void move(int dx, int dy) {
        if (points.isEmpty()) return;
        for (int i = 0; i < points.size(); ++i) {
            points[i] = QPoint(points[i].x() + dx, points[i].y() + dy);
        }
        m_anchorPoint = QPoint(m_anchorPoint.x() + dx, m_anchorPoint.y() + dy);
        updateRelativePoints();
    }
    
    virtual void moveAnchorPoint(const QPoint& newPoint) {
        if (!isPointInside(newPoint)) return;
        m_anchorPoint = newPoint;
        updateRelativePoints();
    }
    
    virtual void scale(double factor) = 0;
    virtual void resize(int handleIndex, int deltaX, int deltaY) = 0;
    virtual QRect getBoundingRect() const = 0;
    virtual int getHandleCount() const { return m_resizeHandles.size(); }
    virtual void updateRelativePoints() = 0;
    
    virtual int hitTestResizeHandle(const QPoint& point) {
        updateResizeHandles();
        for (int i = 0; i < m_resizeHandles.size(); ++i) {
            if (m_resizeHandles[i].contains(point)) return i;
        }
        return -1;
    }
    
    virtual bool hitTestAnchorHandle(const QPoint& point) {
        updateAnchorHandle();
        return m_anchorHandle.contains(point);
    }
    
    virtual void drawResizeHandles(QPainter& painter) {
        if (!m_isSelected) return;
        updateResizeHandles();
        painter.setBrush(QColor(150, 150, 255, 150));
        painter.setPen(Qt::white);
        for (const QRect& handle : m_resizeHandles) {
            painter.fillRect(handle, QColor(150, 150, 255, 150));
            painter.drawRect(handle);
        }
    }
    
    virtual void drawAnchorHandle(QPainter& painter) {
        if (!m_isSelected) return;
        updateAnchorHandle();
        painter.setBrush(QColor(255, 0, 0, 200));
        painter.setPen(QPen(Qt::white, 2));
        painter.drawEllipse(m_anchorHandle);
    }
    
    virtual void setSideColor(int index, const QColor& color) {
        if (index >= 0 && index < edgeColors.size()) {
            edgeColors[index] = color;
        }
    }
    
    virtual void setSideWidth(int index, int width) {
        if (index >= 0 && index < edgeWidths.size()) {
            edgeWidths[index] = width;
        }
    }
    
    virtual QColor getSideColor(int index) const {
        return (index >= 0 && index < edgeColors.size()) ? edgeColors[index] : Qt::black;
    }
    
    virtual int getSideWidth(int index) const {
        return (index >= 0 && index < edgeWidths.size()) ? edgeWidths[index] : 3;
    }
    
    virtual QJsonObject toJson() const {
        QJsonObject json;
        json["type"] = getType();
        json["id"] = m_id;
        json["name"] = m_name;
        json["anchor_x"] = m_anchorPoint.x();
        json["anchor_y"] = m_anchorPoint.y();
        json["fill"] = m_fill;
        json["fill_color"] = m_fillColor.name();
        
        QJsonArray pointsArray;
        for (const QPoint& p : points) {
            QJsonObject pointObj;
            pointObj["x"] = p.x();
            pointObj["y"] = p.y();
            pointsArray.append(pointObj);
        }
        json["points"] = pointsArray;
        
        QJsonArray colorsArray;
        for (const QColor& c : edgeColors) {
            colorsArray.append(c.name());
        }
        json["edge_colors"] = colorsArray;
        
        QJsonArray widthsArray;
        for (int w : edgeWidths) {
            widthsArray.append(w);
        }
        json["edge_widths"] = widthsArray;
        
        QJsonArray relPointsArray;
        for (const QPoint& p : relativePoints) {
            QJsonObject pointObj;
            pointObj["x"] = p.x();
            pointObj["y"] = p.y();
            relPointsArray.append(pointObj);
        }
        json["relative_points"] = relPointsArray;
        
        return json;
    }
    
    virtual bool fromJson(const QJsonObject& json) {
    m_id = json["id"].toInt();
    m_name = json["name"].toString();
    m_anchorPoint = QPoint(json["anchor_x"].toInt(), json["anchor_y"].toInt());
    m_fill = json["fill"].toBool();
    m_fillColor = QColor(json["fill_color"].toString());
    
    QJsonArray pointsArray = json["points"].toArray();
    points.clear();
    for (const QJsonValue& v : pointsArray) {
        QJsonObject pointObj = v.toObject();
        points.append(QPoint(pointObj["x"].toInt(), pointObj["y"].toInt()));
    }
    
    QJsonArray colorsArray = json["edge_colors"].toArray();
    edgeColors.clear();
    for (const QJsonValue& v : colorsArray) {
        edgeColors.append(QColor(v.toString()));
    }
    
    QJsonArray widthsArray = json["edge_widths"].toArray();
    edgeWidths.clear();
    for (const QJsonValue& v : widthsArray) {
        edgeWidths.append(v.toInt());
    }
    
    QJsonArray relPointsArray = json["relative_points"].toArray();
    relativePoints.clear();
    for (const QJsonValue& v : relPointsArray) {
        QJsonObject pointObj = v.toObject();
        relativePoints.append(QPoint(pointObj["x"].toInt(), pointObj["y"].toInt()));
    }
    
    return true;
}
    
    QVector<QPoint> points;
    QVector<QPoint> relativePoints;
    QList<QColor> edgeColors;
    QList<int> edgeWidths;
    
protected:
    int m_id;
    QString m_name;
    bool m_isSelected;
    bool m_isVisible;
    bool m_fill;
    QColor m_fillColor;
    QPoint m_anchorPoint;
    QVector<QRect> m_resizeHandles;
    QRect m_anchorHandle;
    
    virtual void updateResizeHandles() {
        m_resizeHandles.clear();
        int handleSize = 12;
        for (const QPoint& p : points) {
            m_resizeHandles.append(QRect(
                p.x() - handleSize / 2,
                p.y() - handleSize / 2,
                handleSize,
                handleSize
            ));
        }
    }
    
    virtual void updateAnchorHandle() {
        int handleSize = 16;
        m_anchorHandle = QRect(
            m_anchorPoint.x() - handleSize / 2,
            m_anchorPoint.y() - handleSize / 2,
            handleSize,
            handleSize
        );
    }
    
    QPointF calculateMiterJoin(const QPointF& p1, const QPointF& p2, const QPointF& p3,
                               float width1, float width2) {
        QPointF v12 = QPointF(p2.x() - p1.x(), p2.y() - p1.y());
        QPointF v23 = QPointF(p3.x() - p2.x(), p3.y() - p2.y());
        
        float len12 = std::sqrt(v12.x() * v12.x() + v12.y() * v12.y());
        float len23 = std::sqrt(v23.x() * v23.x() + v23.y() * v23.y());
        
        if (len12 < 0.001f || len23 < 0.001f) return p2;
        
        QPointF dir12 = QPointF(v12.x() / len12, v12.y() / len12);
        QPointF dir23 = QPointF(v23.x() / len23, v23.y() / len23);
        
        QPointF norm12 = QPointF(-dir12.y(), dir12.x());
        QPointF norm23 = QPointF(-dir23.y(), dir23.x());
        
        float cross = dir12.x() * dir23.y() - dir12.y() * dir23.x();
        float side = (cross > 0) ? 1.0f : -1.0f;
        
        if (std::abs(cross) < 0.001f) side = 1.0f;
        
        QPointF offset1Start = QPointF(
            p1.x() + norm12.x() * width1 * side,
            p1.y() + norm12.y() * width1 * side
        );
        
        QPointF offset1End = QPointF(
            p2.x() + norm12.x() * width1 * side,
            p2.y() + norm12.y() * width1 * side
        );
        
        QPointF offset2Start = QPointF(
            p2.x() + norm23.x() * width2 * side,
            p2.y() + norm23.y() * width2 * side
        );
        
        QPointF offset2End = QPointF(
            p3.x() + norm23.x() * width2 * side,
            p3.y() + norm23.y() * width2 * side
        );
        
        return lineIntersection(offset1Start, offset1End, offset2Start, offset2End);
    }
    
    QPointF lineIntersection(const QPointF& p1, const QPointF& p2,
                            const QPointF& p3, const QPointF& p4) {
        float x1 = p1.x(), y1 = p1.y();
        float x2 = p2.x(), y2 = p2.y();
        float x3 = p3.x(), y3 = p3.y();
        float x4 = p4.x(), y4 = p4.y();
        
        float denominator = (x1 - x2) * (y3 - y4) - (y1 - y2) * (x3 - x4);
        
        if (std::abs(denominator) < 0.001f) {
            return QPointF((x2 + x3) / 2, (y2 + y3) / 2);
        }
        
        float t = ((x1 - x3) * (y3 - y4) - (y1 - y3) * (x3 - x4)) / denominator;
        
        return QPointF(
            x1 + t * (x2 - x1),
            y1 + t * (y2 - y1)
        );
    }
};

#endif // SHAPE_H