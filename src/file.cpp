#include <cstdint>
#include <fstream>
#include <stdexcept>
#include <string>
#include <vector>

std::vector<uint8_t> file_loader(const std::string& filename)
{
	std::ifstream file(filename, std::ios::binary | std::ios::in);
	if (!file) {
		throw std::runtime_error("Failed to open file: " + filename);
	}
	std::vector<uint8_t> buffer((std::istreambuf_iterator<char>(file)),
		std::istreambuf_iterator<char>());
	file.close();
	return buffer;
}
