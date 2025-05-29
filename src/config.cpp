#include "config.hpp"
#include <CLI/CLI.hpp>
#include <fstream>
#include <limits.h>
#include <nlohmann/json.hpp>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netdb.h>
#include <linux/un.h>
#include <thread>
#include <unistd.h>
#include <ranges>

template <typename T>
void add_remappings(const nlohmann::json& json, std::vector<T>& remappings,
	bool allow_read, bool allow_write, bool allow_execute)
{
	for (const auto& remap : json) {
		tinykvm::VirtualRemapping remapping;
		// Physical is usually 0x0, which means allocate from guest heap
		remapping.phys = 0x0;
		if (remap.is_array()) {
			// Remapping is an array of "0xADDRESS" and size pairs
			// Eg. ["0x100000000": 64] indicating a 64mb remapping
			std::string address = remap.at(0).get<std::string>();
			if (address.find("0x") != std::string::npos) {
				remapping.virt = std::stoull(address, nullptr, 16);
			} else {
				remapping.virt = std::stoull(address);
			}
			// The size is the second element in the array
			remapping.size = remap.at(1).get<uint64_t>() * (1UL << 20); // Convert MB to bytes
			// Set the permissions
			remapping.writable = allow_write;
			remapping.executable = allow_execute;
			remappings.push_back(remapping);
		}
		else if (remap.is_object())
		{
			// Remapping is an object with "virtual", "size", "executable", "writable" etc.
			// There are always virtual and size fields
			remapping.virt = remap.at("virtual").get<uint64_t>();
			remapping.size = remap.at("size").get<uint64_t>() * (1UL << 20); // Convert MB to bytes
			if (remap.contains("physical")) {
				// This should be *very* rare
				remapping.phys = remap["physical"].get<uint64_t>();
			}
			// In this case we get the permissions explicitly from the JSON
			remapping.executable = remap.value("executable", false);
			remapping.writable = remap.value("writable", false);
			remappings.push_back(remapping);
		}
		else {
			// Print the whole object for debugging
			fprintf(stderr, "Invalid remapping format in configuration:\n");
			fprintf(stderr, "%s\n", remap.dump(4).c_str());
			throw std::runtime_error("Invalid remapping format in configuration file");
		}
	}
}


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
		throw CLI::ValidationError("program: Not found on path", program);
	}
	fs::path filepath = fs::absolute(program).lexically_normal();
	if (access(filepath.c_str(), X_OK) == 0) {
		return filepath;
	}
	throw CLI::ValidationError("program: Not an executable", program);
}


// Return value indicates whether to break out of loop
static bool parse_addresses(
	const std::vector<std::string>& config,
	std::vector<struct sockaddr_storage>& allowed_ipv4,
	std::vector<struct sockaddr_storage>& allowed_ipv6,
	const bool verbose
) {
	for (const auto& value : config) {
		if (value.empty() || value == "false") {
			continue;
		}
		uint port = 0;
		std::string address(value);
		if (value == "true") {
			address = "";
		}
		size_t maybe_colon = address.find_last_not_of("0123456789");
		if (maybe_colon != std::string::npos && value[maybe_colon] == ':') {
			uint parsed_port;
			try {
				port = std::stoul(address.substr(maybe_colon + 1, std::string::npos));
			} catch (...) {
				throw CLI::ValidationError("Invalid port", value);
			}
			address = address.substr(0, maybe_colon);
		}

		if (port > std::numeric_limits<in_port_t>::max()) {
			throw CLI::ValidationError("Invalid port", value);
		}

		if (address == "") {
			if (port == 0) {
				allowed_ipv4.clear();
				allowed_ipv6.clear();
			}
			auto& storage = allowed_ipv4.emplace_back((struct sockaddr_storage) {});
			storage.ss_family = AF_INET;
			reinterpret_cast<sockaddr_in*>(&storage)->sin_port = htons(port);
			storage = allowed_ipv6.emplace_back((struct sockaddr_storage) {});
			storage.ss_family = AF_INET6;
			reinterpret_cast<sockaddr_in6*>(&storage)->sin6_port = htons(port);
			if (port == 0) {
				return true;
			};
			continue;
		}

		// IPv6
		if (address.front() == '[') {
			if (address.back() != ']') {
				throw CLI::ValidationError("Invalid ipv6 address", value);
			}
			std::string address(address.substr(1, value.length() - 2));
			auto& storage = allowed_ipv6.emplace_back((struct sockaddr_storage) {});
			sockaddr_in6* addr = reinterpret_cast<sockaddr_in6*>(&storage);
			addr->sin6_family = AF_INET6;
			if (inet_pton(AF_INET6, address.c_str(), &addr->sin6_addr) <= 0) {
				throw CLI::ValidationError("Invalid IPv6 address", value);
			}
			addr->sin6_port = htons(port);
			return false;
		}

		// IPv4
		in_addr sin_addr;
		if (inet_pton(AF_INET, address.c_str(), &sin_addr) > 0) {
			auto& storage = allowed_ipv4.emplace_back((struct sockaddr_storage) {});
			sockaddr_in* addr = reinterpret_cast<sockaddr_in*>(&storage);
			addr->sin_family = AF_INET;
			addr->sin_addr = sin_addr;
			addr->sin_port = htons(port);
			continue;
		}

		// Resolve the domain name to an IP address
		struct addrinfo hints = {};
		struct addrinfo* head;
		hints.ai_family = AF_UNSPEC; // IPv4 or IPv6
		hints.ai_socktype = SOCK_STREAM; // TCP
		if (getaddrinfo(address.c_str(), nullptr, &hints, &head) != 0) {
			throw CLI::ValidationError("Invalid domain name", value);
		}
		for (struct addrinfo* res = head; res != nullptr; res = res->ai_next) {
			if (res->ai_family == AF_INET) {
				auto& storage = allowed_ipv4.emplace_back((struct sockaddr_storage) {});
				sockaddr_in* addr = reinterpret_cast<sockaddr_in*>(&storage);
				addr->sin_family = AF_INET;
				addr->sin_addr = reinterpret_cast<sockaddr_in*>(res->ai_addr)->sin_addr;
				addr->sin_port = htons(port);
				if (verbose) {
					std::string found;
					found.reserve(64);
					inet_ntop(AF_INET, reinterpret_cast<void*>(&addr->sin_addr), found.data(), found.capacity());
					printf("Resolved %s to %s\n", address.c_str(), found.c_str());
				}
			} else if (res->ai_family == AF_INET6) {
				auto& storage = allowed_ipv6.emplace_back((struct sockaddr_storage) {});
				sockaddr_in6* addr = reinterpret_cast<sockaddr_in6*>(&storage);
				addr->sin6_family = AF_INET6;
				addr->sin6_addr = reinterpret_cast<sockaddr_in6*>(res->ai_addr)->sin6_addr;
				addr->sin6_port = htons(port);
				if (verbose) {
					std::string found;
					found.reserve(64);
					inet_ntop(AF_INET6, reinterpret_cast<void*>(&addr->sin6_addr), found.data(), found.capacity());
					printf("Resolved %s to %s\n", address.c_str(), found.c_str());
				}
			} else {
				throw CLI::ValidationError("Invalid address family for domain", value);
			}
		}
		freeaddrinfo(head);
	}
	return false;
}

