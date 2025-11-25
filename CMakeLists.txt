cmake_minimum_required(VERSION 3.12.1)
project(LinuxBootImageBuilder)

if(MSVC)
    SET(CMAKE_CXX_FLAGS "/EHsc")
ENDIF(MSVC)

add_subdirectory(LinuxBootImageBuilder)

export(TARGETS LinuxBootImageBuilder FILE ${PROJECT_BINARY_DIR}/exports.cmake)
