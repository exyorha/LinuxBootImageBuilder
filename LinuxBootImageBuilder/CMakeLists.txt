add_executable(LinuxBootImageBuilder
	Blueprint.cpp
	Blueprint.h
	elf32.h
	FreeBSDTypes.h
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
)

target_link_libraries(LinuxBootImageBuilder PRIVATE lz4)
install(TARGETS LinuxBootImageBuilder DESTINATION bin)
set_target_properties(LinuxBootImageBuilder PROPERTIES CXX_STANDARD 20 CXX_STANDARD_REQUIRED TRUE)
target_include_directories(LinuxBootImageBuilder PRIVATE libfdt)
