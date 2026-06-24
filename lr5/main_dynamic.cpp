#include <QApplication>
#include <QMainWindow>
#include <dlfcn.h>
#include <iostream>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    
    void* handle = dlopen("./libShapesLib.so", RTLD_LAZY);
    
    if (!handle) {
        handle = dlopen("../build/libShapesLib.so", RTLD_LAZY);
    }
    if (!handle) {
        handle = dlopen("libShapesLib.so", RTLD_LAZY);
    }
    
    if (!handle) {
        std::cerr << "dlopen error: " << dlerror() << std::endl;
        return 1;
    }
    
    std::cout << "Library loaded successfully" << std::endl;
    
    return app.exec();
}
