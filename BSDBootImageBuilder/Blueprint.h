#ifndef BLUEPRINT__H
#define BLUEPRINT__H

#include <string>
#include <vector>

enum class ModuleMetadataType {
	DTB,
	KERNEND,
	HOWTO,
	ENVIRONMENT
};

struct ModuleMetadata {
	ModuleMetadataType type;
	std::string singleValue; // DTB, HOWTO
	std::vector<std::pair<std::string, std::string>> keyValuePairs; // ENVIRONMENT
};

struct Module {
	std::string name;
	std::string type;
	std::string fileName;
	std::vector<ModuleMetadata> metadata;
};

class Blueprint {
public:
	Blueprint();
	~Blueprint();

	Blueprint(const Blueprint &other) = delete;
	Blueprint &operator =(const Blueprint &other) = delete;

	void parse(const std::string &filename);
	void parse(std::istream &stream);

	std::vector<Module> modules;
	uint32_t imageBase;
	std::string kickstart;
	std::vector<std::string> initModules;
	bool compress;

private:
	struct ParsingContext {
		enum {
			StateRoot,
			StateMetadata,
			StateValues
		} state;
	};

	void processLine(std::vector<std::string> &&line, ParsingContext &ctx);
};

#endif
