

#include "polygon.h"
#include <QPainterPath>
#include <QPen>
#include <QBrush>
#include <algorithm>
#include <climits>
#include <cmath>

Polygon::Polygon()
    : Shape()
    , m_closed(false)
{
    m_name = "Многоугольник";
}

Polygon::Polygon(const QString& name)
    : Shape()
    , m_closed(false)
{
    m_name = name;
}

Polygon::~Polygon()
{
}

void Polygon::addVertex(const QPoint& point)
{
    points.append(point);
    
    if (points.size() == 1) {
        m_anchorPoint = point;
    }
    
    if (points.size() >= 2 && !m_closed) {
        edgeColors.append(Qt::black);
        edgeWidths.append(3);
    }
    
    updateRelativePoints();
    updateBoundingRect();
}

void Polygon::insertVertex(int index, const QPoint& point)
{
    if (index < 0 || index > points.size()) return;
    
    points.insert(index, point);
    
    if (points.size() >= 2 && !m_closed) {
        edgeColors.insert(index, Qt::black);
        edgeWidths.insert(index, 3);
    }
    
    if (m_closed && points.size() >= 3) {
        rebuildFromVertices();
    }
    
    updateRelativePoints();
    updateBoundingRect();
}

void Polygon::removeVertex(int index)
{
    if (index < 0 || index >= points.size()) return;
    
    points.removeAt(index);
    
    if (!m_closed && index < edgeColors.size()) {
        edgeColors.removeAt(index);
        edgeWidths.removeAt(index);
    }
    
    if (m_closed && points.size() < 3) {
        m_closed = false;
    }
    
    if (m_closed && points.size() >= 3) {
        rebuildFromVertices();
    }
    
    updateRelativePoints();
    updateBoundingRect();
}

void Polygon::clearVertices()
{
    points.clear();
    relativePoints.clear();
    edgeColors.clear();
    edgeWidths.clear();
    m_closed = false;
    updateBoundingRect();
}

void Polygon::closePolygon()
{
    if (points.size() < 3) return;
    if (m_closed) return;
    
    m_closed = true;
    rebuildFromVertices();
    updateBoundingRect();
}

void Polygon::rebuildFromVertices()
{
    if (!m_closed || points.size() < 3) return;
    
    updateRelativePoints();
    
    int expectedSegments = points.size();
    while (edgeColors.size() < expectedSegments) {
        edgeColors.append(Qt::black);
        edgeWidths.append(3);
    }
    while (edgeColors.size() > expectedSegments) {
        edgeColors.removeLast();
        edgeWidths.removeLast();
    }
}

