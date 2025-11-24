#include <stdio.h>

#include <stdexcept>

#include "Blueprint.h"
#include "Image.h"

int main(int argc, char *argv[]) {
	if (argc < 3) {
		fprintf(stderr, "Usage: %s <OUTPUT FILE> <BLUEPRINT FILE>\n", argv[0]);
		return 1;
	}

	Blueprint blueprint;
	try {
		blueprint.parse(argv[2]);
	}
	catch (const std::exception &e) {
		fflush(stdout);
		fprintf(stderr, "Parsing of blueprint file failed: %s\n", e.what());
		fflush(stderr);
		return 1;
	}

	Image image;
	try {
		image.build(blueprint);
	}
	catch (const std::exception &e) {
		fflush(stdout);
		fprintf(stderr, "Image building failed: %s\n", e.what());
		fflush(stderr);
		return 1;
	}

	image.writeElf(argv[1]);

	return 0;
}
