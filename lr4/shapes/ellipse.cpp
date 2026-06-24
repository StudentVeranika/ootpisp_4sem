#include "ellipse.h"
#include <QPainterPath>
#include <cmath>

Ellipse::Ellipse()
    : Shape()
    , m_center(0, 0)
    , m_radiusX(60)
    , m_radiusY(60)
    , m_angle(0)
    , m_cachedFocalDistance(0)
{
    m_name = "Эллипс";
    edgeColors.append(Qt::black);
    edgeWidths.append(3);
    m_anchorPoint = QPoint(0, 0);
    m_center = QPoint(0, 0);
    m_cachedFocalDistance = 2 * sqrt(60*60 - 60*60);
    updateRelativePoints();
    updateBoundingRect();
}

Ellipse::Ellipse(const QString& name)
    : Shape()
    , m_center(0, 0)
    , m_radiusX(60)
    , m_radiusY(60)
    , m_angle(0)
    , m_cachedFocalDistance(0)
{
    m_name = name;
    edgeColors.append(Qt::black);
    edgeWidths.append(3);
    m_anchorPoint = QPoint(0, 0);
    m_center = QPoint(0, 0);
    m_cachedFocalDistance = 2 * sqrt(60*60 - 60*60);
    updateRelativePoints();
    updateBoundingRect();
}

Ellipse::~Ellipse()
{
}

Ellipse* Ellipse::createCircle()
{
    Ellipse* ellipse = new Ellipse("Окружность");
    ellipse->setAsCircle(75);
    ellipse->setFill(true);
    ellipse->setFillColor(QColor(173, 216, 230));
    return ellipse;
}

void Ellipse::setCenter(const QPoint& center)
{
    m_center = center;
    updateRelativePoints();
    updateBoundingRect();
}

void Ellipse::setRadiusX(int rx)
{
    m_radiusX = std::max(10, rx);
    int a = std::max(m_radiusX, m_radiusY);
    int b = std::min(m_radiusX, m_radiusY);
    m_cachedFocalDistance = 2 * sqrt(a * a - b * b);
    updateRelativePoints();
    updateBoundingRect();
}

void Ellipse::setRadiusY(int ry)
{
    m_radiusY = std::max(10, ry);
    int a = std::max(m_radiusX, m_radiusY);
    int b = std::min(m_radiusX, m_radiusY);
    m_cachedFocalDistance = 2 * sqrt(a * a - b * b);
    updateRelativePoints();
    updateBoundingRect();
}

void Ellipse::setAngle(int angle)
{
    m_angle = angle % 360;
    updateRelativePoints();
    updateBoundingRect();
}

void Ellipse::setAsCircle(int radius)
{
    m_radiusX = std::max(10, radius);
    m_radiusY = m_radiusX;
    updateRelativePoints();
    updateBoundingRect();
}

QPoint Ellipse::rotatePoint(const QPoint& point, double angle) const
{
    double rad = angle * M_PI / 180.0;
    double cosA = cos(rad);
    double sinA = sin(rad);
    
    return QPoint(
        static_cast<int>(point.x() * cosA - point.y() * sinA),
        static_cast<int>(point.x() * sinA + point.y() * cosA)
    );
}

QPoint Ellipse::rotateBackPoint(const QPoint& point, double angle) const
{
    double rad = -angle * M_PI / 180.0;
    double cosA = cos(rad);
    double sinA = sin(rad);
    
    return QPoint(
        static_cast<int>(point.x() * cosA - point.y() * sinA),
        static_cast<int>(point.x() * sinA + point.y() * cosA)
    );
}
QPoint Ellipse::getFocus1() const
{
    double c = m_cachedFocalDistance / 2;
    
    double rad = m_angle * M_PI / 180.0;
    double cosA = cos(rad);
    double sinA = sin(rad);
    
    QPoint focus(c, 0);
    QPoint rotatedFocus(
        static_cast<int>(focus.x() * cosA - focus.y() * sinA),
        static_cast<int>(focus.x() * sinA + focus.y() * cosA)
    );
    
    return m_center + rotatedFocus;
}

