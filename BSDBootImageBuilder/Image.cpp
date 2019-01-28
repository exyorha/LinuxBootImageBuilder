#include "Image.h"
#include "Blueprint.h"
#include "FreeBSDTypes.h"
#include "elf32.h"
#include "lz4frame.h"
#include "lz4hc.h"

#include <sstream>
#include <fstream>
#include <algorithm>

struct LZ4FDeleter {
	inline void operator()(LZ4F_cctx *context) const {
		LZ4F_freeCompressionContext(context);
	}
};

static const uint8_t ElfIdentification[EI_NIDENT] = {
	ELFMAG0,
	ELFMAG1,
	ELFMAG2,
	ELFMAG3,
	ELFCLASS32,
	ELFDATA2LSB,
	EV_CURRENT
};

Image::Image() {

}

Image::~Image() {

}

void Image::build(Blueprint &blueprint) {
	m_imageBase = blueprint.imageBase;
	m_allocationPointer = m_imageBase;
	m_image.clear();

	printf("Image base address: %08X\n", m_imageBase);

	for (auto &mod : blueprint.modules) {
		writeMetadata(MODINFO_NAME, mod.name.c_str(), mod.name.size() + 1);
		writeMetadata(MODINFO_TYPE, mod.type.c_str(), mod.type.size() + 1);

		auto infoIt = m_moduleTypes.find(mod.type);
		if (infoIt == m_moduleTypes.end()) {
			std::stringstream error;
			error << "Unknown module type '" << mod.type << "'";
			throw std::runtime_error(error.str());
		}

		auto &info = infoIt->second;
		if (info.type == ModuleType::ElfKernel) {
			alignAllocationPointer(0x00100000); // Kernel base must be aligned to 1MiB
			m_kernelDelta = m_allocationPointer - KERNEL_VADDR;
			printf("Kernel physical base: %08X, virtual base: %08X, delta: %08X\n", m_allocationPointer, KERNEL_VADDR, m_kernelDelta);
		}

		uint32_t base = m_allocationPointer;
		uint32_t size;

		std::ifstream fileStream;
		fileStream.exceptions(std::ios::failbit | std::ios::eofbit | std::ios::badbit);
		fileStream.open(mod.fileName, std::ios::in | std::ios::binary);

		switch (info.type) {
		case ModuleType::ElfKernel:
		{
			Elf32_Ehdr ehdr;
			fileStream.read(reinterpret_cast<char *>(&ehdr), sizeof(ehdr));

			uint32_t limit = base;

			if (memcmp(ehdr.e_ident, ElfIdentification, EI_PAD) != 0 ||
				ehdr.e_type != ET_EXEC ||
				ehdr.e_machine != EM_ARM ||
				ehdr.e_version != EV_CURRENT ||
				ehdr.e_phentsize != sizeof(Elf32_Phdr))
				throw std::runtime_error("Bad ELF identification");

			m_kernelEntryPoint = ehdr.e_entry;

			std::vector<Elf32_Phdr> phdr(ehdr.e_phnum);

			fileStream.seekg(ehdr.e_phoff);
			fileStream.read(reinterpret_cast<char *>(phdr.data()), phdr.size() * sizeof(Elf32_Phdr));

			for (const auto &segment : phdr) {
				if (segment.p_type == PT_LOAD) {
					auto physaddr = segment.p_vaddr + m_kernelDelta;

					limit = std::max<uint32_t>(limit, physaddr + segment.p_memsz);
					if (m_image.size() < limit - m_imageBase) {
						m_image.resize(limit - m_imageBase);
					}

					if (segment.p_filesz > 0) {
						fileStream.seekg(segment.p_offset);
						fileStream.read(reinterpret_cast<char *>(m_image.data() + physaddr - m_imageBase), segment.p_filesz);
					}
				}
			}

			size = limit - base;
		}
		break;

		case ModuleType::Binary:
		{
			fileStream.seekg(0, std::ios::end);
			size = static_cast<uint32_t>(fileStream.tellg());
			fileStream.seekg(0);

			m_image.resize(base + size - m_imageBase);
			fileStream.read(reinterpret_cast<char *>(m_image.data() + base - m_imageBase), size);
		}
		break;
		}

		m_allocationPointer = base + size;
		alignAllocationPointer(4096);

		writeMetadata32(MODINFO_ADDR, base - m_kernelDelta);
		writeMetadata32(MODINFO_SIZE, size);

		printf("%s module %s (from %s): starts at %08X, length %08X\n", mod.type.c_str(), mod.name.c_str(), mod.fileName.c_str(), base, size);

		for (const auto &metadata : mod.metadata) {
			switch (metadata.type) {
			case ModuleMetadataType::DTB:
			{
				uint32_t dtbBase = m_allocationPointer;

				std::ifstream dtbStream;
				dtbStream.exceptions(std::ios::badbit | std::ios::failbit | std::ios::eofbit);
				dtbStream.open(metadata.singleValue, std::ios::in | std::ios::binary);
				dtbStream.seekg(0, std::ios::end);
				uint32_t dtbSize = static_cast<uint32_t>(dtbStream.tellg());
				dtbStream.seekg(0);

				printf("  DTB data: at %08X (virt %08X), size %08X\n", m_allocationPointer, m_allocationPointer - m_kernelDelta, dtbSize);

				m_image.resize(dtbBase + dtbSize - m_imageBase);

				dtbStream.read(reinterpret_cast<char *>(m_image.data() + dtbBase - m_imageBase), dtbSize);

				m_allocationPointer += dtbSize;
				alignAllocationPointer(4096);

				writeMetadata32(MODINFO_METADATA | MODINFOMD_DTBP, dtbBase - m_kernelDelta);
			}
			break;

			case ModuleMetadataType::KERNEND:
				writeMetadataFixup(MODINFO_METADATA | MODINFOMD_KERNEND, [this](unsigned char *target) {
					/*
					 * Kernel end address is set in such way that the kernel, any modules, environment and metadata is preserved, but kickstart code is not.
					 */

					uint32_t value = m_allocationPointer - m_kernelDelta;

					printf("Fixing up KERNEND: %08X\n", value);

					memcpy(target, &value, sizeof(value));
				}, sizeof(uint32_t));
				break;

			case ModuleMetadataType::ENVIRONMENT:
			{
				std::vector<char> environmentBlock;
				for (const auto &variable : metadata.keyValuePairs) {
					environmentBlock.insert(environmentBlock.end(), variable.first.begin(), variable.first.end());
					environmentBlock.push_back('=');
					environmentBlock.insert(environmentBlock.end(), variable.second.begin(), variable.second.end());
					environmentBlock.push_back('\0');
				}
				environmentBlock.push_back('\0');

				uint32_t envBase = m_allocationPointer;
				uint32_t envSize = environmentBlock.size();

				printf("  Environment: at %08X (virt %08X), size %08X\n", envBase, envBase - m_kernelDelta, envSize);

				m_image.resize(envBase + envSize - m_imageBase);
				memcpy(m_image.data() + envBase - m_imageBase, environmentBlock.data(), envSize);

				m_allocationPointer += envSize;
				alignAllocationPointer(4096);

				writeMetadata32(MODINFO_METADATA | MODINFOMD_ENVP, envBase - m_kernelDelta);
			}
			break;

			case ModuleMetadataType::HOWTO:
				writeMetadata32(MODINFO_METADATA | MODINFOMD_HOWTO, std::stoul(metadata.singleValue));
				break;
			}
		}
	}

	writeMetadata(MODINFO_END, nullptr, 0);

	m_metadataBase = m_allocationPointer;
	uint32_t metadataSize = m_metadata.size() * sizeof(uint32_t);

	printf("Metadata: at %08X, size %08X\n", m_metadataBase, metadataSize);

	m_image.resize(m_metadataBase + metadataSize - m_imageBase);
	memcpy(m_image.data() + m_metadataBase - m_imageBase, m_metadata.data(), metadataSize);

	m_allocationPointer += metadataSize;
	alignAllocationPointer(4096);

	m_image.resize(m_allocationPointer - m_imageBase); // Ensure proper zero padding at end

	printf("End of uncompressed image: %08X\n", m_allocationPointer);

	/*
	 * Now that size of the uncompressed image is known, we can fix up relocations in the metadata.
	 */

	for (const auto &fixup : m_metadataFixups) {
		fixup.handler(reinterpret_cast<uint8_t *>(m_image.data() + m_metadataBase - m_imageBase + fixup.offset * sizeof(uint32_t)));
	}

	std::vector<unsigned char> outputBuffer(m_image.size() + 4096);
	size_t outputBufferUsed = 0;

	{
		LZ4F_cctx *rawCtx;

		LZ4F_preferences_t prefs;
		memset(&prefs, 0, sizeof(prefs));
		prefs.frameInfo.blockMode = LZ4F_blockIndependent;
		prefs.compressionLevel = LZ4HC_CLEVEL_MAX;

		LZ4F_compressOptions_t opts;
		memset(&opts, 0, sizeof(opts));
		opts.stableSrc = 1;

		if (LZ4F_createCompressionContext(&rawCtx, LZ4F_VERSION) != 0)
			throw std::runtime_error("LZ4F_createCompressionContext failed");

		std::unique_ptr<LZ4F_cctx, LZ4FDeleter> context(rawCtx);

		auto chunk = LZ4F_compressBegin(
			context.get(),
			outputBuffer.data() + outputBufferUsed,
			outputBuffer.size() - outputBufferUsed,
			&prefs
		);

		if (LZ4F_isError(chunk)) {
			throw std::runtime_error(LZ4F_getErrorName(chunk));
		}

		outputBufferUsed += chunk;

		chunk = LZ4F_compressUpdate(
			context.get(),
			outputBuffer.data() + outputBufferUsed,
			outputBuffer.size() - outputBufferUsed,
			m_image.data(),
			m_image.size(),
			&opts
		);

		if (LZ4F_isError(chunk)) {
			throw std::runtime_error(LZ4F_getErrorName(chunk));
		}

		outputBufferUsed += chunk;

		chunk = LZ4F_compressEnd(
			context.get(),
			outputBuffer.data() + outputBufferUsed,
			outputBuffer.size() - outputBufferUsed,
			&opts
		);

		if (LZ4F_isError(chunk)) {
			throw std::runtime_error(LZ4F_getErrorName(chunk));
		}

		outputBufferUsed += chunk;

	}

	outputBuffer.resize(outputBufferUsed);

	m_imageDisplacement = m_image.size() - outputBuffer.size();

	printf("Compressed image at %08X, %08X bytes (%u%% of original)\n",
		m_imageBase + m_imageDisplacement,
		outputBuffer.size(), outputBuffer.size() * 100 / m_image.size());

	m_image = std::move(outputBuffer);


	printf("Kickstart executable: %s\n", blueprint.kickstart.c_str());

	m_kickstartBase = m_allocationPointer;
	loadExecutable(blueprint.kickstart, m_kickstart, m_kickstartEntry);

	auto kickstartInfo = reinterpret_cast<uint32_t *>(m_kickstart.data());
	kickstartInfo[0] = m_metadataBase - m_kernelDelta;
	kickstartInfo[1] = m_kernelEntryPoint + m_kernelDelta;
	kickstartInfo[2] = m_imageBase + m_imageDisplacement;
	kickstartInfo[3] = m_imageBase;
	
	if (blueprint.initModules.empty()) {
		kickstartInfo[4] = 0;
	}
	else {
		alignAllocationPointer(4);

		auto moduleTable = m_allocationPointer;
		kickstartInfo[4] = moduleTable;

		m_kickstart.resize(m_allocationPointer - m_kickstartBase + sizeof(uint32_t) * (blueprint.initModules.size() + 1));

		m_allocationPointer += sizeof(uint32_t) * (blueprint.initModules.size() + 1);

		size_t index = 0;
		for (const auto &initModule : blueprint.initModules) {
			std::vector<unsigned char> imageData;

			alignAllocationPointer(8);
			auto moduleBase = m_allocationPointer;
			uint32_t imageEntry;
			loadExecutable(initModule, imageData, imageEntry);

			auto moduleLimit = m_allocationPointer;

			printf("Module %s: at %08X, limit %08X, entry %08X\n", initModule.c_str(), moduleBase, moduleLimit, imageEntry);

			m_kickstart.resize(moduleLimit - m_kickstartBase);
			std::copy(imageData.begin(), imageData.end(), m_kickstart.begin() + (moduleBase - m_kickstartBase));

			reinterpret_cast<uint32_t *>(m_kickstart.data() + moduleTable - m_kickstartBase)[index] = imageEntry;
			index++;
		}

		reinterpret_cast<uint32_t *>(m_kickstart.data() + moduleTable - m_kickstartBase)[index] = 0;
	}

}

