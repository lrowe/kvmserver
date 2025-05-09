#include <cstdio>
#include <thread>
#include "vm.hpp"
extern std::vector<uint8_t> file_loader(const std::string& filename);

int main(int argc, char* argv[])
{
	try {
		std::string config_file = "config.json";
		if (argc > 1) {
			config_file = argv[1];
		}
		// Load the configuration file
		Configuration config = Configuration::FromJsonFile(config_file);
		// Print some configuration values
		if (getenv("VERBOSE") != nullptr) {
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
		}

		VirtualMachine::init_kvm();

		// Read the binary file
		std::vector<uint8_t> binary = file_loader(config.filename);

		// Create a VirtualMachine instance
		VirtualMachine vm(binary, config);
		int listening_fd = -1;
		vm.machine().fds().set_listening_socket_callback(
			[&](int vfd, int fd) {
				listening_fd = fd;
			});
		vm.machine().fds().set_epoll_wait_callback(
		[&](int vfd, int epfd, int timeout) {
			if (listening_fd != -1) {
				// If the listening socket is found, we are now waiting for
				// requests, so we can fork a new VM.
				vm.set_waiting_for_requests(true);
				vm.machine().stop();
				return false; // Don't call epoll_wait
			}
			return true; // Call epoll_wait
		});
		// Initialize the VM by running through main()
		vm.initialize();
		// Check if the VM is (likely) waiting for requests
		if (!vm.is_waiting_for_requests()) {
			fprintf(stderr, "The program did not wait for requests\n");
			return 1;
		}

		// Start VM forks
		std::vector<std::thread> threads;
		threads.reserve(config.concurrency);
		printf("Starting up %u threads...\n", config.concurrency);

		for (unsigned int i = 0; i < config.concurrency; ++i)
		{
			threads.emplace_back([&vm, i]() {
				// Fork a new VM
				VirtualMachine forked_vm(vm);
				try {
					while (true) {
						forked_vm.machine().run();
					}
				} catch (const tinykvm::MachineTimeoutException& me) {
					fprintf(stderr, "*** Forked VM %u timed out\n", i);
					fprintf(stderr, "Error: %s Data: 0x%#lX\n", me.what(), me.data());
				} catch (const tinykvm::MachineException& me) {
					fprintf(stderr, "*** Forked VM %u Error: %s Data: 0x%#lX\n",
						i, me.what(), me.data());
				} catch (const std::exception& e) {
					fprintf(stderr, "*** Forked VM %u Error: %s\n", i, e.what());
					fprintf(stderr, "The server has stopped.\n");
				}
				if (getenv("DEBUG") != nullptr) {
					forked_vm.open_debugger();
				}
			});
		}

		fprintf(stderr, "Waiting for requests...\n");
		// Wait for all threads to finish
		for (auto& thread : threads) {
			thread.join();
		}

	} catch (const tinykvm::MachineTimeoutException& me) {
		fprintf(stderr, "Machine timed out\n");
		fprintf(stderr, "Error: %s Data: 0x%#lX\n", me.what(), me.data());
		fprintf(stderr, "The server has stopped.\n");
		return 1;
	} catch (const tinykvm::MachineException& me) {
		fprintf(stderr, "Machine not initialized properly\n");
		fprintf(stderr, "Error: %s Data: 0x%#lX\n", me.what(), me.data());
		fprintf(stderr, "The server has stopped.\n");
		return 1;
	} catch (const std::exception& e) {
		fprintf(stderr, "Error: %s\n", e.what());
		fprintf(stderr, "The server has stopped.\n");
		return 1;
	}
	return 0;
}
