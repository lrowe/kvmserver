#include <atomic>
#include <cstdio>
#include <filesystem>
#include <getopt.h>
#include <sstream>
#include <thread>
#include <unistd.h>
#include "vm.hpp"
extern std::vector<uint8_t> file_loader(const std::string& filename);
static std::array<std::atomic<uint64_t>, 64> reset_counters;

std::string lookup_program(const std::string& program)
{
	namespace fs = std::filesystem;
	if (program.find('/') == std::string::npos) {
		std::stringstream ss(getenv("PATH"));
		std::string dirname;
		while (std::getline(ss, dirname, ':')) {
			fs::path filepath = (fs::path(dirname) / program).lexically_normal();
			if (access(filepath.c_str(), X_OK) == 0) {
				return filepath;
			}
		}
		throw std::invalid_argument("Program not found on path: " + program);
	}
	fs::path filepath = fs::absolute(program).lexically_normal();
	if (access(filepath.c_str(), X_OK) == 0) {
		return filepath;
	}
	throw std::invalid_argument("Program not an executable: " + program);
}

struct CommandLineArgs
{
	int concurrency = -1;
	bool ephemeral = false;
	bool verbose = false;
	bool allow_read = false;
	bool allow_write = false;
	bool allow_env = false;
	uint16_t warmup_requests = 0;
	std::string config_file;
	std::string filename;
	std::vector<std::string> remaining_args;
};
const int ALLOW_ALL = 0x100;
const int ALLOW_READ = 0x101;
const int ALLOW_WRITE = 0x102;
const int ALLOW_ENV = 0x103;
static const struct option longopts[] = {
	{"program", required_argument, nullptr, 'p'},
	{"config", required_argument, nullptr, 'c'},
	{"threads", required_argument, nullptr, 't'},
	{"ephemeral", no_argument, nullptr, 'e'},
	{"warmup", required_argument, nullptr, 'w'},
	{"verbose", no_argument, nullptr, 'v'},
	{"allow-all", no_argument, nullptr, ALLOW_ALL},
	{"allow-read", no_argument, nullptr, ALLOW_READ},
	{"allow-write", no_argument, nullptr, ALLOW_WRITE},
	{"allow-env", no_argument, nullptr, ALLOW_ENV},
	{nullptr, 0, nullptr, 0}
};

static void print_usage(const char* program_name)
{
	fprintf(stderr, "Usage: %s [options] [<args>]\n", program_name);
	fprintf(stderr, "Options:\n");
	fprintf(stderr, "  -p, --program <file>     Program\n");
	fprintf(stderr, "  -c, --config <file>      Configuration file\n");
	fprintf(stderr, "  -t, --threads <num>      Number of request VMs (default: 1)\n");
	fprintf(stderr, "  -e, --ephemeral          Use ephemeral VMs\n");
	fprintf(stderr, "  -w, --warmup <num>       Number of warmup requests (default: 0)\n");
	fprintf(stderr, "  -v, --verbose            Enable verbose output\n");
	fprintf(stderr, "  --allow-all              Allow all access\n");
	fprintf(stderr, "  --allow-read             Allow filesystem read access\n");
	fprintf(stderr, "  --allow-write            Allow filesystem write access\n");
	fprintf(stderr, "  --allow-env              Allow environment access\n");
	fprintf(stderr, "A program or configuration file is required in order to be able to host a program.\n");
}

static CommandLineArgs parse_command_line(int argc, char* argv[])
{
	CommandLineArgs args;
	int opt;
	while ((opt = getopt_long(argc, argv, "p:c:t:ew:v", longopts, nullptr)) != -1) {
		switch (opt) {
			case 'p':
				args.filename = optarg;
				break;
			case 'c':
				args.config_file = optarg;
				break;
			case 't':
				args.concurrency = static_cast<int>(std::stoul(optarg));
				break;
			case 'e':
				args.ephemeral = true;
				break;
			case 'w':
				args.warmup_requests = static_cast<uint16_t>(std::stoul(optarg));
				break;
			case 'v':
				args.verbose = true;
				break;
			case ALLOW_ALL:
				args.allow_read = true;
				args.allow_write = true;
				args.allow_env = true;
				break;
			case ALLOW_READ:
				args.allow_read = true;
				break;
			case ALLOW_WRITE:
				args.allow_write = true;
				break;
			case ALLOW_ENV:
				args.allow_env = true;
				break;
			default:
				print_usage(argv[0]);
				exit(EXIT_FAILURE);
		}
	}
	// Remaining arguments are stored in args.remaining_args
	for (int i = optind; i < argc; ++i) {
		args.remaining_args.push_back(argv[i]);
	}
	return args;
}


