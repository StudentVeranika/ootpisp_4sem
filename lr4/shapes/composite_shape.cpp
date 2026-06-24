#include "composite_shape.h"
#include "polygon.h"
#include "ellipse.h"
#include <QPainterPath>
#include <climits>

CompositeShape::CompositeShape(const QString& name)
    : Shape()
{
    m_name = name;
}

CompositeShape::~CompositeShape()
{
    qDeleteAll(m_shapes);
    m_shapes.clear();
}

void CompositeShape::addShape(Shape* shape)
{
    if (!shape) return;
    m_shapes.append(shape);
    
    for (int i = 0; i < shape->edgeColors.size(); ++i) {
        edgeColors.append(shape->edgeColors[i]);
        edgeWidths.append(shape->edgeWidths[i]);
    }
    
    updateBoundingRect();
    updateRelativePoints();
}

void CompositeShape::removeShape(Shape* shape)
{
    if (m_shapes.removeOne(shape)) {
        delete shape;
        rebuildColorLists();
        updateBoundingRect();
        updateRelativePoints();
    }
}

void CompositeShape::removeShapeAt(int index)
{
    if (index >= 0 && index < m_shapes.size()) {
        delete m_shapes[index];
        m_shapes.removeAt(index);
        rebuildColorLists();
        updateBoundingRect();
        updateRelativePoints();
    }
}

void CompositeShape::clearShapes()
{
    qDeleteAll(m_shapes);
    m_shapes.clear();
    edgeColors.clear();
    edgeWidths.clear();
    points.clear();
    relativePoints.clear();
    m_boundingRect = QRect();
}

void CompositeShape::bringToFront(Shape* shape)
{
    int index = m_shapes.indexOf(shape);
    if (index >= 0 && index < m_shapes.size() - 1) {
        m_shapes.removeAt(index);
        m_shapes.append(shape);
        rebuildColorLists();
        updateBoundingRect();
    }
}

void CompositeShape::sendToBack(Shape* shape)
{
    int index = m_shapes.indexOf(shape);
    if (index > 0) {
        m_shapes.removeAt(index);
        m_shapes.prepend(shape);
        rebuildColorLists();
        updateBoundingRect();
    }
}

void CompositeShape::bringForward(Shape* shape)
{
    int index = m_shapes.indexOf(shape);
    if (index >= 0 && index < m_shapes.size() - 1) {
        m_shapes.removeAt(index);
        m_shapes.insert(index + 1, shape);
        rebuildColorLists();
        updateBoundingRect();
    }
}

void CompositeShape::sendBackward(Shape* shape)
{
    int index = m_shapes.indexOf(shape);
    if (index > 0) {
        m_shapes.removeAt(index);
        m_shapes.insert(index - 1, shape);
        rebuildColorLists();
        updateBoundingRect();
    }
}

Shape* CompositeShape::hitTestSubShape(const QPoint& point)
{
    for (int i = m_shapes.size() - 1; i >= 0; --i) {
        if (m_shapes[i]->hitTest(point)) {
            return m_shapes[i];
        }
    }
    return nullptr;
}

void CompositeShape::rebuildColorLists()
{
    edgeColors.clear();
    edgeWidths.clear();
    
    for (Shape* shape : m_shapes) {
        for (int i = 0; i < shape->edgeColors.size(); ++i) {
            edgeColors.append(shape->edgeColors[i]);
            edgeWidths.append(shape->edgeWidths[i]);
        }
    }
}

void CompositeShape::draw(QPainter& painter)
{
    for (Shape* shape : m_shapes) {
        shape->draw(painter);
    }
}

bool CompositeShape::hitTest(const QPoint& point)
{
    if (m_isSelectableOnly) {
        return m_boundingRect.contains(point);
    }
    return hitTestSubShape(point) != nullptr || m_boundingRect.contains(point);
}

bool CompositeShape::isPointInside(const QPoint& point)
{
    return hitTest(point);
}

void CompositeShape::move(int dx, int dy)
{
    m_anchorPoint = QPoint(m_anchorPoint.x() + dx, m_anchorPoint.y() + dy);
    m_boundingRect.translate(dx, dy);
    
    for (Shape* shape : m_shapes) {
        shape->move(dx, dy);
    }
    
    updateRelativePoints();
}

