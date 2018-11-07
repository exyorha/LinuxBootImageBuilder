add_library(lz4 STATIC
	lz4.c
	lz4.h
	lz4hc.c
	lz4hc.h
	lz4frame.c
	lz4frame.h
	xxhash.c
	xxhash.h
)

target_include_directories(lz4 PUBLIC .)