QPoint Ellipse::getFocus2() const
{
    double c = m_cachedFocalDistance / 2;
    
    double rad = m_angle * M_PI / 180.0;
    double cosA = cos(rad);
    double sinA = sin(rad);
    
    QPoint focus(-c, 0);
    QPoint rotatedFocus(
        static_cast<int>(focus.x() * cosA - focus.y() * sinA),
        static_cast<int>(focus.x() * sinA + focus.y() * cosA)
    );
    
    return m_center + rotatedFocus;
}
double Ellipse::getFocalDistance() const
{
    return m_cachedFocalDistance;
}

void Ellipse::setFocalDistance(double distance)
{
    if (distance < 0) distance = 0;
    m_cachedFocalDistance = distance;
    
    int a = std::max(m_radiusX, m_radiusY);
    int b = std::min(m_radiusX, m_radiusY);
    
    double c = distance / 2.0;
    double newA = sqrt(c * c + b * b);
    
    if (newA < 10) newA = 10;
    
    if (m_radiusX >= m_radiusY) {
        m_radiusX = static_cast<int>(newA);
    } else {
        m_radiusY = static_cast<int>(newA);
    }
    
    updateRelativePoints();
    updateBoundingRect();
    updateResizeHandles();
}

int Ellipse::getFocusHandleIndex(const QPoint& point, int tolerance) const
{
    QPoint f1 = getFocus1();
    QPoint f2 = getFocus2();
    
    int dx1 = point.x() - f1.x();
    int dy1 = point.y() - f1.y();
    int dist1 = static_cast<int>(sqrt(dx1*dx1 + dy1*dy1));
    
    int dx2 = point.x() - f2.x();
    int dy2 = point.y() - f2.y();
    int dist2 = static_cast<int>(sqrt(dx2*dx2 + dy2*dy2));
    
    if (dist1 <= tolerance) return 0;
    if (dist2 <= tolerance) return 1;
    return -1;
}

void Ellipse::resizeByFocus(int focusIndex, const QPoint& newPos)
{
    QPoint f1 = getFocus1();
    QPoint f2 = getFocus2();
    
    if (focusIndex == 0) {
        f1 = newPos;
    } else {
        f2 = newPos;
    }
    
    QPoint newCenter = (f1 + f2) / 2;
    
    double dx = newCenter.x() - m_center.x();
    double dy = newCenter.y() - m_center.y();
    
    QPoint adjustedF1(f1.x() - dx, f1.y() - dy);
    QPoint adjustedF2(f2.x() - dx, f2.y() - dy);
    
    double newFocalDistance = sqrt(pow(adjustedF1.x() - adjustedF2.x(), 2) + 
                                    pow(adjustedF1.y() - adjustedF2.y(), 2));
    
    m_cachedFocalDistance = newFocalDistance;
    
    double newC = newFocalDistance / 2;
    
    int a = std::max(m_radiusX, m_radiusY);
    int b = std::min(m_radiusX, m_radiusY);
    double newA = sqrt(newC * newC + b * b);
    
    if (newA < 10) newA = 10;
    
    if (m_radiusX >= m_radiusY) {
        m_radiusX = static_cast<int>(newA);
    } else {
        m_radiusY = static_cast<int>(newA);
    }
    
    double angle = atan2(adjustedF2.y() - adjustedF1.y(), adjustedF2.x() - adjustedF1.x()) * 180 / M_PI;
    m_angle = static_cast<int>(angle);
    
    updateRelativePoints();
    updateBoundingRect();
    updateResizeHandles();
}
void Ellipse::draw(QPainter& painter)
{
    painter.setRenderHint(QPainter::Antialiasing);
    
    int penWidth = edgeWidths.isEmpty() ? 3 : edgeWidths[0];
    int halfPen = penWidth / 2;
    
    painter.save();
    painter.translate(m_center);
    painter.rotate(m_angle);
    
    if (m_fill) {
        painter.setBrush(m_fillColor);
        painter.setPen(Qt::NoPen);
        painter.drawEllipse(QPoint(0, 0), m_radiusX - halfPen, m_radiusY - halfPen);
    }
    
    if (!edgeColors.isEmpty()) {
        painter.setBrush(Qt::NoBrush);
        QPen pen(edgeColors[0], penWidth);
        pen.setJoinStyle(Qt::MiterJoin);
        painter.setPen(pen);
        painter.drawEllipse(QPoint(0, 0), m_radiusX - halfPen, m_radiusY - halfPen);
    }
    
    painter.restore();
    
    if (m_isSelected) {
        QPoint f1 = getFocus1();
        QPoint f2 = getFocus2();
        
        painter.setBrush(QColor(255, 0, 0, 200));
        painter.setPen(QPen(Qt::white, 2));
        painter.drawEllipse(f1, 6, 6);
        painter.drawEllipse(f2, 6, 6);
        
        painter.setPen(QPen(QColor(255, 0, 0, 100), 1, Qt::DashLine));
        painter.drawLine(f1, f2);
    }
}