void Image::loadExecutable(const std::string &executable, std::vector<unsigned char> &image, uint32_t &entry) {
	std::ifstream fileStream;
	fileStream.exceptions(std::ios::failbit | std::ios::eofbit | std::ios::badbit);
	fileStream.open(executable, std::ios::in | std::ios::binary);

	Elf32_Ehdr ehdr;
	fileStream.read(reinterpret_cast<char *>(&ehdr), sizeof(ehdr));

	uint32_t base = m_allocationPointer;
	uint32_t limit = m_allocationPointer;
	uint32_t allocationLimit = m_allocationPointer;

	if (memcmp(ehdr.e_ident, ElfIdentification, EI_PAD) != 0 ||
		ehdr.e_type != ET_EXEC ||
		ehdr.e_machine != EM_ARM ||
		ehdr.e_version != EV_CURRENT ||
		ehdr.e_phentsize != sizeof(Elf32_Phdr))
		throw std::runtime_error("Bad ELF identification");

	entry = ehdr.e_entry + base;

	std::vector<Elf32_Phdr> phdr(ehdr.e_phnum);

	fileStream.seekg(ehdr.e_phoff);
	fileStream.read(reinterpret_cast<char *>(phdr.data()), phdr.size() * sizeof(Elf32_Phdr));

	for (const auto &segment : phdr) {
		if (segment.p_type == PT_LOAD) {
			auto physaddr = segment.p_paddr + base;

			allocationLimit = std::max<uint32_t>(limit, physaddr + segment.p_memsz);
			limit = std::max<uint32_t>(limit, physaddr + segment.p_filesz);
			if (image.size() < limit - base) {
				image.resize(limit - base);
			}

			if (segment.p_filesz > 0) {
				fileStream.seekg(segment.p_offset);
				fileStream.read(reinterpret_cast<char *>(image.data() + physaddr - base), segment.p_filesz);
			}
		}
	}

	uint32_t kickstartSize = allocationLimit - base;
	printf("Kickstart module at %08X, size %08X\n", base, kickstartSize);

	std::vector<Elf32_Shdr> shdr(ehdr.e_shnum);

	fileStream.seekg(ehdr.e_shoff);
	fileStream.read(reinterpret_cast<char *>(shdr.data()), shdr.size() * sizeof(Elf32_Shdr));

	for (const auto &section : shdr) {
		if (section.sh_type == SHT_REL) {
			if ((section.sh_entsize != sizeof(Elf32_Rel) || (section.sh_size % sizeof(Elf32_Rel)) != 0)) {
				throw std::runtime_error("bad relocation section size");
			}

			std::vector<Elf32_Rel> relocations(section.sh_size / sizeof(Elf32_Rel));
			fileStream.seekg(section.sh_offset);
			fileStream.read(reinterpret_cast<char *>(relocations.data()), relocations.size() * sizeof(Elf32_Rel));
			processImageRelocations(image, base, relocations);
		}
		else if (section.sh_type == SHT_RELA) {
			if ((section.sh_entsize != sizeof(Elf32_Rela) || (section.sh_size % sizeof(Elf32_Rela)) != 0)) {
				throw std::runtime_error("bad relocation section size");
			}

			std::vector<Elf32_Rela> relocations(section.sh_size / sizeof(Elf32_Rela));
			fileStream.seekg(section.sh_offset);
			fileStream.read(reinterpret_cast<char *>(relocations.data()), relocations.size() * sizeof(Elf32_Rela));
			processImageRelocations(image, base, relocations);
		}
	}

	m_allocationPointer = allocationLimit;

}

