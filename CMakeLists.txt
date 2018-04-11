project(fp)

cmake_minimum_required(VERSION 2.6.0)

SET(QtV "4" CACHE STRING "QtV")

if (${QtV} STREQUAL "4")
    find_package(Qt4 COMPONENTS QtCore QtNetwork REQUIRED)
    include(${QT_USE_FILE})
else()
    find_package(Qt5Core REQUIRED)
    find_package(Qt5Network REQUIRED)
endif()

set(fp_SRCS
    test/commandlinethread.h
    test/errorparser.cpp
    test/main.cpp
    )


include_directories(qfp/3partys/qextserialport/src)
include_directories(qfp/src)
add_subdirectory(qfp)

add_executable(fp ${fp_SRCS})
set_target_properties(fp PROPERTIES AUTOMOC true)
target_link_libraries(fp ${QT_LIBRARIES})
target_link_libraries(fp qfp)

MESSAGE("Qt Version: " ${QtV})
