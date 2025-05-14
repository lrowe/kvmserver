#include <cstdint>
#include <cstdio>
#include <stdexcept>
#include <string>
#include <vector>

std::vector<uint8_t> file_loader(const std::string& filename)
{
	FILE *file = fopen(filename.c_str(), "rb");
	if (!file) {
		throw std::runtime_error("Failed to open file: " + filename);
	}
	fseek(file, 0, SEEK_END);
	size_t size = ftell(file);
	fseek(file, 0, SEEK_SET);
	std::vector<uint8_t> buffer(size);
	if (fread(buffer.data(), 1, size, file) != size) {
		fclose(file);
		throw std::runtime_error("Failed to read file: " + filename);
	}
	fclose(file);
	return buffer;
}
