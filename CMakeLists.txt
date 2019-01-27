cmake_minimum_required(VERSION 3.12.1)
project(BSDBootImageBuilder)

if(MSVC)
    SET(CMAKE_CXX_FLAGS "/EHsc")
ENDIF(MSVC)

add_subdirectory(BSDBootImageBuilder)
add_subdirectory(lz4)

export(TARGETS BSDBootImageBuilder FILE ${PROJECT_BINARY_DIR}/exports.cmake)
