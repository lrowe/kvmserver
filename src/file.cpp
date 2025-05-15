#include <cstdint>
#include <cstdio>
#include <cstring>
#include <errno.h>
#include <stdexcept>
#include <string>
#include <sys/stat.h>
#include <vector>

std::vector<uint8_t> file_loader(const std::string& filename)
{
	struct stat s;
	if (stat(filename.c_str(), &s) != 0) {
		fprintf(stderr, "Failed to stat file '%s': %s\n",
			filename.c_str(), strerror(errno));
		throw std::runtime_error("Failed to stat file: " + filename);
	}
	if (s.st_size == 0) {
		fprintf(stderr, "File '%s' is empty\n", filename.c_str());
		throw std::runtime_error("File is empty: " + filename);
	}
	if (!S_ISREG(s.st_mode) && !S_ISLNK(s.st_mode)) {
		fprintf(stderr, "File '%s' is not a file\n", filename.c_str());
		throw std::runtime_error("File is not a regular file: " + filename);
	}

	FILE *file = std::fopen(filename.c_str(), "rb");
	if (!file) {
		fprintf(stderr, "Failed to open file '%s': %s\n",
			filename.c_str(), strerror(errno));
		throw std::runtime_error("Failed to open file: " + filename);
	}
	if (std::fseek(file, 0, SEEK_END) != 0) {
		fclose(file);
		fprintf(stderr, "Failed to seek to end of file '%s': %s\n",
			filename.c_str(), strerror(errno));
		throw std::runtime_error("Failed to seek to end of file: " + filename);
	}
	long size = std::ftell(file);
	if (size < 0) {
		fclose(file);
		fprintf(stderr, "Failed to determine file '%s' size: %s\n",
			filename.c_str(), strerror(errno));
		throw std::runtime_error("Failed to determine file size: " + filename);
	}
	if (std::fseek(file, 0, SEEK_SET) != 0) {
		fclose(file);
		fprintf(stderr, "Failed to seek to start of file '%s': %s\n",
			filename.c_str(), strerror(errno));
		throw std::runtime_error("Failed to seek to start of file: " + filename);
	}
	std::vector<uint8_t> buffer(size);
	if (fread(buffer.data(), 1, size, file) != size) {
		fclose(file);
		throw std::runtime_error("Failed to read file: " + filename);
	}
	fclose(file);
	return buffer;
}