void Polygon::setSideAngle(int index, int newAngle)
{
    if (!m_closed || index < 0 || index >= points.size()) return;
    
    newAngle = std::max(1, std::min(179, newAngle));
    
    int n = points.size();
    int prevIdx = (index - 1 + n) % n;
    int nextIdx = (index + 1) % n;
    
    QPoint pPrev = points[prevIdx];
    QPoint pCurr = points[index];
    QPoint pNext = points[nextIdx];
    

    double currentAngle = calculateAngle(pPrev, pCurr, pNext);
    double angleDiff = newAngle - currentAngle;
    
    
    QPointF v2 = QPointF(pNext.x() - pCurr.x(), pNext.y() - pCurr.y());
    double len2 = sqrt(v2.x() * v2.x() + v2.y() * v2.y());
    
    if (len2 < 0.1) return;
    
    
    double angleRad = angleDiff * M_PI / 180.0;
    double newX = v2.x() * cos(angleRad) - v2.y() * sin(angleRad);
    double newY = v2.x() * sin(angleRad) + v2.y() * cos(angleRad);
    
    
    double newLen = sqrt(newX * newX + newY * newY);
    newX = newX * len2 / newLen;
    newY = newY * len2 / newLen;
    
    QPoint newNext = QPoint(
        pCurr.x() + static_cast<int>(newX),
        pCurr.y() + static_cast<int>(newY)
    );
    
    points[nextIdx] = newNext;
    
   
    if (!tryClosePolygonWithCurrentLengths(index)) {
       
        adjustPolygonToClose(index);
    }
    
    updateRelativePoints();
    updateBoundingRect();
    updateResizeHandles();
}
bool Polygon::tryClosePolygonWithCurrentLengths(int changedIndex)
{
    int n = points.size();
    if (n <= 3) return true; 
    
    
    QVector<double> targetLengths;
    for (int i = 0; i < n; ++i) {
        int nextIdx = (i + 1) % n;
        QPoint diff = points[nextIdx] - points[i];
        targetLengths.append(sqrt(diff.x() * diff.x() + diff.y() * diff.y()));
    }
    
   
    QVector<QPoint> tempPoints = points;
    
    for (int i = 1; i < n - 1; ++i) {
        int currIdx = (changedIndex + i) % n;
        int nextIdx = (currIdx + 1) % n;
        int prevIdx = (currIdx - 1 + n) % n;
        
        
        double angle = calculateAngle(tempPoints[prevIdx], tempPoints[currIdx], tempPoints[nextIdx]);
        
        
        QPointF vIn = QPointF(tempPoints[currIdx].x() - tempPoints[prevIdx].x(), 
                               tempPoints[currIdx].y() - tempPoints[prevIdx].y());
        double lenIn = sqrt(vIn.x() * vIn.x() + vIn.y() * vIn.y());
        
        if (lenIn > 0.1) {
            QPointF vInNorm = QPointF(vIn.x() / lenIn, vIn.y() / lenIn);
            
           
            QPointF vOut = QPointF(tempPoints[nextIdx].x() - tempPoints[currIdx].x(),
                                    tempPoints[nextIdx].y() - tempPoints[currIdx].y());
            double cross = vInNorm.x() * vOut.y() - vInNorm.y() * vOut.x();
            double sign = (cross >= 0) ? 1.0 : -1.0;
            
            double angleRad = angle * M_PI / 180.0 * sign;
            
            double newX = vInNorm.x() * cos(angleRad) - vInNorm.y() * sin(angleRad);
            double newY = vInNorm.x() * sin(angleRad) + vInNorm.y() * cos(angleRad);
            
            tempPoints[nextIdx] = QPoint(
                tempPoints[currIdx].x() + static_cast<int>(newX * targetLengths[currIdx]),
                tempPoints[currIdx].y() + static_cast<int>(newY * targetLengths[currIdx])
            );
        }
    }
    
    
    QPoint diff = tempPoints[0] - tempPoints[changedIndex];
    double distToClose = sqrt(diff.x() * diff.x() + diff.y() * diff.y());
    
   
    if (distToClose < 5.0) {
        points = tempPoints;
        return true;
    }
    
    return false;
}

