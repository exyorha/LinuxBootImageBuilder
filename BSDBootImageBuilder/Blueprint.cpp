#include "Blueprint.h"

#include <fstream>
#include <sstream>
#include <stdexcept>
#include <unordered_map>

Blueprint::Blueprint() {

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

	ParsingContext ctx;
	ctx.state = ParsingContext::StateRoot;

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
					processLine(std::move(tokens), ctx);
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
					processLine(std::move(tokens), ctx);
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

void Blueprint::processLine(std::vector<std::string> &&line, ParsingContext &ctx) {
	enum class MetadataValueType {
		None,
		Single,
		Multiple
	};

	static const std::unordered_map<std::string, std::pair<ModuleMetadataType, MetadataValueType>> metadata{
		{ "DTB", { ModuleMetadataType::DTB, MetadataValueType::Single } },
		{ "KERNEND", { ModuleMetadataType::KERNEND, MetadataValueType::None } },
		{ "HOWTO", { ModuleMetadataType::HOWTO, MetadataValueType::Single } },
		{ "ENVIRONMENT", { ModuleMetadataType::ENVIRONMENT, MetadataValueType::Multiple } },
	};

	auto controlToken = line[0];

	auto it = line.begin() + 1;
	auto end = line.end();
	
	switch (ctx.state) {
	case ParsingContext::StateRoot:
		if (controlToken == "MODULE") {
			modules.emplace_back();
			auto &mod = modules.back();

			if (it == end)
				throw std::runtime_error("Module name expected");

			mod.name = std::move(*it++);

			if (it == end)
				throw std::runtime_error("Module type expected");

			mod.type = std::move(*it++);

			if (it == end)
				throw std::runtime_error("Module file name expected");

			mod.fileName = std::move(*it++);

			if (it != end) {
				auto controlToken = std::move(*it++);
				if(controlToken != "METADATA")
					throw std::runtime_error("'METADATA' or end of line is expected");

				ctx.state = ParsingContext::StateMetadata;
			}
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
		}
		else {
			std::stringstream error;
			error << "Invalid token in root context: '" << controlToken << "'\n";
			throw std::runtime_error(error.str());
		}
		break;

	case ParsingContext::StateMetadata:
		if (controlToken == "END") {
			ctx.state = ParsingContext::StateRoot;
		}
		else {
			auto mit = metadata.find(controlToken);
			if (mit == metadata.end()) {
				std::stringstream error;
				error << "Invalid token in metadata context: '" << controlToken << "'\n";
				throw std::runtime_error(error.str());
			}
			else {
				auto &mod = modules.back();
				mod.metadata.emplace_back();
				auto &metadata = mod.metadata.back();

				metadata.type = mit->second.first;
				switch (mit->second.second) {
				case MetadataValueType::None:
					break;

				case MetadataValueType::Single:
					if (it == end) {
						throw std::runtime_error("metadata value expected");
					}

					metadata.singleValue = std::move(*it++);
					break;

				case MetadataValueType::Multiple:
					ctx.state = ParsingContext::StateValues;
					break;
				}
			}
		}
		break;

	case ParsingContext::StateValues:
		if (controlToken == "END") {
			ctx.state = ParsingContext::StateMetadata;
		}
		else if (controlToken == "SET") {
			auto &metadata = modules.back().metadata.back();
			
			metadata.keyValuePairs.emplace_back();
			auto &value = metadata.keyValuePairs.back();

			if (it == end)
				throw std::runtime_error("Key expected");

			value.first = std::move(*it++);

			if (it == end)
				throw std::runtime_error("Value expected");

			value.second = std::move(*it++);
		}
		else {
			std::stringstream error;
			error << "Invalid token in environment context: '" << controlToken << "'\n";
			throw std::runtime_error(error.str());
		}
		break;
	}
}
