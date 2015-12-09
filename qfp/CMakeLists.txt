cmake_minimum_required(VERSION 2.6.0)
find_package(Qt4 COMPONENTS QtCore REQUIRED)
include(${QT_USE_FILE})

set(qfp_SRCS
    src/serialport.cpp
    src/driverfiscalepson.cpp
    src/driverfiscalepsonext.cpp
    src/driverfiscalhasar.cpp
    src/packageepson.cpp
    src/packageepsonext.cpp
    src/packagehasar.cpp
    src/fiscalprinter.cpp
)

add_subdirectory(3partys)

include_directories(3partys/qextserialport/src)
include_directories(3partys/logger)
include_directories(src)
add_library(qfp STATIC ${qfp_SRCS})
target_link_libraries(qfp 3partys)
set_target_properties(qfp PROPERTIES AUTOMOC true)