void Polygon::adjustPolygonToClose(int changedIndex)
{
    int n = points.size();
    if (n < 3) return;
    
   
    QPoint anchorPoint = points[changedIndex];
    int nextIdx = (changedIndex + 1) % n;
    
    
    QPoint diff = points[0] - points[changedIndex];
    double gapLength = sqrt(diff.x() * diff.x() + diff.y() * diff.y());
    
    if (gapLength < 0.1) return; // Уже замкнута
    
   
    QVector<QPointF> directions;
    QVector<double> lengths;
    
    for (int i = 0; i < n; ++i) {
        int currIdx = i;
        int nextVertexIdx = (i + 1) % n;
        QPointF dir = QPointF(points[nextVertexIdx].x() - points[currIdx].x(),
                               points[nextVertexIdx].y() - points[currIdx].y());
        double len = sqrt(dir.x() * dir.x() + dir.y() * dir.y());
        
        if (len > 0.1) {
            directions.append(QPointF(dir.x() / len, dir.y() / len));
        } else {
            directions.append(QPointF(1, 0));
        }
        lengths.append(len);
    }
    
   
    double targetGap = 0.0;
    double adjustmentFactor = 0.5;
    int maxIterations = 100;
    int iteration = 0;
    
    while (iteration < maxIterations) {
        QVector<QPoint> testPoints;
        testPoints.append(anchorPoint);
        
        for (int i = 0; i < n - 1; ++i) {
            int dirIdx = (changedIndex + i) % n;
            QPoint prev = testPoints.last();
            QPoint next = QPoint(
                prev.x() + static_cast<int>(directions[dirIdx].x() * lengths[dirIdx]),
                prev.y() + static_cast<int>(directions[dirIdx].y() * lengths[dirIdx])
            );
            testPoints.append(next);
        }
        
       
        QPoint closingDiff = testPoints.last() - testPoints.first();
        double currentGap = sqrt(closingDiff.x() * closingDiff.x() + closingDiff.y() * closingDiff.y());
        
        if (currentGap < 1.0) {
            
            points.clear();
            for (int i = 0; i < n; ++i) {
                points.append(testPoints[(i - changedIndex + n) % n]);
            }
            return;
        }
        
       
        if (currentGap > targetGap) {
            
            for (int i = 1; i < n; ++i) { 
                int lenIdx = (changedIndex + i) % n;
                if (lenIdx != changedIndex) {
                    lengths[lenIdx] *= (1.0 - adjustmentFactor * 0.1);
                    lengths[lenIdx] = std::max(lengths[lenIdx], 5.0); 
                }
            }
        }
        
        iteration++;
    }
    
    
    QVector<QPoint> finalPoints;
    finalPoints.append(anchorPoint);
    
    for (int i = 0; i < n - 2; ++i) {
        int dirIdx = (changedIndex + i) % n;
        QPoint prev = finalPoints.last();
        QPoint next = QPoint(
            prev.x() + static_cast<int>(directions[dirIdx].x() * lengths[dirIdx]),
            prev.y() + static_cast<int>(directions[dirIdx].y() * lengths[dirIdx])
        );
        finalPoints.append(next);
    }
    
   
    finalPoints.append(anchorPoint);
    
    
    points.clear();
    for (int i = 0; i < n; ++i) {
        points.append(finalPoints[(i - changedIndex + n) % n]);
    }
}
void Polygon::rebuildVertexWithAngleAndLength(int index, double angle, double length)
{
    if (!m_closed || index < 0 || index >= points.size()) return;
    
    int n = points.size();
    int prevIdx = (index - 1 + n) % n;
    int nextIdx = (index + 1) % n;
    
    QPoint pPrev = points[prevIdx];
    QPoint pCurr = points[index];
    
   
    QPointF vIn = QPointF(pCurr.x() - pPrev.x(), pCurr.y() - pPrev.y());
    double lenIn = sqrt(vIn.x() * vIn.x() + vIn.y() * vIn.y());
    
    if (lenIn < 0.1) return;
    
  
    QPointF vInNorm = QPointF(vIn.x() / lenIn, vIn.y() / lenIn);
    
    
    double angleRad = angle * M_PI / 180.0;
    
   
    double cross = vInNorm.x() * 0 - vInNorm.y() * 1; 
    
    double newX = vInNorm.x() * cos(angleRad) - vInNorm.y() * sin(angleRad);
    double newY = vInNorm.x() * sin(angleRad) + vInNorm.y() * cos(angleRad);
    
    
    QPoint newNext = QPoint(
        pCurr.x() + static_cast<int>(newX * length),
        pCurr.y() + static_cast<int>(newY * length)
    );
    
    points[nextIdx] = newNext;
}

void Polygon::setSideLength(int index, int newLength)
{
    if (!m_closed || index < 0 || index >= points.size()) return;
    
    newLength = std::max(10, newLength);
    
    int nextIdx = (index + 1) % points.size();
    QPoint diff = points[nextIdx] - points[index];
    int currentLength = static_cast<int>(sqrt(diff.x()*diff.x() + diff.y()*diff.y()));
    
    if (currentLength == 0) return;
    
    double scale = static_cast<double>(newLength) / currentLength;
    QPoint newDiff(
        static_cast<int>(diff.x() * scale),
        static_cast<int>(diff.y() * scale)
    );
    
    points[nextIdx] = points[index] + newDiff;
    
    updateRelativePoints();
    updateBoundingRect();
    updateResizeHandles();
}

void Polygon::setSideColor(int index, const QColor& color)
{
    if (index >= 0 && index < edgeColors.size()) {
        edgeColors[index] = color;
    }
}

void Polygon::setSideWidth(int index, int width)
{
    if (index >= 0 && index < edgeWidths.size()) {
        edgeWidths[index] = width;
    }
}

int Polygon::getSideAngle(int index) const
{
    if (!m_closed || index < 0 || index >= points.size()) return 90;
    
    int prevIdx = (index - 1 + points.size()) % points.size();
    int nextIdx = (index + 1) % points.size();
    
    return static_cast<int>(calculateAngle(points[prevIdx], points[index], points[nextIdx]));
}

int Polygon::getSideLength(int index) const
{
    if (index < 0 || index >= points.size()) return 50;
    
    int nextIdx = (index + 1) % points.size();
    QPoint diff = points[nextIdx] - points[index];
    return static_cast<int>(sqrt(diff.x()*diff.x() + diff.y()*diff.y()));
}

int Polygon::findVertexAtPoint(const QPoint& point, int tolerance)
{
    for (int i = 0; i < points.size(); ++i) {
        int dx = point.x() - points[i].x();
        int dy = point.y() - points[i].y();
        int dist = static_cast<int>(sqrt(dx*dx + dy*dy));
        if (dist <= tolerance) {
            return i;
        }
    }
    return -1;
}

