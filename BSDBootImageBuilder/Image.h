#ifndef IMAGE__H
#define IMAGE__H

#include <unordered_map>
#include <string>
#include <functional>

class Blueprint;

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
	enum class ModuleType {
		ElfKernel,
		Binary
	};

	struct ModuleTypeInfo {
		ModuleType type;
	};

	struct MetadataFixup {
		size_t offset;
		std::function<void(uint8_t *data)> handler;
	};

	void writeMetadata(uint32_t type, const void *data, size_t dataSize);
	void writeMetadata32(uint32_t type, uint32_t value);
	void writeMetadataFixup(uint32_t type, std::function<void(uint8_t *data)> &&fixup, size_t length);
	
	void alignAllocationPointer(uint32_t alignment);

	template<typename T>
	void processImageRelocations(std::vector<unsigned char> &image, uint32_t base, const std::vector<T> &relocations);

	void loadExecutable(const std::string &executable, std::vector<unsigned char> &image, uint32_t &entry);

	static const std::unordered_map<std::string, ModuleTypeInfo> m_moduleTypes;

	std::vector<uint32_t> m_metadata;
	uint32_t m_imageBase;
	uint32_t m_allocationPointer;
	uint32_t m_kernelDelta;
	uint32_t m_kernelEntryPoint;
	uint32_t m_metadataBase;
	uint32_t m_kickstartBase;
	uint32_t m_kickstartEntry;
	uint32_t m_imageDisplacement;
	std::vector<uint8_t> m_image;
	std::vector<uint8_t> m_kickstart;
	std::vector<MetadataFixup> m_metadataFixups;
};

#endif
