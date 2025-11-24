#include "Blueprint.h"

#include <fstream>
#include <sstream>
#include <stdexcept>

Blueprint::Blueprint() : imageBase(UINT32_C(0x100000)), compress(false) {

}

Blueprint::~Blueprint() {

}

void Blueprint::parse(const std::string &filename) {
	std::ifstream stream;
	stream.exceptions(std::ios::failbit | std::ios::badbit | std::ios::eofbit);
	stream.open(filename, std::ios::in);
	parse(stream);
}

void Blueprint::parse(std::istream &stream) {
	stream.exceptions(std::ios::badbit);

	enum {
		Normal,
		String,
		Escaped,
		Comment
	} lexerState = Normal;
	std::vector<std::string> tokens;
	std::string tokenBuffer;

	char character;
	bool tokenBufferActive = false;

	while (true) {
		stream.get(character);

		if (stream.fail())
			break;

		switch (lexerState) {
		case Normal:
			if (character == '"') {
				tokenBufferActive = true;
				lexerState = String;
			}
			else if (character == ';') {
				lexerState = Comment;
			}
			else if (isspace((unsigned char)character)) {
				if (tokenBufferActive) {
					tokens.push_back(tokenBuffer);
					tokenBuffer.clear();
					tokenBufferActive = false;
				}

				if (character == '\n' && tokens.size() != 0) {
					processLine(std::move(tokens));
					tokens = std::vector<std::string>();
				}
			}
			else {
				tokenBuffer.push_back(character);
				tokenBufferActive = true;
			}

			break;

		case String:
			if (character == '\\')
				lexerState = Escaped;
			else if (character == '"')
				lexerState = Normal;
			else
				tokenBuffer.push_back(character);

			break;

		case Escaped:
			tokenBuffer.push_back(character);
			lexerState = String;

			break;

		case Comment:
			if (character == '\n') {
				if (tokenBufferActive) {
					tokens.push_back(tokenBuffer);
					tokenBuffer.clear();
					tokenBufferActive = false;
				}

				if (tokens.size() != 0) {
					processLine(std::move(tokens));
					tokens.clear();
				}

				lexerState = Normal;
			}

			break;
		}
	}

	if (lexerState != Normal)
		throw std::runtime_error("End of file reached before closing quote");

	if (tokenBufferActive || !tokens.empty())
		throw std::runtime_error("No newline at the end of file");
}

void Blueprint::processLine(std::vector<std::string> &&line) {

	auto controlToken = line[0];

	auto it = line.begin() + 1;
	auto end = line.end();

	if (controlToken == "KERNEL") {
		if(kernel.has_value())
			throw std::runtime_error("the kernel image is already specified");

		if (it == end)
			throw std::runtime_error("Kernel name expected");

		kernel = std::move(*it++);
	}
	else if (controlToken == "DTB") {
		if(dtb.has_value())
			throw std::runtime_error("the device tree (DTB) image is already specified");

		if (it == end)
			throw std::runtime_error("DTB name expected");

		dtb = std::move(*it++);
	}
	else if (controlToken == "INITRAMFS") {
		if(initramfs.has_value())
			throw std::runtime_error("the initramfs image is already specified");

		if (it == end)
			throw std::runtime_error("Initramfs name expected");

		initramfs = std::move(*it++);
	}
	else if (controlToken == "IMAGE_BASE") {
		if (it == end)
			throw std::runtime_error("Number expected");

		imageBase = std::stoul(*it++, nullptr, 0);
	}
	else if (controlToken == "KICKSTART") {
		if (it == end) {
			throw std::runtime_error("File name expected");
		}

		kickstart = std::move(*it++);
	} else if(controlToken == "COMPRESS") {
		compress = true;
	}
	else {
		std::stringstream error;
		error << "Invalid token in root context: '" << controlToken << "'\n";
		throw std::runtime_error(error.str());
	}
}