void Ellipse::drawVirtualFigure(QPainter& painter)
{
    if (m_boundingRect.isValid()) {
        QPen dashPen(m_isSelected ? Qt::blue : Qt::gray, 1);
        dashPen.setStyle(Qt::DashLine);
        painter.setPen(dashPen);
        painter.setBrush(Qt::NoBrush);
        painter.drawRect(m_boundingRect);
    }
}

bool Ellipse::hitTest(const QPoint& point)
{
    int penWidth = edgeWidths.isEmpty() ? 3 : edgeWidths[0];
    int halfPen = penWidth / 2;
    
    QPoint localPoint = point - m_center;
    QPoint rotatedPoint = rotateBackPoint(localPoint, m_angle);
    
    int testRadiusX = m_radiusX - halfPen;
    int testRadiusY = m_radiusY - halfPen;
    
    if (testRadiusX <= 0 || testRadiusY <= 0) return false;
    
    double value = (rotatedPoint.x() * rotatedPoint.x()) / (testRadiusX * testRadiusX) + 
                   (rotatedPoint.y() * rotatedPoint.y()) / (testRadiusY * testRadiusY);
    return value <= 1.0;
}

bool Ellipse::isPointInside(const QPoint& point)
{
    return hitTest(point);
}

void Ellipse::move(int dx, int dy)
{
    m_anchorPoint = QPoint(m_anchorPoint.x() + dx, m_anchorPoint.y() + dy);
    m_center = QPoint(m_center.x() + dx, m_center.y() + dy);
    m_boundingRect.translate(dx, dy);
    updateRelativePoints();
}

void Ellipse::moveAnchorPoint(const QPoint& newPoint)
{
    if (!isPointInside(newPoint)) return;
    
    m_anchorPoint = newPoint;
    
    updateRelativePoints();
}

void Ellipse::scale(double factor)
{
    m_radiusX = static_cast<int>(m_radiusX * factor);
    m_radiusY = static_cast<int>(m_radiusY * factor);
    
    m_radiusX = std::max(10, m_radiusX);
    m_radiusY = std::max(10, m_radiusY);
    
    updateRelativePoints();
    updateBoundingRect();
}

void Ellipse::resize(int handleIndex, int deltaX, int deltaY)
{
    Q_UNUSED(handleIndex);
    
    int delta = std::abs(deltaX) > std::abs(deltaY) ? deltaX : deltaY;
    
    if (delta != 0) {
        if (m_radiusX == m_radiusY) {
            m_radiusX = std::max(10, m_radiusX + delta);
            m_radiusY = m_radiusX;
        } else {
            m_radiusX = std::max(10, m_radiusX + delta);
            m_radiusY = std::max(10, m_radiusY + delta);
        }
    }
    
    updateRelativePoints();
    updateBoundingRect();
    updateResizeHandles();
}

QRect Ellipse::getBoundingRect() const
{
    return m_boundingRect;
}

void Ellipse::updateRelativePoints()
{
    relativePoints.clear();
    
    relativePoints.append(QPoint(m_center.x() - m_anchorPoint.x(),
                                 m_center.y() - m_anchorPoint.y()));
    relativePoints.append(QPoint(m_radiusX, m_radiusY));
    relativePoints.append(QPoint(m_angle, 0));
    
    points.clear();
    points.append(m_center);
    points.append(QPoint(m_center.x() + m_radiusX, m_center.y()));
}

