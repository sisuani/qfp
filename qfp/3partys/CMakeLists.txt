cmake_minimum_required(VERSION 2.6.0)

if (${QtV} STREQUAL "4")
    find_package(Qt4 COMPONENTS QtCore QtNetwork REQUIRED)
    include(${QT_USE_FILE})
else()
    find_package(Qt5Core REQUIRED)
    find_package(Qt5Network REQUIRED)
endif()


set(3partys_SRCS
    qextserialport/src/qextserialenumerator.cpp
    qextserialport/src/qextserialport.cpp
    logger/helper.cpp
    logger/logger.cpp
)

if(UNIX)
    set(3partys_SRCS ${3partys_SRCS}
        qextserialport/src/qextserialport_unix.cpp
        qextserialport/src/qextserialenumerator_linux.cpp
        qextserialport/src/qextserialenumerator_unix.cpp
    )
endif(UNIX)

if(WIN32)
    set(3partys_SRCS ${3partys_SRCS}
        qextserialport/src/qextserialport_win.cpp
        qextserialport/src/qextserialenumerator_win.cpp
    )
endif(WIN32)

add_subdirectory(qjson)
include_directories(${CMAKE_CURRENT_BINARY_DIR})
include_directories(qextserialport/src)
include_directories(logger)
add_library(3partys STATIC ${3partys_SRCS})
set_target_properties(3partys PROPERTIES AUTOMOC true)

if (${QtV} STREQUAL "5")
    TARGET_LINK_LIBRARIES(3partys Qt5::Core)
    TARGET_LINK_LIBRARIES(3partys Qt5::Network)
endif()

if(WIN32)
    target_link_libraries(3partys ${QT_LIBRARIES})
    target_link_libraries(3partys setupapi)
    target_link_libraries(3partys advapi32)
    target_link_libraries(3partys user32)
endif(WIN32)

