#include "Image.h"
#include "Blueprint.h"
#include "elf32.h"
#include "lzfse.h"
extern "C" {
#include "libfdt.h"
}

#include <sstream>
#include <fstream>
#include <algorithm>
#include <cstring>
#include <memory>

static const uint8_t ElfIdentification[EI_NIDENT] = {
	ELFMAG0,
	ELFMAG1,
	ELFMAG2,
	ELFMAG3,
	ELFCLASS32,
	ELFDATA2LSB,
	EV_CURRENT
};

Image::Image() = default;

Image::~Image() = default;

void Image::writeSymbolSection(const Elf32_Shdr &section, uint32_t &esym, const std::vector<Elf32_Shdr> &sections, std::istream &fileStream) {
	uint32_t size = section.sh_size;
	
	if (m_image.size() < esym + sizeof(size) + size) {
		m_image.resize(esym + sizeof(size) + size - m_imageBase);
	}

	memcpy(m_image.data() + esym - m_imageBase, &size, sizeof(size));

	fileStream.seekg(section.sh_offset);
	fileStream.read(reinterpret_cast<char *>(m_image.data()) + esym + sizeof(size) - m_imageBase, section.sh_size);

	esym = (esym + sizeof(size) + size + 3) & ~3;

	if (section.sh_type == SHT_SYMTAB) {
		writeSymbolSection(sections[section.sh_link], esym, sections, fileStream);
	}

}

static void checkFDTResult(int result) {
	if(result != 0)
		throw std::runtime_error("libfdt error: " + std::string(fdt_strerror(result)));
}

