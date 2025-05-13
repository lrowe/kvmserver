#include "vm.hpp"

#include <chrono>
#include <cstring>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <netinet/in.h>
#include <stdexcept>
#include <thread>
#include <unistd.h>
// The warmup thread hosts a simple HTTP server that is able to
// send minimalistic requests intended to warm up a JIT compiler.
static constexpr int NUM_WARMUP_THREADS = 4;
static std::vector<std::thread> warmup_threads;
static std::atomic<int> warmup_thread_completed = 0;
static bool warmup_thread_stop_please = false;

void VirtualMachine::warmup()
{
	// No need to warm up the JIT compiler if we are not using ephemeral VMs
	if (config().warmup_connect_requests == 0) {
		return;
	}
	printf("Warming up the guest VM (%u requests)...\n", config().warmup_connect_requests);
	this->set_waiting_for_requests(false);
	// Waiting for a certain amount of requests in order
	// to warm up the JIT compiler in the VM
	int freed_fds = 0;
	auto old_listening_fd = this->m_tracked_client_fd;
	this->m_tracked_client_fd = -1;
	machine().fds().free_fd_callback =
	[&](int vfd, tinykvm::FileDescriptors::Entry& entry) -> bool {
		freed_fds++;
		return false; // Nothing happened
	};
	machine().fds().epoll_wait_callback =
	[&](int vfd, int epfd, int timeout) {
		if (freed_fds >= NUM_WARMUP_THREADS * config().warmup_connect_requests) {
			if (config().verbose) {
				fprintf(stderr, "Warmed up the JIT compiler\n");
			}
			// If the listening socket is found, we are now waiting for
			// requests, so we can fork a new VM.
			this->set_waiting_for_requests(true);
			this->machine().stop();
			return false; // Don't call epoll_wait
		}
		return true; // Call epoll_wait
	};

	// Measure the time it takes to warm up the JIT compiler
	auto start = std::chrono::high_resolution_clock::now();

	// Start the warmup client
	this->begin_warmup_client();

	// Run the VM until it stops
	machine().run( config().max_boot_time );
	// Make sure the program is waiting for requests
	if (!this->is_waiting_for_requests()) {
		fprintf(stderr, "The program did not wait for requests after warmup\n");
		throw std::runtime_error("The program did not wait for requests after warmup");
	}
	// Restore the listening socket
	this->m_tracked_client_fd = old_listening_fd;

	// Run one last time to make sure the VM takes into
	// account the remainder of the warmup requests
	machine().run( config().max_boot_time );

	// Stop the warmup client
	this->stop_warmup_client();

	auto end = std::chrono::high_resolution_clock::now();
	auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
	if (config().verbose) {
	}
	fprintf(stderr, "Warmup took %ld ms\n", duration);
}

