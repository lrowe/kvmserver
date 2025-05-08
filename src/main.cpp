#include <cstdio>
#include "vm.hpp"
extern std::vector<uint8_t> file_loader(const std::string& filename);

int main(int argc, char* argv[])
{
	try {
		Configuration config = Configuration::FromJsonFile("config.json");
		// Print some configuration values
		printf("Server Address: %s\n", config.server_address.c_str());
		printf("Server Port: %u\n", config.server_port);
		printf("Concurrency: %u\n", config.concurrency);
		printf("Filename: %s\n", config.filename.c_str());
		// Main arguments
		printf("Main arguments: [");
		for (const auto& arg : config.main_arguments) {
			printf("%s ", arg.c_str());
		}
		printf("]\n");
		// Environment variables
		printf("Environment: [");
		for (const auto& env : config.environ) {
			printf("%s ", env.c_str());
		}
		printf("]\n");
		// Allowed paths
		for (const auto& path : config.allowed_paths) {
			printf("Allowed Path: %s -> %s\n", path.real_path.c_str(), path.virtual_path.c_str());
		}

		VirtualMachine::init_kvm();

		// Read the binary file
		std::vector<uint8_t> binary = file_loader(config.filename);

		// Create a VirtualMachine instance
		VirtualMachine vm(binary, config);
		vm.initialize();

	} catch (const std::exception& e) {
		fprintf(stderr, "Error: %s\n", e.what());
		fprintf(stderr, "The server has stopped.\n");
		return 1;
	}
	return 0;
}
