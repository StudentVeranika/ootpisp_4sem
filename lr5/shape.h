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

#ifdef SHAPESLIB_LIBRARY
#define SHAPESLIB_EXPORT Q_DECL_EXPORT
#else
#define SHAPESLIB_EXPORT Q_DECL_IMPORT
#endif

class SHAPESLIB_EXPORT Shape
{
public:
    Shape();
    virtual ~Shape();
    
    int getId() const;
    void setId(int id);
    QString getName() const;
    void setName(const QString& name);
    bool isSelected() const;
    void setSelected(bool selected);
    bool isVisible() const;
    void setVisible(bool visible);
    bool hasFill() const;
    void setFill(bool fill);
    QColor getFillColor() const;
    void setFillColor(const QColor& color);
    QPoint getAnchorPoint() const;
    void setAnchorPoint(const QPoint& point);
    
    virtual QString getType() const = 0;
    virtual void draw(QPainter& painter) = 0;
    virtual void drawVirtualFigure(QPainter& painter) = 0;
    virtual bool hitTest(const QPoint& point) = 0;
    virtual bool isPointInside(const QPoint& point) = 0;
    virtual void move(int dx, int dy);
    virtual void moveAnchorPoint(const QPoint& newPoint);
    virtual void scale(double factor) = 0;
    virtual void resize(int handleIndex, int deltaX, int deltaY) = 0;
    virtual QRect getBoundingRect() const = 0;
    virtual int getHandleCount() const;
    virtual void updateRelativePoints() = 0;
    virtual int hitTestResizeHandle(const QPoint& point);
    virtual bool hitTestAnchorHandle(const QPoint& point);
    virtual void drawResizeHandles(QPainter& painter);
    virtual void drawAnchorHandle(QPainter& painter);
    virtual void setSideColor(int index, const QColor& color);
    virtual void setSideWidth(int index, int width);
    virtual QColor getSideColor(int index) const;
    virtual int getSideWidth(int index) const;
    virtual QJsonObject toJson() const;
    virtual bool fromJson(const QJsonObject& json);
    
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
    
    virtual void updateResizeHandles();
    virtual void updateAnchorHandle();
    QPointF calculateMiterJoin(const QPointF& p1, const QPointF& p2, const QPointF& p3, float width1, float width2);
    QPointF lineIntersection(const QPointF& p1, const QPointF& p2, const QPointF& p3, const QPointF& p4);
};

#endif
