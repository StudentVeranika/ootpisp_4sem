#include "shape_loader.h"
#include "shapes/shape.h"
#include <QLibrary>
#include <QDebug>

typedef Shape* (*CreateShapeFunc)();

ShapeLoader& ShapeLoader::instance()
{
    static ShapeLoader loader;
    return loader;
}

bool ShapeLoader::loadPlugin(const QString& pluginPath)
{
    QLibrary library(pluginPath);
    if (!library.load()) {
        qDebug() << "Failed to load plugin:" << library.errorString();
        return false;
    }
    
    CreateShapeFunc createFunc = (CreateShapeFunc)library.resolve("createShape");
    if (!createFunc) {
        qDebug() << "Failed to resolve createShape function";
        return false;
    }
    
    Shape* shape = createFunc();
    if (shape) {
        m_externalCreators[shape->getType()] = createFunc;
        delete shape;
        return true;
    }
    
    return false;
}

Shape* ShapeLoader::createExternalShape(const QString& type) const
{
    auto it = m_externalCreators.find(type);
    if (it != m_externalCreators.end()) {
        return it.value()();
    }
    return nullptr;
}

QStringList ShapeLoader::getExternalTypes() const
{
    return m_externalCreators.keys();
}