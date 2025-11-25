add_executable(LinuxBootImageBuilder
	Blueprint.cpp
	Blueprint.h
	elf32.h
	main.cpp
	Image.cpp
	Image.h
	libfdt/fdt.c
	libfdt/fdt.h
	libfdt/fdt_empty_tree.c
	libfdt/fdt_ro.c
	libfdt/fdt_rw.c
	libfdt/fdt_strerror.c
	libfdt/fdt_sw.c
	libfdt/fdt_wip.c
	libfdt/libfdt.h
	libfdt/libfdt_env.h
	libfdt/libfdt_internal.h
	${CMAKE_CURRENT_SOURCE_DIR}/../lzma/7zTypes.h
	${CMAKE_CURRENT_SOURCE_DIR}/../lzma/Compiler.h
	${CMAKE_CURRENT_SOURCE_DIR}/../lzma/CpuArch.c
	${CMAKE_CURRENT_SOURCE_DIR}/../lzma/CpuArch.h
	${CMAKE_CURRENT_SOURCE_DIR}/../lzma/LzFind.c
	${CMAKE_CURRENT_SOURCE_DIR}/../lzma/LzFind.h
	${CMAKE_CURRENT_SOURCE_DIR}/../lzma/LzHash.h
	${CMAKE_CURRENT_SOURCE_DIR}/../lzma/LzmaEnc.c
	${CMAKE_CURRENT_SOURCE_DIR}/../lzma/LzmaEnc.h
	${CMAKE_CURRENT_SOURCE_DIR}/../lzma/Precomp.h
)

install(TARGETS LinuxBootImageBuilder DESTINATION bin)
set_target_properties(LinuxBootImageBuilder PROPERTIES CXX_STANDARD 20 CXX_STANDARD_REQUIRED TRUE)
target_include_directories(LinuxBootImageBuilder PRIVATE libfdt ${CMAKE_CURRENT_SOURCE_DIR}/../lzma)
target_compile_definitions(LinuxBootImageBuilder PRIVATE Z7_ST)
