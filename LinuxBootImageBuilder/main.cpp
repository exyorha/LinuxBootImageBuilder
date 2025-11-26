#include <stdio.h>
#include <unistd.h>
#include <getopt.h>

#include <stdexcept>

#include "Blueprint.h"
#include "Image.h"

int main(int argc, char *argv[]) {

	Blueprint blueprint;

	std::optional<std::string> output;

	const struct option options[]{
		{ .name = "output", .has_arg = required_argument },
		{ .name = "dtb", .has_arg = required_argument },
		{ .name = "kernel", .has_arg = required_argument },
		{ .name = "initramfs", .has_arg = required_argument },
		{ .name = "kickstart", .has_arg = required_argument },
		{ .name = "compress", .has_arg = optional_argument },
		{ .name = nullptr }
	};

	int longind;
	int result;

	while((result = getopt_long_only(argc, argv, "", options, &longind)) >= 0) {

		switch(result) {
			case 0:
				switch(longind) {
					case 0:
						output.emplace(optarg);
						break;

					case 1:
						blueprint.dtb.emplace(optarg);
						break;

					case 2:
						blueprint.kernel.emplace(optarg);
						break;

					case 3:
						blueprint.initramfs.emplace(optarg);
						break;

					case 4:
						blueprint.kickstart.emplace(optarg);
						break;

					case 5:
						blueprint.compress = true;
						break;

				default:
					throw std::runtime_error("unexpected longind from getopt_long: " + std::to_string(longind));
				}
				break;

			case '?':
			case ':':
				return 1;

			default:
				throw std::runtime_error("unexpected result from getopt_long: " + std::to_string(result));
		}
	}

	if (argc < 3) {
		fprintf(stderr, "Usage: %s <OUTPUT FILE> <BLUEPRINT FILE>\n", argv[0]);
		return 1;
	}

	if(!output.has_value()) {
		fprintf(stderr, "-output must be specified\n");
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

	image.writeElf(*output);

	return 0;
}
