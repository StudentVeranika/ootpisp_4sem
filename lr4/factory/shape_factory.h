#ifndef SHAPE_FACTORY_H
#define SHAPE_FACTORY_H

#include <QString>
#include <QJsonObject>
#include <functional>
#include <map>

class Shape;

class ShapeFactory
{
public:
    static ShapeFactory& instance();
    
    using CreatorFunc = std::function<Shape*()>;
    
    void registerShape(const QString& type, CreatorFunc creator);
    Shape* createShape(const QString& type) const;
    Shape* createShapeFromJson(const QJsonObject& json) const;
    
    QStringList getRegisteredTypes() const;
    
private:
    ShapeFactory() = default;
    std::map<QString, CreatorFunc> m_creators;
};

#endif // SHAPE_FACTORY_H