void Image::build(Blueprint &blueprint) {
	std::ifstream fileStream;
	fileStream.exceptions(std::ios::failbit | std::ios::eofbit | std::ios::badbit);

	m_imageBase = blueprint.imageBase;
	m_allocationPointer = m_imageBase;
	m_image.clear();

	printf("Image base address: %08X\n", m_imageBase);

	if(!blueprint.dtb.has_value())
		throw std::runtime_error("The DTB image must be specified to make a valid OS image");

	fileStream.open(*blueprint.dtb, std::ios::in | std::ios::binary);

	fileStream.seekg(0, std::ios::end);
	auto dtbLength = size_t(fileStream.tellg());
	fileStream.seekg(0);

	m_fdt.resize(dtbLength + 8192);

	fileStream.read(reinterpret_cast<char *>(m_fdt.data()), dtbLength);

	fileStream.close();

	checkFDTResult(fdt_open_into(m_fdt.data(), m_fdt.data(), m_fdt.size()));

	if(!blueprint.kernel.has_value())
		throw std::runtime_error("The kernel image must be specified to make a valid OS image");

	alignAllocationPointer(0x00200000); // Kernel base must be aligned to 2MiB

	fileStream.open(*blueprint.kernel, std::ios::in | std::ios::binary);

	Elf32_Ehdr ehdr;
	fileStream.read(reinterpret_cast<char *>(&ehdr), sizeof(ehdr));

	uint32_t limit = m_allocationPointer;

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
			if(!m_kernelDelta.has_value()) {
				uint32_t alignedPaddr = segment.p_paddr & 0xffe00000;

				printf("The kernel is linked for 0x%08X, the actual load address will be 0x%08X\n",
					   alignedPaddr, m_allocationPointer);

				m_kernelDelta.emplace(m_allocationPointer - alignedPaddr);
			}

			auto physaddr = segment.p_paddr + *m_kernelDelta;

			printf("Segment vaddr: %08X, physaddr: %08X, remapped to: %08X\n", segment.p_vaddr, segment.p_paddr, physaddr);

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

	fileStream.close();

	m_allocationPointer = limit;
	alignAllocationPointer(4096);

	auto chosenNode = fdt_path_offset(m_fdt.data(), "/chosen");
	if(chosenNode < 0)
		checkFDTResult(-chosenNode);

	if(blueprint.initramfs.has_value()) {
		auto initramfsBase = m_allocationPointer;

		fileStream.open(*blueprint.initramfs, std::ios::in | std::ios::binary);

		fileStream.seekg(0, std::ios::end);
		auto initramfsLength = size_t(fileStream.tellg());
		fileStream.seekg(0);

		limit = initramfsBase + initramfsLength;

		if (m_image.size() < limit - m_imageBase) {
			m_image.resize(limit - m_imageBase);
		}

		m_fdt.resize(dtbLength + 8192);

		fileStream.read(reinterpret_cast<char *>(m_image.data() + initramfsBase - m_imageBase), initramfsLength);

		fileStream.close();

		m_allocationPointer = limit;
		alignAllocationPointer(4096);

		printf("Initamfs: at 0x%08X, %zu bytes in length\n", initramfsBase, initramfsLength);

        checkFDTResult(fdt_setprop_u32(m_fdt.data(), chosenNode, "linux,initrd-start", (uint32_t)initramfsBase));
        checkFDTResult(fdt_setprop_u32(m_fdt.data(), chosenNode, "linux,initrd-end", (uint32_t)(initramfsBase + initramfsLength)));

	} else {
		fdt_delprop(m_fdt.data(), chosenNode, "linux,initrd-start");
		fdt_delprop(m_fdt.data(), chosenNode, "linux,initrd-end");
	}

	// No modifications will be made to DTB now, copy it to the image.

	m_fdtBase = m_allocationPointer;
	limit = m_allocationPointer + fdt_totalsize(m_fdt.data());
	printf("FDT will be placed at 0x%08X, %u bytes in length\n", m_allocationPointer, limit - m_allocationPointer);

	if (m_image.size() < limit - m_imageBase) {
		m_image.resize(limit - m_imageBase);
	}

	memcpy(m_image.data() + (m_allocationPointer - m_imageBase), m_fdt.data(), fdt_totalsize(m_fdt.data()));

	m_allocationPointer = limit;
	alignAllocationPointer(4096);

	m_imageDisplacement = 0;

	unsigned int uncompressedLength = m_image.size();

	if(blueprint.compress) {
		auto scratch = std::make_unique<unsigned char[]>(lzfse_encode_scratch_size());

		std::vector<unsigned char> outputBuffer(m_image.size());

		size_t outputSize = lzfse_encode_buffer(outputBuffer.data(), outputBuffer.size(),
							m_image.data(), m_image.size(),
							scratch.get());

		if(outputSize == 0) {
			printf("The image is larger compressed than uncompressed, or has failed to compress. Storing as is.\n");
		} else {

			outputBuffer.resize(outputSize);

			m_imageDisplacement = (m_image.size() + 4095) & ~4095;

			printf("Compressed image at %08X, %08zX bytes (%zu%% of original)\n",
				m_imageBase + m_imageDisplacement,
				outputBuffer.size(), outputBuffer.size() * 100 / m_image.size());

			m_image = std::move(outputBuffer);

			m_allocationPointer = m_imageBase + m_imageDisplacement + m_image.size();

			alignAllocationPointer(4096);
		}

	}

	if(!blueprint.kickstart.has_value())
		throw std::runtime_error("the kickstart executable must be specified to build a valid boot image");

	printf("Kickstart executable: %s\n", blueprint.kickstart->c_str());

	m_kickstartBase = m_allocationPointer;
	loadExecutable(*blueprint.kickstart, m_kickstart, m_kickstartEntry);

	auto kickstartInfo = reinterpret_cast<uint32_t *>(m_kickstart.data());
	kickstartInfo[0] = m_fdtBase;
	kickstartInfo[1] = m_kernelEntryPoint + *m_kernelDelta;
	kickstartInfo[2] = m_imageBase + m_imageDisplacement;
	kickstartInfo[3] = m_imageBase;
	kickstartInfo[4] = m_image.size();
	kickstartInfo[5] = uncompressedLength;

	if(m_image.size() < m_allocationPointer - m_imageDisplacement - m_imageBase)
		m_image.resize(m_allocationPointer  - m_imageDisplacement - m_imageBase);

	memcpy(m_image.data() + m_kickstartBase - m_imageDisplacement  - m_imageBase, m_kickstart.data(), m_kickstart.size());

	printf("Final entry point: 0x%08X\n", m_kickstartEntry);
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
		if(section.sh_type != SHT_REL && section.sh_type != SHT_RELA)
			continue;

		auto &dest = shdr[section.sh_info];
		if(!(dest.sh_flags & SHF_ALLOC))
			continue;

		if (section.sh_type == SHT_REL) {
			if ((section.sh_entsize != sizeof(Elf32_Rel) || (section.sh_size % sizeof(Elf32_Rel)) != 0)) {
				throw std::runtime_error("bad relocation section size");
			}

			std::vector<Elf32_Rel> relocations(section.sh_size / sizeof(Elf32_Rel));
			fileStream.seekg(section.sh_offset);
			fileStream.read(reinterpret_cast<char *>(relocations.data()), relocations.size() * sizeof(Elf32_Rel));
			printf("%zu relocations\n", relocations.size());
			processImageRelocations(image, base, relocations);
		}
		else if (section.sh_type == SHT_RELA) {
			if ((section.sh_entsize != sizeof(Elf32_Rela) || (section.sh_size % sizeof(Elf32_Rela)) != 0)) {
				throw std::runtime_error("bad relocation section size");
			}

			std::vector<Elf32_Rela> relocations(section.sh_size / sizeof(Elf32_Rela));
			fileStream.seekg(section.sh_offset);
			fileStream.read(reinterpret_cast<char *>(relocations.data()), relocations.size() * sizeof(Elf32_Rela));
			printf("%zu relocations\n", relocations.size());
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
		case R_ARM_GOT32:
			*reinterpret_cast<uint32_t *>(image.data() + reloc.r_offset) += base;
			break;

		case R_ARM_THM_CALL:
		case R_ARM_REL32:
		case R_ARM_CALL:
		case R_ARM_JUMP24:
		case R_ARM_GOTPC:
		case R_ARM_V4BX:
		case R_ARM_THM_JUMP24:
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

	std::vector<Elf32_Phdr> phdrs(1);

	auto &imagePhdr = phdrs[0];
	imagePhdr.p_type = PT_LOAD;
	imagePhdr.p_offset = dataPos;
	imagePhdr.p_vaddr = m_imageBase + m_imageDisplacement;
	imagePhdr.p_paddr = m_imageBase + m_imageDisplacement;
	imagePhdr.p_filesz = m_image.size();
	imagePhdr.p_memsz = m_image.size();
	imagePhdr.p_flags = PF_R | PF_W | PF_X;
	imagePhdr.p_align = 4096;

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
}
