#include <atomic>
#include <cstdio>
#include <thread>
#include <sys/signal.h>
#include "vm.hpp"
extern std::vector<uint8_t> file_loader(const std::string& filename);
static std::array<std::atomic<uint64_t>, 64> reset_counters;

int main(int argc, char* argv[])
{
	signal(SIGPIPE, SIG_IGN); // How much misery has this misfeature caused?
	const bool verbose = getenv("VERBOSE") != nullptr;
	try {
		std::string config_file = "config.json";
		if (argc > 1) {
			config_file = argv[1];
		}
		// Load the configuration file
		Configuration config = Configuration::FromJsonFile(config_file);
		// Print some configuration values
		if (getenv("VERBOSE") != nullptr) {
			printf("Filename: %s\n", config.filename.c_str());
			printf("Concurrency: %u\n", config.concurrency);
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
		vm.machine().fds().listening_socket_callback =
		[&](int vfd, int fd) {
			listening_fd = fd;
		};
		vm.machine().fds().epoll_wait_callback =
		[&](int vfd, int epfd, int timeout) {
			if (listening_fd != -1) {
				// If the listening socket is found, we are now waiting for
				// requests, so we can fork a new VM.
				vm.set_waiting_for_requests(true);
				vm.machine().stop();
				return false; // Don't call epoll_wait
			}
			return true; // Call epoll_wait
		};
		// Initialize the VM by running through main()
		const int warmup_requests = 500;
		vm.initialize([&] {
			// No need to warm up the JIT compiler if we are not using ephemeral VMs
			if (!config.ephemeral) {
				return;
			}
			printf("Warming up the guest VM...\n");
			vm.set_waiting_for_requests(false);
			// Waiting for a certain amount of requests in order
			// to warm up the JIT compiler in the VM
			int freed_fds = 0;
			auto old_listening_fd = listening_fd;
			listening_fd = -1;
			vm.machine().fds().free_fd_callback =
			[&](int vfd, tinykvm::FileDescriptors::Entry& entry) -> bool {
				freed_fds++;
				if (freed_fds >= warmup_requests) {
					fprintf(stderr, "Warmed up the JIT compiler\n");
					vm.set_waiting_for_requests(true);
					vm.machine().stop();
					return true; // The VM was reset
				}
				return false; // Nothing happened
			};
			// Run the VM until it stops
			vm.machine().run();
			// Make sure the program is waiting for requests
			if (!vm.is_waiting_for_requests()) {
				fprintf(stderr, "The program did not wait for requests after warmup\n");
				throw std::runtime_error("The program did not wait for requests after warmup");
			}
			// Restore the listening socket
			listening_fd = old_listening_fd;
		});
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
			threads.emplace_back([&vm, i, verbose]()
			{
				// Fork a new VM
				VirtualMachine forked_vm(vm, i);
				forked_vm.set_on_reset_callback([&vm, i]()
				{
					// Progressively print the reset counter
					const uint64_t reset_counter = reset_counters.at(i).fetch_add(1);
					if (i == 0) {
						if (reset_counter % 64 == 0) {
							std::string counters_str;
							for (unsigned int j = 0; j < vm.config().concurrency; ++j) {
								counters_str += std::to_string(j) + ": " + std::to_string(reset_counters[j].load()) + " ";
							}
							fprintf(stderr, "\rForked VMs have been reset: %s\n", counters_str.c_str());
						} else {
							// Print a dot in between resets
							fprintf(stderr, ".");
						}
					}
				});
				while (true) {
					bool failure = false;
					try {
						forked_vm.machine().vmresume();
					} catch (const tinykvm::MachineTimeoutException& me) {
						fprintf(stderr, "*** Forked VM %u timed out\n", i);
						fprintf(stderr, "Error: %s Data: 0x%#lX\n", me.what(), me.data());
						failure = true;
					} catch (const tinykvm::MachineException& me) {
						fprintf(stderr, "*** Forked VM %u Error: %s Data: 0x%#lX\n",
							i, me.what(), me.data());
						failure = true;
					} catch (const std::exception& e) {
						fprintf(stderr, "*** Forked VM %u Error: %s\n", i, e.what());
						failure = true;
					}
					if (failure) {
						if (getenv("DEBUG") != nullptr) {
							forked_vm.open_debugger();
						}
					}
					if (vm.config().ephemeral) {
						printf("Forked VM %u finished. Resetting...\n", i);
						try {
							forked_vm.reset_to(vm);
						} catch (const std::exception& e) {
							fprintf(stderr, "*** Forked VM %u failed to reset: %s\n", i, e.what());
						}
					}
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
