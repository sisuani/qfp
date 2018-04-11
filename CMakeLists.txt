project(fp)

cmake_minimum_required(VERSION 2.6.0)

if ("$ENV{QT_SELECT}" STREQUAL "qt5")
    FIND_PACKAGE(Qt5Core QUIET)
endif()

if (Qt5Core_FOUND)
    find_package(Qt5Core REQUIRED)
    find_package(Qt5Network REQUIRED)
else()
    find_package(Qt4 COMPONENTS QtCore QtNetwork REQUIRED)
    include(${QT_USE_FILE})
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
