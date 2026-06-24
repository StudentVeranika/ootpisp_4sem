#include <QApplication>
#include "main_window.h"
#include "factory/shape_factory.h"
#include "shapes/polygon.h"
#include "shapes/ellipse.h"
#include "shapes/composite_shape.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    
    ShapeFactory::instance().registerShape("Polygon", []() { return new Polygon(); });
    ShapeFactory::instance().registerShape("Ellipse", []() { return new Ellipse(); });
    ShapeFactory::instance().registerShape("Composite", []() { return new CompositeShape(""); });
    
    MainWindow window;
    window.show();
    
    return app.exec();
}