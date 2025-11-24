cmake_minimum_required(VERSION 3.12)
project(LinuxBootImageBuilder-TargetCode)
enable_language(ASM)

add_subdirectory(kickstart)
add_subdirectory(zynq_init)
