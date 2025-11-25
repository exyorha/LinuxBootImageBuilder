cmake_minimum_required(VERSION 3.12)
project(kickstart)
enable_language(ASM)

add_executable(kickstart
  cache_init.S
  decompress.c
  kickstart.S
  zynq_init.S
  ../lzma/LzmaDec.c
  ../lzma/LzmaDec.h
)

target_compile_options(kickstart PRIVATE -Os -Wall -W -ffreestanding -march=armv7-a -mthumb -fno-pic -fvisibility=hidden -munaligned-access)
set_target_properties(kickstart PROPERTIES LINK_FLAGS "-nostdlib -Wl,-T,${CMAKE_CURRENT_SOURCE_DIR}/kickstart.lds,-z,max-page-size=256,--emit-relocs")
set_target_properties(kickstart PROPERTIES LINK_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/kickstart.lds")
set_target_properties(kickstart PROPERTIES C_STANDARD 11)
target_include_directories(kickstart PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../lzma)
target_link_libraries(kickstart PRIVATE -lgcc)

install(TARGETS kickstart EXPORT kickstart DESTINATION share/bsdbootimagebuilder)

export(TARGETS kickstart FILE ${PROJECT_BINARY_DIR}/exports.cmake)