int main(int argc, char* argv[], char* envp[])
{
	try {
		CommandLineArgs args = parse_command_line(argc, argv);
		// Load the configuration file
		Configuration config = Configuration::FromJsonFile(args.config_file);
		// Anything set on the command-line will override the config file
		if (args.verbose) {
			config.verbose = true;
		}
		if (args.concurrency >= 0) {
			config.concurrency = args.concurrency;
		}
		if (config.concurrency == 0) {
			config.concurrency = std::thread::hardware_concurrency();
		}
		if (args.ephemeral) {
			config.ephemeral = true;
		}
		if (args.warmup_requests > 0) {
			config.warmup_connect_requests = 1;
			config.warmup_intra_connect_requests = args.warmup_requests;
		}
		if (args.filename.empty()) {
			if (config.filename.empty()) {
				fprintf(stderr, "Error: Program filename is required\n");
				print_usage(argv[0]);
				exit(EXIT_FAILURE);
			}
		} else {
			config.filename = args.filename;
		}
		config.filename = lookup_program(config.filename);
		// TODO: Should arguments be appended or replaced?
		for (const auto& arg : args.remaining_args) {
			config.main_arguments.push_back(arg);
		}
		if (args.allow_write) {
			config.allowed_paths.push_back(Configuration::VirtualPath {
				.real_path = "/",
				.virtual_path = "/",
				.writable = true,
				.prefix = true,
			});
			config.allowed_paths.push_back(Configuration::VirtualPath {
				.real_path = config.current_working_directory,
				.virtual_path = ".",
				.writable = true,
				.prefix = true,
			});
		}
		if (args.allow_read) {
			config.allowed_paths.push_back(Configuration::VirtualPath {
				.real_path = "/",
				.virtual_path = "$/",
				.writable = false
			});
			config.allowed_paths.push_back(Configuration::VirtualPath {
				.real_path = config.current_working_directory,
				.virtual_path = ".",
				.writable = false,
				.prefix = true,
			});
		}
		// TODO: avoid duplicates
		if (args.allow_env) {
			for (char** env = envp; *env != nullptr; ++env) {
				config.environ.push_back(*env);
			}
		} else {
			config.environ.insert(config.environ.end(), {
				"LC_TYPE=C", "LC_ALL=C", "USER=root"
			});
		}
		// Print some configuration values
		if (args.verbose) {
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
				printf("Allowed Path: %s -> %s%s\n",
					path.virtual_path.c_str(), path.real_path.c_str(),
					path.prefix ? " (prefix)" : "");
			}
		}

		VirtualMachine::init_kvm();

		// Read the binary file
		const std::vector<uint8_t> binary = file_loader(config.filename);

		// Create a VirtualMachine instance
		VirtualMachine vm(binary, config);
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
			while (true)
			{
				bool failure = false;
				try {
					vm.machine().run();
				} catch (const tinykvm::MachineTimeoutException& me) {
					fprintf(stderr, "*** Main VM timed out\n");
					fprintf(stderr, "Error: %s Data: 0x%#lX\n", me.what(), me.data());
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
			threads.emplace_back([&vm, i, verbose = args.verbose]()
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
				if (getenv("DEBUG_FORK") != nullptr) {
					forked_vm.open_debugger();
				}
				while (true) {
					bool failure = false;
					try {
						forked_vm.resume_fork();
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
					if (vm.is_ephemeral() || failure) {
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