double Polygon::calculateAngle(const QPoint& p1, const QPoint& p2, const QPoint& p3) const
{
    QPointF v1 = QPointF(p1.x() - p2.x(), p1.y() - p2.y());
    QPointF v2 = QPointF(p3.x() - p2.x(), p3.y() - p2.y());
    
    double dot = QPointF::dotProduct(v1, v2);
    double det = v1.x() * v2.y() - v1.y() * v2.x();
    double angle = atan2(det, dot) * 180.0 / M_PI;
    
    angle = std::abs(angle);
    if (angle > 180) angle = 360 - angle;
    
    return angle;
}

void Polygon::drawWithMiterJoin(QPainter& painter, const QVector<QPoint>& polyPoints)
{
    if (polyPoints.size() < 2) return;
    
    int numSegments = polyPoints.size();
    
    for (int i = 0; i < numSegments; ++i) {
        int currIdx = i;
        int nextIdx = (i + 1) % polyPoints.size();
        
        QPointF p1(polyPoints[currIdx]);
        QPointF p2(polyPoints[nextIdx]);
        
        int prevIdx = (currIdx - 1 + polyPoints.size()) % polyPoints.size();
        int nextNextIdx = (nextIdx + 1) % polyPoints.size();
        
        QPointF pPrev = polyPoints[prevIdx];
        QPointF pNext = polyPoints[nextNextIdx];
        
        float widthPrev = (prevIdx < edgeWidths.size()) ? edgeWidths[prevIdx] : 3;
        float widthCurr = (currIdx < edgeWidths.size()) ? edgeWidths[currIdx] : 3;
        float widthNext = (nextIdx < edgeWidths.size()) ? edgeWidths[nextIdx] : 3;
        
        if (widthCurr < 2) widthCurr = 2;
        
        QPointF startJoin = calculateMiterJoin(pPrev, p1, p2, widthPrev, widthCurr);
        QPointF endJoin = calculateMiterJoin(p1, p2, pNext, widthCurr, widthNext);
        
        QVector<QPointF> quadPoints(4);
        quadPoints[0] = p1;
        quadPoints[1] = p2;
        quadPoints[2] = endJoin;
        quadPoints[3] = startJoin;
        
        QColor edgeColor = (currIdx < edgeColors.size()) ? edgeColors[currIdx] : Qt::black;
        
        painter.setBrush(edgeColor);
        painter.setPen(Qt::NoPen);
        painter.drawPolygon(quadPoints);
    }
}

void Polygon::draw(QPainter& painter)
{
    painter.setRenderHint(QPainter::Antialiasing);
    
    if (points.size() < 2) return;
    
    if (m_closed && m_fill && points.size() >= 3) {
        QPainterPath path;
        QPolygon polygon;
        for (const QPoint& p : points) polygon << p;
        path.addPolygon(polygon);
        
        QColor transparentFill = m_fillColor;
        transparentFill.setAlpha(40);
        painter.fillPath(path, QBrush(transparentFill));
    }
    
    drawWithMiterJoin(painter, points);
    
    if (m_isSelected && m_closed) {
        painter.setPen(Qt::blue);
        painter.setFont(QFont("Arial", 10, QFont::Bold));
        
        for (int i = 0; i < points.size(); ++i) {
            int nextIdx = (i + 1) % points.size();
            
            QPoint midPoint(
                (points[i].x() + points[nextIdx].x()) / 2,
                (points[i].y() + points[nextIdx].y()) / 2
            );
            
            painter.setBrush(QColor(255, 255, 255, 200));
            painter.setPen(QPen(Qt::blue, 2));
            painter.drawEllipse(midPoint, 12, 12);
            
            painter.setPen(Qt::blue);
            QRect textRect(midPoint.x() - 8, midPoint.y() - 8, 16, 16);
            painter.drawText(textRect, Qt::AlignCenter, QString::number(i + 1));
        }
    }
}

void Polygon::drawVirtualFigure(QPainter& painter)
{
    if (m_boundingRect.isValid()) {
        QPen dashPen(m_isSelected ? Qt::blue : Qt::gray, 1);
        dashPen.setStyle(Qt::DashLine);
        painter.setPen(dashPen);
        painter.setBrush(Qt::NoBrush);
        painter.drawRect(m_boundingRect);
    }
}

