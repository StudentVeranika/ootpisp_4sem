#ifndef ELLIPSE_H
#define ELLIPSE_H

#include "shape.h"
#include <cmath>
class Ellipse : public Shape
{
public:
    Ellipse();
    explicit Ellipse(const QString& name);
    ~Ellipse();
    
    static Ellipse* createCircle();
    
    void setCenter(const QPoint& center);
    QPoint getCenter() const { return m_center; }
    
    void setRadiusX(int rx);
    int getRadiusX() const { return m_radiusX; }
    
    void setRadiusY(int ry);
    int getRadiusY() const { return m_radiusY; }
    
    void setAngle(int angle);
    int getAngle() const { return m_angle; }
    
    void setAsCircle(int radius);
    
    QPoint getFocus1() const;
    QPoint getFocus2() const;
    void setFocus1(const QPoint& pos);
    void setFocus2(const QPoint& pos);
    double getFocalDistance() const;
    void setFocalDistance(double distance);
    int getFocusHandleIndex(const QPoint& point, int tolerance = 10) const;
    void resizeByFocus(int focusIndex, const QPoint& newPos);
    
    QString getType() const override { return "Ellipse"; }
    void draw(QPainter& painter) override;
    void drawVirtualFigure(QPainter& painter) override;
    bool hitTest(const QPoint& point) override;
    bool isPointInside(const QPoint& point) override;
    void move(int dx, int dy) override;
    void moveAnchorPoint(const QPoint& newPoint) override;
    void scale(double factor) override;
    void resize(int handleIndex, int deltaX, int deltaY) override;
    QRect getBoundingRect() const override;
    void updateRelativePoints() override;
    
    QJsonObject toJson() const override;
    bool fromJson(const QJsonObject& json) override;
    
private:
    QPoint m_center;
    int m_radiusX;
    int m_radiusY;
    int m_angle;
    QRect m_boundingRect;
    double m_cachedFocalDistance;
    
    void updateBoundingRect();
    void updateResizeHandles() override;
    QPoint rotatePoint(const QPoint& point, double angle) const;
    QPoint rotateBackPoint(const QPoint& point, double angle) const;
};

#endif