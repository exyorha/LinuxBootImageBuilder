add_executable(LinuxBootImageBuilder
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
	${CMAKE_CURRENT_SOURCE_DIR}/../lzfse/lzfse.h
	${CMAKE_CURRENT_SOURCE_DIR}/../lzfse/lzfse_encode.c
	${CMAKE_CURRENT_SOURCE_DIR}/../lzfse/lzfse_encode_base.c
	${CMAKE_CURRENT_SOURCE_DIR}/../lzfse/lzfse_encode_tables.h
	${CMAKE_CURRENT_SOURCE_DIR}/../lzfse/lzfse_fse.c
	${CMAKE_CURRENT_SOURCE_DIR}/../lzfse/lzfse_fse.h
	${CMAKE_CURRENT_SOURCE_DIR}/../lzfse/lzfse_internal.h
	${CMAKE_CURRENT_SOURCE_DIR}/../lzfse/lzfse_tunables.h
)

install(TARGETS LinuxBootImageBuilder DESTINATION bin)
set_target_properties(LinuxBootImageBuilder PROPERTIES CXX_STANDARD 20 CXX_STANDARD_REQUIRED TRUE)
target_include_directories(LinuxBootImageBuilder PRIVATE libfdt ${CMAKE_CURRENT_SOURCE_DIR}/../lzfse)
target_compile_definitions(LinuxBootImageBuilder PRIVATE Z7_ST)
