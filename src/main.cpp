#include <atomic>
#include <cstdio>
#include "mmap_file.hpp"
#include <thread>
#include "vm.hpp"
static std::array<std::atomic<uint64_t>, 64> reset_counters;

int main(int argc, char* argv[], char* envp[])
{
	try {
		Configuration config = Configuration::FromArgs(argc, argv);
		VirtualMachine::init_kvm();

		// Read the binary file
		MmapFile binary_file(config.filename);

		std::unique_ptr<MmapFile> storage_binary_file;
		std::unique_ptr<VirtualMachine> storage_vm;
		std::vector<std::unique_ptr<VirtualMachine>> storage_forks;
		std::mutex storage_vm_mutex;
		if (config.storage) {
			// Load the storage VM binary
			storage_binary_file = std::make_unique<MmapFile>(config.filename);
			// Create the storage VM
			storage_vm = std::make_unique<VirtualMachine>(storage_binary_file->view(), config, true);
			// Make sure only one thread at a time can access the storage VM
			storage_vm->machine().cpu().remote_serializer = &storage_vm_mutex;
			auto init = storage_vm->initialize(nullptr, false);
			if (!storage_vm->is_waiting_for_requests()) {
				fprintf(stderr, "The storage VM did not wait for requests\n");
				return 1;
			}
			printf("Storage VM initialized. init=%lums\n", init.initialization_time.count());
			storage_binary_file->dontneed(); // Lazily drop pages from the file
		}

		// Create a VirtualMachine instance
		VirtualMachine vm(binary_file.view(), config);
		if (storage_vm != nullptr) {
			// Link the main storage VM to the main VM
			vm.machine().remote_connect(storage_vm->machine());
		}
		// Initialize the VM by running through main()
		// and then do a warmup, if required
		const bool just_one_vm = (config.concurrency == 1 && !config.ephemeral);
		auto init = vm.initialize(std::bind(&VirtualMachine::warmup, &vm), just_one_vm);
		// Check if the VM is (likely) waiting for requests
		if (!vm.is_waiting_for_requests()) {
			if (just_one_vm)
				return 0; // It exited normally
			fprintf(stderr, "The program did not wait for requests\n");
			return 1;
		}
		binary_file.dontneed(); // Lazily drop pages from the file

		if (config.storage_1_to_1 && !just_one_vm) {
			// Prepare storage VM for forking
			if (storage_vm == nullptr) {
				fprintf(stderr, "Configuration error: --storage-1-to-1 requires --storage\n");
				return 1;
			}
			storage_vm->machine().prepare_copy_on_write();
			// Create one storage VM per request VM
			storage_forks.resize(config.concurrency);
		}

		// Get warmup time (if any)
		const std::string warmup_time = (init.warmup_time.count() > 0) ?
			(" warmup=" + std::to_string(init.warmup_time.count()) + "ms") : "";
		// Get /proc/self RSS
		std::string process_rss;
		FILE* fp = fopen("/proc/self/statm", "r");
		if (fp) {
			uint64_t size = 0;
			uint64_t rss = 0;
			fscanf(fp, "%lu %lu", &size, &rss);
			fclose(fp);
			rss = (rss * getpagesize()) >> 20; // Convert to MB
			process_rss = " rss=" + std::to_string(rss) + "MB";
		}

		// Print informational message
		std::string method = "epoll";
		if (vm.poll_method() == VirtualMachine::PollMethod::Poll) {
			method = "poll";
		} else if (vm.poll_method() == VirtualMachine::PollMethod::Blocking) {
			method = "blocking";
		} else if (vm.poll_method() == VirtualMachine::PollMethod::Undefined) {
			method = "undefined";
		}
		printf("Program '%s' loaded. %s vm=%u%s huge=%u/%u init=%lums%s%s\n",
			config.filename.c_str(),
			method.c_str(),
			config.concurrency,
			(config.ephemeral ? (config.ephemeral_keep_working_memory ? " ephemeral-kwm" : " ephemeral") : ""),
			config.hugepage_arena_size > 0,
			config.hugepage_requests_arena > 0,
			init.initialization_time.count(),
			warmup_time.c_str(),
			process_rss.c_str());

		// Non-ephemeral single-threaded - we already have a VM
		if (just_one_vm)
		{
			vm.restart_poll_syscall();

			while (true)
			{
				bool failure = false;
				try {
					vm.machine().run();
				} catch (const tinykvm::MachineTimeoutException& me) {
					fprintf(stderr, "*** Main VM timed out\n");
					fprintf(stderr, "Error: %s Data: 0x%#lX\n", me.what(), me.data());
					failure = true;
				} catch (const tinykvm::MemoryException& me) {
					fprintf(stderr, "*** Main VM memory error: %s Addr: 0x%#lX Size: %zu OOM: %d\n",
						me.what(), me.addr(), me.size(), me.is_oom());
					failure = true;
				} catch (const tinykvm::MachineException& me) {
					fprintf(stderr, "*** Main VM Error: %s Data: 0x%#lX\n",
						me.what(), me.data());
					failure = true;
				} catch (const std::exception& e) {
					fprintf(stderr, "*** Main VM Error: %s\n", e.what());
					failure = true;
				}
				if (failure) {
					if (getenv("DEBUG") != nullptr) {
						vm.open_debugger();
					}
				}
			}
		}

		// Start VM forks
		std::vector<std::thread> threads;
		threads.reserve(config.concurrency);

		for (unsigned int i = 0; i < config.concurrency; ++i)
		{
			const bool is_storage_1_to_1 = (config.storage && config.storage_1_to_1);
			threads.emplace_back([&vm, &storage_forks, &storage_vm, i, is_storage_1_to_1]()
			{
				// Create a new VM
				std::unique_ptr<VirtualMachine> forked_vm;
				try {
					// Fork a new VM
					forked_vm = std::make_unique<VirtualMachine>(vm, i, false);
					// Link the specific storage VM to the forked VM
					if (is_storage_1_to_1 && i < storage_forks.size()) {
						storage_forks[i] = std::make_unique<VirtualMachine>(*storage_vm, i, true);
						forked_vm->machine().remote_connect(storage_forks[i]->machine());
					}
					forked_vm->set_on_reset_callback([&vm, i]()
					{
						if (!vm.config().verbose)
							return;
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
					if (getenv("DEBUG_FORK") != nullptr) {
						forked_vm->open_debugger();
					}
				} catch (const tinykvm::MachineTimeoutException& me) {
					fprintf(stderr, "*** Forked VM %u failed to initialize: timed out\n", i);
					fprintf(stderr, "Error: %s Data: 0x%#lX\n", me.what(), me.data());
					return;
				} catch (const tinykvm::MemoryException& me) {
					fprintf(stderr, "*** Forked VM %u failed to initialize: memory error: %s Addr: 0x%#lX Size: %zu OOM: %d\n",
						i, me.what(), me.addr(), me.size(), me.is_oom());
					return;
				} catch (const tinykvm::MachineException& me) {
					fprintf(stderr, "*** Forked VM %u failed to initialize: %s Data: 0x%#lX\n", i, me.what(), me.data());
					return;
				} catch (const std::exception& e) {
					fprintf(stderr, "*** Forked VM %u failed to initialize: %s\n", i, e.what());
					return;
				}
				while (true) {
					bool failure = false;
					try {
						forked_vm->resume_fork();
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
							forked_vm->open_debugger();
						}
					}
					if (vm.is_ephemeral() || failure) {
						printf("Forked VM %u finished. Resetting...\n", i);
						try {
							forked_vm->reset_to(vm);
						} catch (const std::exception& e) {
							fprintf(stderr, "*** Forked VM %u failed to reset: %s\n", i, e.what());
						}
					}
				}
			});
		}

		// Wait for all threads to finish
		for (auto& thread : threads) {
			thread.join();
		}

	} catch (const tinykvm::MachineTimeoutException& me) {
		fprintf(stderr, "Machine timed out\n");
		fprintf(stderr, "Error: %s Data: 0x%lX\n", me.what(), me.data());
		fprintf(stderr, "The server has stopped.\n");
		return 1;
	} catch (const tinykvm::MachineException& me) {
		fprintf(stderr, "Machine not initialized properly\n");
		fprintf(stderr, "Error: %s Data: 0x%lX\n", me.what(), me.data());
		fprintf(stderr, "The server has stopped.\n");
		return 1;
	} catch (const std::exception& e) {
		fprintf(stderr, "Error: %s\n", e.what());
		fprintf(stderr, "The server has stopped.\n");
		return 1;
	}
	return 0;
}
