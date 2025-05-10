#include <atomic>
#include <cstdio>
#include <thread>
#include <sys/signal.h>
#include "vm.hpp"
extern std::vector<uint8_t> file_loader(const std::string& filename);
static std::atomic<uint64_t> g_reset_counter = 0;

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
		const int warmup_requests = 2;
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
				VirtualMachine forked_vm(vm);
				int new_vfd = -1;
				// When a VM is ephemeral we try to detect when a client
				// disconnects, so we can reset the VM and accept a new connection
				// with a clean slate.
				if (vm.config().ephemeral)
				{
					forked_vm.machine().fds().accept_socket_callback =
					[&](int listener_vfd, int listener_fd, int fd, struct sockaddr_storage& addr, socklen_t& addrlen) {
						if (new_vfd != -1) {
							fprintf(stderr, "Forked VM %u already has a connection on fd %d\n", i, new_vfd);
							return -EAGAIN;
						}
						new_vfd = forked_vm.machine().fds().manage(fd, true, true);
						if (verbose) {
							printf("Forked VM %u accepted connection on fd %d\n", i, new_vfd);
						}
						// We no longer accept connections, as ephemeral VMs shouldn't
						// be handling multiple clients at once. If one client sends
						// multiple requests, that is fine.
						for (auto& entry : forked_vm.machine().fds().get_epoll_entries()) {
							auto it = entry.second->epoll_fds.find(listener_vfd);
							if (it != entry.second->epoll_fds.end()) {
								// Remove the listener from the epoll set
								// It will be added back when the VM is reset
								epoll_ctl(entry.first, EPOLL_CTL_DEL, listener_fd, nullptr);
								if (verbose) {
									printf("Forked VM %u removed listener fd %d from epoll set (resets=%lu)\n",
										i, listener_fd, g_reset_counter.load());
								}
								break;
							}
						}
						forked_vm.machine().fds().set_accepting_connections(false);
						return new_vfd;
					};
					forked_vm.machine().fds().free_fd_callback =
					[&](int vfd, tinykvm::FileDescriptors::Entry& entry) -> bool {
						if (vfd == new_vfd) {
							if (verbose) {
								printf("Forked VM %u closed connection on fd %d. Resetting...\n", i, new_vfd);
							}
							new_vfd = -1;
							forked_vm.machine().fds().set_accepting_connections(true);
							forked_vm.reset_to(vm);
							// Progressively print the reset counter
							const uint64_t reset_counter = g_reset_counter.fetch_add(1);
							if (i == 0) {
								if (reset_counter % 64 == 0) {
									fprintf(stderr, "\rForked VM %u has been reset %lu times\n", i, reset_counter);
								} else {
									// Print a dot in between resets
									fprintf(stderr, ".");
								}
							}
							return true; // The VM was reset
						}
						return false; // Nothing happened
					};
				}
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
						fprintf(stderr, "The server has stopped.\n");
						failure = true;
					}
					if (failure) {
						if (getenv("DEBUG") != nullptr) {
							forked_vm.open_debugger();
						} else {
							printf("Forked VM %u finished. Resetting...\n", i);
							new_vfd = -1;
							forked_vm.reset_to(vm);
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
