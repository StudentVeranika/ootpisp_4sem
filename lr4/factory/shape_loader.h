#ifndef SHAPE_LOADER_H
#define SHAPE_LOADER_H

#include <QString>
#include <QMap>

class Shape;

class ShapeLoader
{
public:
    static ShapeLoader& instance();
    
    bool loadPlugin(const QString& pluginPath);
    Shape* createExternalShape(const QString& type) const;
    QStringList getExternalTypes() const;
    
private:
    ShapeLoader() = default;
    QMap<QString, Shape*(*)()> m_externalCreators;
};

#endif