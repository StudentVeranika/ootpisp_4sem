
#ifndef POLYGON_H
#define POLYGON_H

#include "shape.h"

class Polygon : public Shape
{
public:
    Polygon();
    explicit Polygon(const QString& name);
    ~Polygon();
    
    // Создание из углов и длин
    static Polygon* createFromAnglesAndLengths(const QList<int>& angles, const QList<int>& lengths, const QString& name);
    
    // Управление вершинами
    void addVertex(const QPoint& point);
    void insertVertex(int index, const QPoint& point);
    void removeVertex(int index);
    void clearVertices();
    void closePolygon();
    bool isClosed() const { return m_closed; }
    int vertexCount() const { return points.size(); }
    
    void setSideAngle(int index, int angle);
    void setSideLength(int index, int length);
    void setSideColor(int index, const QColor& color);
    void setSideWidth(int index, int width);
    int getSideAngle(int index) const;
    int getSideLength(int index) const;
    
    int findVertexAtPoint(const QPoint& point, int tolerance = 15);
    
    QString getType() const override { return "Polygon"; }
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
    bool m_closed;
    QRect m_boundingRect;

    bool tryClosePolygonWithCurrentLengths(int changedIndex);
    void adjustPolygonToClose(int changedIndex);
    void updateBoundingRect();
    void rebuildVertexWithAngleAndLength(int index, double angle, double length);
    void rebuildFromVertices();
    void drawWithMiterJoin(QPainter& painter, const QVector<QPoint>& polyPoints);
    double calculateAngle(const QPoint& p1, const QPoint& p2, const QPoint& p3) const;
    void updateResizeHandles() override;
};

#endif // POLYGON_H