Configuration Configuration::FromArgs(int argc, char* argv[])
{
	Configuration config;
	CLI::App app{"kvmserver"};

	bool allow_read = false;
	bool allow_write = false;
	std::vector<std::string> allow_env;
	std::vector<std::string> allow_net;
	std::vector<std::string> allow_connect;
	std::vector<std::string> allow_listen;

	app.set_config("-c,--config", "kvmserver.toml", "Read a toml file");

	app.add_option("program", config.filename, "Program")->required();
	app.add_option("args", config.main_arguments, "Program arguments");
	app.add_option("--cwd", config.current_working_directory, "Set the guests working directory");
	// TODO: This does not allow env=[] in config file.
	app.add_option("--env", config.environ, "add an environment variable")->allow_extra_args(false);

	app.add_option("-t,--threads", config.concurrency, "Number of request VMs (0 to use cpu count)")->capture_default_str();
	app.add_flag("-e,--ephemeral", config.ephemeral, "Use ephemeral VMs");
	app.add_option("-w,--warmup", config.warmup_connect_requests, "Number of warmup requests")->capture_default_str();
	// -vv include syscalls, -vvv also include memory maps
	app.add_flag("-v,--verbose", [&](uint verbose) {
		if (verbose >= 1) {
			config.verbose = true;
		}
		if (verbose >= 2) {
			config.verbose_syscalls = true;
		}
		if (verbose >= 3) {
			config.verbose_pagetable = true;
		}
	}, "Enable verbose output");
	app.add_flag("--allow-all", [&](bool allow_all) {
		if (allow_all) {
			allow_read = true;
			allow_write = true;
			allow_env.emplace_back("*");
			allow_net.emplace_back("true");
		}
	}, "Allow all access")->group("Permissions");
	app.add_flag("--allow-read", allow_read, "Allow filesystem read access")->excludes("--allow-all")->group("Permissions");
	app.add_flag("--allow-write", allow_write, "Allow filesystem write access")->excludes("--allow-all")->group("Permissions");
	app.add_flag("--allow-env{*}", allow_env, "Allow access to environment variables. Optionally specify accessible environment variables (e.g. --allow-env=USER,PATH,API_*).")->delimiter(',')->excludes("--allow-all")->group("Permissions");
	app.add_flag("--allow-net", allow_net, "Allow network access")->delimiter(',')->excludes("--allow-all")->group("Permissions");
	app.add_flag("--allow-connect", allow_connect, "Allow outgoing network access")->delimiter(',')->excludes("--allow-all")->group("Permissions");
	app.add_flag("--allow-listen", allow_listen, "Allow incoming network access")->delimiter(',')->excludes("--allow-all")->group("Permissions");

	app.add_option("--max-boot-time", config.max_boot_time)->capture_default_str()->group("Advanced");
	app.add_option("--max-request-time", config.max_req_time)->capture_default_str()->group("Advanced");
	app.add_option("--max-main-memory", config.max_main_memory)->capture_default_str()->group("Advanced");
	app.add_option("--max-address-space", config.max_address_space)->capture_default_str()->group("Advanced");
	app.add_option("--max-request-memory", config.max_req_mem)->capture_default_str()->group("Advanced");
	app.add_option("--limit-request-memory", config.limit_req_mem)->capture_default_str()->group("Advanced");
	app.add_option("--shared-memory", config.shared_memory)->capture_default_str()->group("Advanced");
	app.add_option("--dylink-address-hint", config.dylink_address_hint)->capture_default_str()->group("Advanced");
	app.add_option("--heap-address-hint", config.heap_address_hint)->capture_default_str()->group("Advanced");
	app.add_option("--hugepage-arena-size", config.hugepage_arena_size)->capture_default_str()->group("Advanced");
	app.add_option("--hugepage-requests-arena", config.hugepage_requests_arena)->capture_default_str()->group("Advanced");
	app.add_flag("!--no-executable-heap", config.executable_heap)->capture_default_str()->group("Advanced");
	app.add_flag("--hugepages", config.hugepages)->group("Advanced");
	app.add_flag("!--no-split-hugepages", config.split_hugepages)->group("Advanced");
	app.add_flag("--transparent-hugepages", config.transparent_hugepages)->group("Advanced");
	app.add_flag("!--no-relocate-fixed-mmap", config.relocate_fixed_mmap)->group("Advanced");
	app.add_flag("!--no-ephemeral-keep-working-memory", config.ephemeral_keep_working_memory)->group("Advanced");

	CLI::Option* print_config = app.add_flag("--print-config", "Print config and exit without running program")->configurable(false);

	app.callback([&]() {
		config.filename = lookup_program(config.filename);

		if (config.concurrency == 0) {
			config.concurrency = std::thread::hardware_concurrency();
		}
		if (config.current_working_directory.empty()) {
			char* cwd = get_current_dir_name();
			config.current_working_directory = cwd;
			free(cwd);
		}

		if (allow_write) {
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
		else if (allow_read) {
			config.allowed_paths.push_back(Configuration::VirtualPath {
				.real_path = "/",
				.virtual_path = "/",
				.writable = false,
				.prefix = true,
			});
			config.allowed_paths.push_back(Configuration::VirtualPath {
				.real_path = config.current_working_directory,
				.virtual_path = ".",
				.writable = false,
				.prefix = true,
			});
		}
		extern char **environ;
		for (const auto& name : allow_env) {
			// XXX ensure name has no = using validator
			if (name.back() == '*') {
				for (char** env = environ; *env != nullptr; ++env) {
					if (strncmp(*env, name.data(), name.size() - 1) == 0) {
						config.environ.push_back(*env);
					}
				}
			} else {
				std::string namestring(name.begin(), name.end());
				config.environ.push_back(namestring + "=" + getenv(namestring.c_str()));
			}
		}
		// Do we need "LC_TYPE=C", "LC_ALL=C"?
		if (
			std::find_if(config.environ.begin(), config.environ.end(),
				[](auto& value) { return value.starts_with("USER="); }
			) == config.environ.end()
		) {
			config.environ.emplace_back("USER=nobody");
		}

		bool skip_allow_connect_listen = parse_addresses(
			allow_net,
			config.allowed_connect_ipv4,
			config.allowed_connect_ipv6,
			config.verbose
		);
		config.allowed_listen_ipv4 = config.allowed_connect_ipv4;
		config.allowed_listen_ipv6 = config.allowed_connect_ipv6;
		if (!skip_allow_connect_listen) {
			parse_addresses(
				allow_connect,
				config.allowed_connect_ipv4,
				config.allowed_connect_ipv6,
				config.verbose
			);
			parse_addresses(
				allow_listen,
				config.allowed_listen_ipv4,
				config.allowed_listen_ipv6,
				config.verbose
			);
		}

		// The address space must at least be as large as the main memory
		config.max_address_space = std::max(config.max_address_space, config.max_main_memory);

		// Raise the memory sizes into megabytes
		config.max_address_space = config.max_address_space * (1ULL << 20);
		config.max_main_memory = config.max_main_memory * (1ULL << 20);
		config.max_req_mem = config.max_req_mem * (1UL << 20);
		config.limit_req_mem = config.limit_req_mem * (1UL << 20);
		config.shared_memory = config.shared_memory * (1UL << 20);
		config.dylink_address_hint = config.dylink_address_hint * (1UL << 20);
		config.heap_address_hint = config.heap_address_hint * (1UL << 20);
	});

	try {
		app.parse(argc, argv);
	} catch (const CLI::ParseError &e) {
    std::exit(app.exit(e));
	}

	if (*print_config) {
		std::cout<<app.config_to_str(false, true);
		std::exit(0);
	}

	return config;
}
