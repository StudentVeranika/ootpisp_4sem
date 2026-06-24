#ifndef COMPOSITE_SHAPE_H
#define COMPOSITE_SHAPE_H

#include "shape.h"
#include <QList>

class CompositeShape : public Shape
{
public:
    CompositeShape(const QString& name);
    ~CompositeShape();
    void setSelectableOnly(bool selectable) { m_isSelectableOnly = selectable; }
    void addShape(Shape* shape);
    void removeShape(Shape* shape);
    void removeShapeAt(int index);
    void clearShapes();
    int shapeCount() const { return m_shapes.size(); }
    const QList<Shape*>& getShapes() const { return m_shapes; }
    
    void bringToFront(Shape* shape);
    void sendToBack(Shape* shape);
    void bringForward(Shape* shape);
    void sendBackward(Shape* shape);
    
    Shape* hitTestSubShape(const QPoint& point);
    
    QString getType() const override { return "Composite"; }
    void draw(QPainter& painter) override;
    bool hitTest(const QPoint& point) override;
    bool isPointInside(const QPoint& point) override;
    void drawVirtualFigure(QPainter& painter) override;
    void move(int dx, int dy) override;
    void moveAnchorPoint(const QPoint& newPoint) override;
    void scale(double factor) override;
    void resize(int handleIndex, int deltaX, int deltaY) override;
    QRect getBoundingRect() const override;
    void updateRelativePoints() override;
    
    QJsonObject toJson() const override;
    bool fromJson(const QJsonObject& json) override;
    
private:
    QList<Shape*> m_shapes;
    QRect m_boundingRect;
    bool m_isSelectableOnly = false;
    void updateBoundingRect();
    void updateResizeHandles() override;
    void rebuildColorLists();
};

#endif