template<typename T>
void Image::processImageRelocations(std::vector<unsigned char> &image, uint32_t base, const std::vector<T> &relocations) {
	for (const auto &reloc : relocations) {
		switch (ELF32_R_TYPE(reloc.r_info)) {
		case R_ARM_ABS32:
			*reinterpret_cast<uint32_t *>(image.data() + reloc.r_offset) += base;
			break;

		case R_ARM_REL32:
		case R_ARM_CALL:
			break;

		case R_ARM_PREL31:
			break;
		
		default:
		{
			std::stringstream error;
			error << "Unsupported relocation type " << static_cast<unsigned int>(ELF32_R_TYPE(reloc.r_info));
			throw std::runtime_error(error.str());
		}
		}
	}
}

void Image::writeMetadata(uint32_t type, const void *data, size_t dataSize) {
	auto words = (dataSize + 3) / 4;
	m_metadata.reserve(m_metadata.size() + 2 + words);

	m_metadata.push_back(type);
	m_metadata.push_back(dataSize);

	if (dataSize > 0) {
		auto dataPos = m_metadata.size();
		m_metadata.resize(dataPos + words);
		memcpy(&m_metadata[dataPos], data, dataSize);
	}
}

void Image::writeMetadataFixup(uint32_t type, std::function<void(uint8_t *data)> &&fixup, size_t dataSize) {
	auto words = (dataSize + 3) / 4;
	m_metadata.reserve(m_metadata.size() + 2 + words);

	m_metadata.push_back(type);
	m_metadata.push_back(dataSize);

	if (dataSize > 0) {
		m_metadataFixups.emplace_back(MetadataFixup{ m_metadata.size(), std::move(fixup) });
		m_metadata.resize(m_metadata.size() + words);
	}
}