bool VirtualMachine::connect_and_send_request(const std::string& address, uint16_t port)
{
	static const std::string UNIX_PREFIX = "unix:";
	struct sockaddr_storage serv_addr;
	memset(&serv_addr, 0, sizeof(serv_addr));
	socklen_t serv_addr_len;
	if (address.find(UNIX_PREFIX) == 0) {
		struct sockaddr_un* serv_addr_un = reinterpret_cast<struct sockaddr_un*>(&serv_addr);
		serv_addr_len = sizeof(*serv_addr_un);
		serv_addr_un->sun_family = AF_UNIX;
		// sun_path must be nul terminated.
		if (address.length() - UNIX_PREFIX.size() > sizeof(serv_addr_un->sun_path) - 1) {
			fprintf(stderr, "Warmup: Invalid address (path too long): %s\n", address.c_str());
			return false;
		}
		strncpy(serv_addr_un->sun_path, &address[UNIX_PREFIX.size()], sizeof(serv_addr_un->sun_path) - 1);
	} else {
		struct sockaddr_in* serv_addr_in = reinterpret_cast<struct sockaddr_in*>(&serv_addr);
		serv_addr_len = sizeof(*serv_addr_in);
		serv_addr_in->sin_family = AF_INET;
		serv_addr_in->sin_port = htons(port);
		if (inet_pton(AF_INET, address.c_str(), &serv_addr_in->sin_addr) <= 0) {
			fprintf(stderr, "Warmup: Invalid address: %s\n", address.c_str());
			return false;
		}
	}

	int sockfd = socket(serv_addr.ss_family, SOCK_STREAM, 0);
	if (sockfd < 0) {
		fprintf(stderr, "Warmup: Failed to create socket: %s\n", strerror(errno));
		return false;
	}
	if (connect(sockfd, (struct sockaddr*)&serv_addr, serv_addr_len) < 0) {
		fprintf(stderr, "Warmup: Connection failed: %s\n", strerror(errno));
		close(sockfd);
		return false;
	}

	int intra_connect_requests = config().warmup_intra_connect_requests;
	for (int i = 0; i < intra_connect_requests; ++i)
	{
		std::string request = "GET " + config().warmup_path + " HTTP/1.1\r\nHost: localhost\r\n\r\n";
		if (send(sockfd, request.c_str(), request.size(), MSG_NOSIGNAL) < 0) {
			fprintf(stderr, "Warmup: Failed to send request: %s\n", strerror(errno));
			break;
		}
		char buffer[32768];
		ssize_t bytes = recv(sockfd, buffer, sizeof(buffer), MSG_NOSIGNAL);
		if (bytes < 0) {
			fprintf(stderr, "Warmup: Failed to receive data: %s\n", strerror(errno));
			break;
		} else if (bytes == 0) {
			break; // Connection closed
		}
		while (bytes > 0) {
			bytes = recv(sockfd, buffer, sizeof(buffer), MSG_NOSIGNAL | MSG_DONTWAIT);
			if (bytes < 0) {
				if (errno == EAGAIN || errno == EWOULDBLOCK) {
					break; // No more data
				}
				fprintf(stderr, "Warmup: Failed to receive data: %s\n", strerror(errno));
				break;
			} else if (bytes == 0) {
				break; // Connection closed
			}
		}
	}

	close(sockfd);
	warmup_thread_completed++;
	return true;
}

void VirtualMachine::begin_warmup_client()
{
	if (config().warmup_connect_requests == 0) {
		return;
	}
	if (config().warmup_intra_connect_requests == 0) {
		fprintf(stderr, "Warmup: No intra connect requests, skipping...\n");
		return;
	}
	for (auto& thread : warmup_threads) {
		if (thread.joinable()) {
			thread.join();
		}
	}
	warmup_threads.reserve(NUM_WARMUP_THREADS);
	for (int i = 0; i < NUM_WARMUP_THREADS; ++i) {
		warmup_threads.emplace_back([this]()
		{
			if (config().warmup_address.find("unix:") == 0) {
				printf("Warmup: Starting warmup client at %s, requests %u\n",
					config().warmup_address.c_str(), config().warmup_connect_requests);
			} else {
				printf("Warmup: Starting warmup client at %s:%u, requests %u\n",
					config().warmup_address.c_str(), config().warmup_port, config().warmup_connect_requests);
			}
			// Start a simple HTTP client that will send
			// a request to the VM in order to warm up the guest program.
			for (int i = 0; i < config().warmup_connect_requests; ++i) {
				if (!connect_and_send_request(config().warmup_address, config().warmup_port)) {
					fprintf(stderr, "Warmup: Failed to send request %d\n", i);
					break;
				}
				if (warmup_thread_stop_please) {
					break;
				}
			}
			if (config().verbose) {
				fprintf(stderr, "Warmup: Finished sending requests\n");
			}
		});
	}
}

void VirtualMachine::stop_warmup_client()
{
	warmup_thread_stop_please = true;
	for (auto& thread : warmup_threads) {
		if (thread.joinable()) {
			thread.join();
		}
	}
	if (config().verbose) {
		fprintf(stderr, "Warmup: Stopped warmup server\n");
	}
}
