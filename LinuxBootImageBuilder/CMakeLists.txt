add_executable(LinuxBootImageBuilder
	Blueprint.cpp
	Blueprint.h
	elf32.h
	FreeBSDTypes.h
	main.cpp
	Image.cpp
	Image.h
)

target_link_libraries(LinuxBootImageBuilder PRIVATE lz4)
install(TARGETS LinuxBootImageBuilder DESTINATION bin)