bool Polygon::hitTest(const QPoint& point)
{
    if (!m_closed) return false;
    
    int tolerance = 5;
    
    for (int i = 0; i < points.size(); ++i) {
        int nextIdx = (i + 1) % points.size();
        
        QPointF p1(points[i]);
        QPointF p2(points[nextIdx]);
        QPointF p(point);
        
        QPointF v = p2 - p1;
        QPointF w = p - p1;
        
        float c1 = QPointF::dotProduct(w, v);
        if (c1 <= 0) {
            float dist = sqrt(pow(p.x() - p1.x(), 2) + pow(p.y() - p1.y(), 2));
            if (dist <= tolerance) return true;
            continue;
        }
        
        float c2 = QPointF::dotProduct(v, v);
        if (c2 <= c1) {
            float dist = sqrt(pow(p.x() - p2.x(), 2) + pow(p.y() - p2.y(), 2));
            if (dist <= tolerance) return true;
            continue;
        }
        
        float b = c1 / c2;
        QPointF pb = p1 + b * v;
        float dist = sqrt(pow(p.x() - pb.x(), 2) + pow(p.y() - pb.y(), 2));
        
        if (dist <= tolerance) return true;
    }
    
    QPolygon polygon;
    for (const QPoint& p : points) polygon << p;
    if (polygon.containsPoint(point, Qt::OddEvenFill)) return true;
    
    return m_boundingRect.contains(point);
}

bool Polygon::isPointInside(const QPoint& point)
{
    return hitTest(point);
}

void Polygon::move(int dx, int dy)
{
    m_anchorPoint = QPoint(m_anchorPoint.x() + dx, m_anchorPoint.y() + dy);
    m_boundingRect.translate(dx, dy);
    
    for (int i = 0; i < points.size(); ++i) {
        points[i] = QPoint(points[i].x() + dx, points[i].y() + dy);
    }
    
    updateResizeHandles();
}

void Polygon::moveAnchorPoint(const QPoint& newPoint)
{
    if (!isPointInside(newPoint)) return;
    
    
    QPoint oldAnchor = m_anchorPoint;
    
    
    m_anchorPoint = newPoint;
    
   
    updateRelativePoints();
    updateBoundingRect();
    updateResizeHandles();
}

void Polygon::scale(double factor)
{
    for (int i = 0; i < points.size(); ++i) {
        points[i] = QPoint(
            m_anchorPoint.x() + static_cast<int>((points[i].x() - m_anchorPoint.x()) * factor),
            m_anchorPoint.y() + static_cast<int>((points[i].y() - m_anchorPoint.y()) * factor)
        );
    }
    
    updateRelativePoints();
    updateBoundingRect();
    updateResizeHandles();
}

void Polygon::resize(int handleIndex, int deltaX, int deltaY)
{
    if (handleIndex < 0 || handleIndex >= points.size() || !m_closed) return;
    
    points[handleIndex] = QPoint(
        points[handleIndex].x() + deltaX,
        points[handleIndex].y() + deltaY
    );
    
    updateRelativePoints();
    updateBoundingRect();
    updateResizeHandles();
}

QRect Polygon::getBoundingRect() const
{
    return m_boundingRect;
}

void Polygon::updateRelativePoints()
{
    relativePoints.clear();
    for (const QPoint& p : points) {
        relativePoints.append(QPoint(p.x() - m_anchorPoint.x(), 
                                      p.y() - m_anchorPoint.y()));
    }
}

void Polygon::updateBoundingRect()
{
    if (points.isEmpty()) {
        m_boundingRect = QRect();
        return;
    }
    
    int minX = INT_MAX, minY = INT_MAX;
    int maxX = INT_MIN, maxY = INT_MIN;
    
    for (const QPoint& p : points) {
        minX = std::min(minX, p.x());
        minY = std::min(minY, p.y());
        maxX = std::max(maxX, p.x());
        maxY = std::max(maxY, p.y());
    }
    
    m_boundingRect = QRect(minX, minY, maxX - minX, maxY - minY);
}

void Polygon::updateResizeHandles()
{
    m_resizeHandles.clear();
    int handleSize = 14;
    
    for (const QPoint& p : points) {
        m_resizeHandles.append(QRect(
            p.x() - handleSize / 2,
            p.y() - handleSize / 2,
            handleSize,
            handleSize
        ));
    }
}

QJsonObject Polygon::toJson() const
{
    QJsonObject json = Shape::toJson();
    json["closed"] = m_closed;
    return json;
}

bool Polygon::fromJson(const QJsonObject& json)
{
    if (!Shape::fromJson(json)) return false;
    m_closed = json["closed"].toBool();
    
    if (m_closed && points.size() >= 3) {
        rebuildFromVertices();
    }
    
    updateRelativePoints();
    updateBoundingRect();
    
    return true;
}









