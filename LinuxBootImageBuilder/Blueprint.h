#ifndef BLUEPRINT__H
#define BLUEPRINT__H

#include <string>
#include <cstdint>
#include <optional>

struct Blueprint {
	uint32_t imageBase = 0x100000;
	std::optional<std::string> kickstart;
	std::optional<std::string> kernel;
	std::optional<std::string> dtb;
	std::optional<std::string> initramfs;
	bool compress = false;
};

#endif
