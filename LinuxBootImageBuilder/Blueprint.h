#ifndef BLUEPRINT__H
#define BLUEPRINT__H

#include <string>
#include <vector>
#include <cstdint>
#include <optional>

class Blueprint {
public:
	Blueprint();
	~Blueprint();

	Blueprint(const Blueprint &other) = delete;
	Blueprint &operator =(const Blueprint &other) = delete;

	void parse(const std::string &filename);
	void parse(std::istream &stream);

	uint32_t imageBase;
	std::optional<std::string> kickstart;
	std::vector<std::string> initModules;
	std::optional<std::string> kernel;
	std::optional<std::string> dtb;
	std::optional<std::string> initramfs;

	bool compress;

private:
	void processLine(std::vector<std::string> &&line);
};

#endif
