cmake_minimum_required(VERSION 2.6.0)

if ("$ENV{QT_SELECT}" STREQUAL "qt5")
    FIND_PACKAGE(Qt5Core QUIET)
endif()

IF (Qt5Core_FOUND)
    find_package(Qt5Core REQUIRED)
    find_package(Qt5Network REQUIRED)
else()
    find_package(Qt4 COMPONENTS QtCore QtNetwork REQUIRED)
    include(${QT_USE_FILE})
endif()

set(qfp_SRCS
    src/serialport.cpp
    src/driverfiscalepson.cpp
    src/driverfiscalepsonext.cpp
    src/driverfiscalhasar.cpp
    src/driverfiscalhasar2g.cpp
    src/packageepson.cpp
    src/packageepsonext.cpp
    src/packagehasar.cpp
    src/fiscalprinter.cpp
    src/networkport.cpp
)

add_subdirectory(3partys)

include_directories(3partys/qjson/include)
include_directories(3partys/qextserialport/src)
include_directories(3partys/logger)
include_directories(src)
add_library(qfp STATIC ${qfp_SRCS})
target_link_libraries(qfp 3partys)
target_link_libraries(qfp qjson)
set_target_properties(qfp PROPERTIES AUTOMOC true)