void Ellipse::updateBoundingRect()
{
    int penWidth = edgeWidths.isEmpty() ? 3 : edgeWidths[0];
    int halfPen = penWidth / 2;
    
    double rad = m_angle * M_PI / 180.0;
    double cosA = cos(rad);
    double sinA = sin(rad);
    
    int rx = m_radiusX - halfPen;
    int ry = m_radiusY - halfPen;
    
    double a = rx;
    double b = ry;
    
    double t1 = atan2(-b * sinA, a * cosA);
    double t2 = atan2(b * cosA, a * sinA);
    
    double x1 = a * cos(t1) * cosA - b * sin(t1) * sinA;
    double y1 = a * cos(t1) * sinA + b * sin(t1) * cosA;
    
    double x2 = a * cos(t2) * cosA - b * sin(t2) * sinA;
    double y2 = a * cos(t2) * sinA + b * sin(t2) * cosA;
    
    int minX = static_cast<int>(qMin(x1, -x1));
    int maxX = static_cast<int>(qMax(x1, -x1));
    int minY = static_cast<int>(qMin(y2, -y2));
    int maxY = static_cast<int>(qMax(y2, -y2));
    
    m_boundingRect = QRect(m_center.x() - maxX,
                           m_center.y() - maxY,
                           maxX * 2,
                           maxY * 2);
}

void Ellipse::updateResizeHandles()
{
    m_resizeHandles.clear();
    int handleSize = 14;
    
    int penWidth = edgeWidths.isEmpty() ? 3 : edgeWidths[0];
    int halfPen = penWidth / 2;
    
    QPoint rightPoint(m_radiusX - halfPen, 0);
    QPoint rotatedRight = rotatePoint(rightPoint, m_angle);
    QPoint worldRight = m_center + rotatedRight;
    
    m_resizeHandles.append(QRect(
        worldRight.x() - handleSize / 2,
        worldRight.y() - handleSize / 2,
        handleSize,
        handleSize
    ));
}
void Ellipse::setFocus1(const QPoint& pos)
{
    QPoint f2 = getFocus2();
    QPoint oldF1 = getFocus1();
    
    double newFocalDistance = sqrt(pow(pos.x() - f2.x(), 2) + pow(pos.y() - f2.y(), 2));
    
    if (newFocalDistance < 1) newFocalDistance = 1;
    
    m_cachedFocalDistance = newFocalDistance;
    
    double newC = newFocalDistance / 2;
    
    int a = std::max(m_radiusX, m_radiusY);
    int b = std::min(m_radiusX, m_radiusY);
    double newA = sqrt(newC * newC + b * b);
    
    if (newA < 10) newA = 10;
    
    if (m_radiusX >= m_radiusY) {
        m_radiusX = static_cast<int>(newA);
    } else {
        m_radiusY = static_cast<int>(newA);
    }
    
    double angle = atan2(f2.y() - pos.y(), f2.x() - pos.x()) * 180 / M_PI;
    m_angle = static_cast<int>(angle);
    
    updateRelativePoints();
    updateBoundingRect();
    updateResizeHandles();
}

void Ellipse::setFocus2(const QPoint& pos)
{
    QPoint f1 = getFocus1();
    
    double newFocalDistance = sqrt(pow(f1.x() - pos.x(), 2) + pow(f1.y() - pos.y(), 2));
    
    if (newFocalDistance < 1) newFocalDistance = 1;
    
    m_cachedFocalDistance = newFocalDistance;
    
    double newC = newFocalDistance / 2;
    
    int a = std::max(m_radiusX, m_radiusY);
    int b = std::min(m_radiusX, m_radiusY);
    double newA = sqrt(newC * newC + b * b);
    
    if (newA < 10) newA = 10;
    
    if (m_radiusX >= m_radiusY) {
        m_radiusX = static_cast<int>(newA);
    } else {
        m_radiusY = static_cast<int>(newA);
    }
    
    double angle = atan2(pos.y() - f1.y(), pos.x() - f1.x()) * 180 / M_PI;
    m_angle = static_cast<int>(angle);
    
    updateRelativePoints();
    updateBoundingRect();
    updateResizeHandles();
}
QJsonObject Ellipse::toJson() const
{
    QJsonObject json = Shape::toJson();
    json["center_x"] = m_center.x();
    json["center_y"] = m_center.y();
    json["radius_x"] = m_radiusX;
    json["radius_y"] = m_radiusY;
    json["angle"] = m_angle;
    json["focal_distance"] = m_cachedFocalDistance;
    return json;
}

bool Ellipse::fromJson(const QJsonObject& json)
{
    if (!Shape::fromJson(json)) return false;
    m_center = QPoint(json["center_x"].toInt(), json["center_y"].toInt());
    m_radiusX = json["radius_x"].toInt();
    m_radiusY = json["radius_y"].toInt();
    m_angle = json["angle"].toInt();
    m_cachedFocalDistance = json["focal_distance"].toDouble();
    
    updateRelativePoints();
    updateBoundingRect();
    return true;
}