void CompositeShape::moveAnchorPoint(const QPoint& newPoint)
{
    if (!isPointInside(newPoint)) return;
    
    m_anchorPoint = newPoint;
    
    updateRelativePoints();
    updateBoundingRect();
}

void CompositeShape::scale(double factor)
{
    for (Shape* shape : m_shapes) {
        shape->scale(factor);
    }
    
    updateBoundingRect();
    updateRelativePoints();
}

void CompositeShape::resize(int handleIndex, int deltaX, int deltaY)
{
    Q_UNUSED(handleIndex);
    Q_UNUSED(deltaX);
    Q_UNUSED(deltaY);
}

QRect CompositeShape::getBoundingRect() const
{
    return m_boundingRect;
}

void CompositeShape::updateRelativePoints()
{
    relativePoints.clear();
    points.clear();
    
    QRect rect = getBoundingRect();
    if (rect.isValid()) {
        points.append(rect.topLeft());
        points.append(rect.topRight());
        points.append(rect.bottomRight());
        points.append(rect.bottomLeft());
        
        for (const QPoint& p : points) {
            relativePoints.append(QPoint(p.x() - m_anchorPoint.x(), 
                                         p.y() - m_anchorPoint.y()));
        }
    }
    
    updateResizeHandles();
}

void CompositeShape::updateBoundingRect()
{
    if (m_shapes.isEmpty()) {
        m_boundingRect = QRect();
        return;
    }
    
    int minX = INT_MAX, minY = INT_MAX;
    int maxX = INT_MIN, maxY = INT_MIN;
    
    for (Shape* shape : m_shapes) {
        QRect rect = shape->getBoundingRect();
        minX = qMin(minX, rect.left());
        minY = qMin(minY, rect.top());
        maxX = qMax(maxX, rect.right());
        maxY = qMax(maxY, rect.bottom());
    }
    
    m_boundingRect = QRect(minX, minY, maxX - minX, maxY - minY);
}

void CompositeShape::updateResizeHandles()
{
    m_resizeHandles.clear();
    int handleSize = 14;
    
    QRect rect = getBoundingRect();
    if (rect.isValid()) {
        QVector<QPoint> corners = {
            rect.topLeft(), rect.topRight(),
            rect.bottomRight(), rect.bottomLeft()
        };
        
        for (const QPoint& p : corners) {
            m_resizeHandles.append(QRect(
                p.x() - handleSize / 2,
                p.y() - handleSize / 2,
                handleSize,
                handleSize
            ));
        }
    }
}

QJsonObject CompositeShape::toJson() const
{
    QJsonObject json = Shape::toJson();
    json["sub_shape_count"] = m_shapes.size();
    
    QJsonArray subShapesArray;
    for (Shape* shape : m_shapes) {
        subShapesArray.append(shape->toJson());
    }
    json["sub_shapes"] = subShapesArray;
    
    return json;
}
void CompositeShape::drawVirtualFigure(QPainter& painter)
{
    if (m_boundingRect.isValid()) {
        QPen dashPen(m_isSelected ? Qt::blue : Qt::gray, 1);
        dashPen.setStyle(Qt::DashLine);
        painter.setPen(dashPen);
        painter.setBrush(Qt::NoBrush);
        painter.drawRect(m_boundingRect);
    }
}
bool CompositeShape::fromJson(const QJsonObject& json)
{
    if (!Shape::fromJson(json)) return false;
    
    clearShapes();
    
    QJsonArray subShapesArray = json["sub_shapes"].toArray();
    for (const QJsonValue& value : subShapesArray) {
        QJsonObject shapeJson = value.toObject();
        QString type = shapeJson["type"].toString();
        
        Shape* shape = nullptr;
        if (type == "Polygon") {
            shape = new Polygon();
        } else if (type == "Ellipse") {
            shape = new Ellipse();
        } else if (type == "Composite") {
            shape = new CompositeShape("");
        }
        
        if (shape && shape->fromJson(shapeJson)) {
            m_shapes.append(shape);
        } else {
            delete shape;
        }
    }
    
    rebuildColorLists();
    updateBoundingRect();
    updateRelativePoints();
    
    return true;
}