void Image::writeMetadata32(uint32_t type, uint32_t value) {
	writeMetadata(type, &value, sizeof(value));
}

void Image::alignAllocationPointer(uint32_t alignment) {
	m_allocationPointer = (m_allocationPointer + (alignment - 1)) & ~(alignment - 1);
}

void Image::writeElf(const std::string &filename) {
	std::ofstream stream;
	stream.exceptions(std::ios::failbit | std::ios::eofbit | std::ios::badbit);
	stream.open(filename, std::ios::out | std::ios::trunc | std::ios::binary);
	writeElf(stream);
}

void Image::writeElf(std::ostream &stream) {
	size_t dataPos = 4096;

	std::vector<Elf32_Phdr> phdrs(2);

	auto &imagePhdr = phdrs[0];
	imagePhdr.p_type = PT_LOAD;
	imagePhdr.p_offset = dataPos;
	imagePhdr.p_vaddr = m_imageBase + m_imageDisplacement;
	imagePhdr.p_paddr = m_imageBase + m_imageDisplacement;
	imagePhdr.p_filesz = m_image.size();
	imagePhdr.p_memsz = m_image.size();
	imagePhdr.p_flags = PF_R | PF_W | PF_X;
	imagePhdr.p_align = 4096;

	dataPos += (imagePhdr.p_filesz + 4095) & ~4095;

	auto &kickstartPhdr = phdrs[1];
	kickstartPhdr.p_type = PT_LOAD;
	kickstartPhdr.p_offset = dataPos;
	kickstartPhdr.p_vaddr = m_kickstartBase;
	kickstartPhdr.p_paddr = m_kickstartBase;
	kickstartPhdr.p_filesz = m_kickstart.size();
	kickstartPhdr.p_memsz = m_allocationPointer - m_kickstartBase;
	kickstartPhdr.p_flags = PF_R | PF_W | PF_X;
	kickstartPhdr.p_align = 4096;

	Elf32_Ehdr ehdr;
	memset(&ehdr, 0, sizeof(ehdr));
	memcpy(ehdr.e_ident, ElfIdentification, sizeof(ElfIdentification));
	ehdr.e_type = ET_EXEC;
	ehdr.e_machine = EM_ARM;
	ehdr.e_version = EV_CURRENT;
	ehdr.e_entry = m_kickstartEntry;
	ehdr.e_phoff = sizeof(ehdr);
	ehdr.e_ehsize = sizeof(ehdr);
	ehdr.e_phentsize = sizeof(Elf32_Phdr);
	ehdr.e_phnum = static_cast<Elf32_Half>(phdrs.size());
	stream.write(reinterpret_cast<char *>(&ehdr), sizeof(ehdr));
	stream.write(reinterpret_cast<char *>(phdrs.data()), phdrs.size() * sizeof(Elf32_Phdr));
	stream.seekp(imagePhdr.p_offset);
	stream.write(reinterpret_cast<char *>(m_image.data()), imagePhdr.p_filesz);
	stream.seekp(kickstartPhdr.p_offset);
	stream.write(reinterpret_cast<char *>(m_kickstart.data()), kickstartPhdr.p_filesz);
}


const std::unordered_map<std::string, Image::ModuleTypeInfo> Image::m_moduleTypes{
	{ "elf kernel", { ModuleType::ElfKernel } },
	{ "md_image", { ModuleType::Binary } }
};
