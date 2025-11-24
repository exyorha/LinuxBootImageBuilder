#ifndef IMAGE__H
#define IMAGE__H

#include <unordered_map>
#include <string>
#include <functional>
#include <cstdint>
#include <optional>

class Blueprint;

struct Elf32_Shdr;

class Image {
public:
	Image();
	~Image();

	Image(const Image &other) = delete;
	Image &operator =(const Image &other) = delete;

	void build(Blueprint &blueprint);

	void writeElf(const std::string &filename);
	void writeElf(std::ostream &stream);

private:
	void alignAllocationPointer(uint32_t alignment);

	template<typename T>
	void processImageRelocations(std::vector<unsigned char> &image, uint32_t base, const std::vector<T> &relocations);

	void loadExecutable(const std::string &executable, std::vector<unsigned char> &image, uint32_t &entry);

	void writeSymbolSection(const Elf32_Shdr &section, uint32_t &esym, const std::vector<Elf32_Shdr> &sections, std::istream &fileStream);

	uint32_t m_imageBase;
	uint32_t m_allocationPointer;
	std::optional<uint32_t> m_kernelDelta;
	uint32_t m_kernelEntryPoint;
	uint32_t m_fdtBase;
	uint32_t m_kickstartBase;
	uint32_t m_kickstartEntry;
	uint32_t m_imageDisplacement;
	std::vector<uint8_t> m_image;
	std::vector<uint8_t> m_kickstart;
	std::vector<uint8_t> m_fdt;
};

#endif
