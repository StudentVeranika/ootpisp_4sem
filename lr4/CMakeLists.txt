cmake_minimum_required(VERSION 3.16)
project(ShapesEditor)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt5 REQUIRED COMPONENTS Widgets Core Gui)

set(SOURCES
    main.cpp
    main_window.cpp
    factory/shape_factory.cpp
    factory/shape_loader.cpp
    shapes/shape.cpp
    shapes/polygon.cpp
    shapes/ellipse.cpp
    shapes/composite_shape.cpp
)

set(HEADERS
    main_window.h
    factory/shape_factory.h
    factory/shape_loader.h
    shapes/shape.h
    shapes/polygon.h
    shapes/ellipse.h
    shapes/composite_shape.h
)

add_executable(${PROJECT_NAME} ${SOURCES} ${HEADERS})

target_link_libraries(${PROJECT_NAME} 
    Qt5::Widgets 
    Qt5::Core 
    Qt5::Gui
)

target_include_directories(${PROJECT_NAME} PRIVATE 
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/shapes
    ${CMAKE_CURRENT_SOURCE_DIR}/factory
)