#include "shape_factory.h"
#include "polygon.h"
#include "ellipse.h"

ShapeFactory& ShapeFactory::instance()
{
    static ShapeFactory factory;
    return factory;
}

void ShapeFactory::registerShape(const QString& type, CreatorFunc creator)
{
    m_creators[type] = creator;
}

Shape* ShapeFactory::createShape(const QString& type) const
{
    auto it = m_creators.find(type);
    if (it != m_creators.end()) {
        return it->second();
    }
    return nullptr;
}

Shape* ShapeFactory::createShapeFromJson(const QJsonObject& json) const
{
    QString type = json["type"].toString();
    Shape* shape = createShape(type);
    if (shape && shape->fromJson(json)) {
        return shape;
    }
    delete shape;
    return nullptr;
}

QStringList ShapeFactory::getRegisteredTypes() const
{
    QStringList types;
    for (const auto& pair : m_creators) {
        types.append(pair.first);
    }
    return types;
}