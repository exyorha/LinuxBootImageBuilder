add_executable(BSDBootImageBuilder
	Blueprint.cpp
	Blueprint.h
	elf32.h
	FreeBSDTypes.h
	main.cpp
	Image.cpp
	Image.h
)

target_link_libraries(BSDBootImageBuilder PRIVATE lz4)
install(TARGETS BSDBootImageBuilder DESTINATION bin)
