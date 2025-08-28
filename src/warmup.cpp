#include "vm.hpp"

#include <atomic>
#include <cstring>
#include <limits.h>
#include <netdb.h>
#include <stdexcept>
#include <sys/socket.h>
#include <thread>
#include <unistd.h>
// The warmup thread hosts a simple HTTP server that is able to
// send minimalistic requests intended to warm up a JIT compiler.
static constexpr int NUM_WARMUP_THREADS = 1;
static std::vector<std::thread> warmup_threads;
static std::atomic<int> warmup_thread_completed = 0;
static bool warmup_thread_stop_please = false;

void VirtualMachine::warmup()
{
	// No need to warm up the JIT compiler if we are not using ephemeral VMs
	if (config().warmup_connect_requests == 0) {
		return;
	}
	this->set_waiting_for_requests(false);
	// Waiting for a certain amount of requests in order
	// to warm up the JIT compiler in the VM
	int freed_sockets = 0;
	std::unordered_set<int> accepted_sockets;
	// Track accepted sockets
	machine().fds().accept_socket_callback =
	[&](int listener_vfd, int listener_fd, int fd, struct sockaddr_storage& addr, socklen_t& addrlen) {
		const int vfd = machine().fds().manage(fd, true, true);
		accepted_sockets.insert(vfd);
		return vfd;
	};
	// Track closed accepted sockets
	machine().fds().free_fd_callback =
	[&](int vfd, tinykvm::FileDescriptors::Entry& entry) -> bool {
		if (accepted_sockets.find(vfd) != accepted_sockets.end()) {
			accepted_sockets.erase(vfd);
			freed_sockets++;
		}
		return false; // Nothing happened
	};
	machine().fds().epoll_wait_callback =
	[&](int vfd, int epfd, int timeout) {
		if (freed_sockets >= NUM_WARMUP_THREADS * config().warmup_connect_requests) {
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
	machine().fds().poll_callback =
	[&](struct pollfd* fds, unsigned nfds, int timeout) {
		if (freed_sockets >= NUM_WARMUP_THREADS * config().warmup_connect_requests) {
			if (config().verbose) {
				fprintf(stderr, "Warmed up the JIT compiler\n");
			}
			// If the listening socket is found, we are now waiting for
			// requests, so we can fork a new VM.
			this->set_waiting_for_requests(true);
			this->machine().stop();
			return false; // Don't call poll
		}
		return true; // Call poll
	};

	// Start the warmup client
	this->begin_warmup_client();

	this->restart_poll_syscall();

	// Run the VM until it stops
	machine().run( config().max_boot_time );
	// Make sure the program is waiting for requests
	if (!this->is_waiting_for_requests()) {
		fprintf(stderr, "The program did not wait for requests after warmup\n");
		throw std::runtime_error("The program did not wait for requests after warmup");
	}

	// Stop the warmup client
	this->stop_warmup_client();
}

bool VirtualMachine::connect_and_send_requests(const sockaddr* serv_addr, socklen_t serv_addr_len)
{
	int sockfd = socket(serv_addr->sa_family, SOCK_STREAM, 0);
	if (sockfd < 0) {
		fprintf(stderr, "Warmup: Failed to create socket: %s\n", strerror(errno));
		return false;
	}
	if (connect(sockfd, serv_addr, serv_addr_len) < 0) {
		fprintf(stderr, "Warmup: Connection failed: %s\n", strerror(errno));
		close(sockfd);
		return false;
	}

	int intra_connect_requests = config().warmup_intra_connect_requests;
	char buffer[32768];
	ssize_t bytes = 0;
	for (int i = 0; i < intra_connect_requests; ++i)
	{
		std::string request = "GET " + config().warmup_path + " HTTP/1.1\r\n"
			+ "Host: localhost\r\n"
			+ (intra_connect_requests == i + 1 ? "Connection: close\r\n" : "")
			+ "\r\n";
		if (send(sockfd, request.c_str(), request.size(), MSG_NOSIGNAL) < 0) {
			fprintf(stderr, "Warmup: Failed to send request: %s\n", strerror(errno));
			break;
		}
		bytes = recv(sockfd, buffer, sizeof(buffer), MSG_NOSIGNAL);
		if (bytes < 0) {
			fprintf(stderr, "Warmup: Failed to receive data: %s\n", strerror(errno));
			break;
		} else if (bytes == 0) {
			break; // Connection closed
		}
	}
	// Read until connection close
	while (bytes > 0) {
		bytes = recv(sockfd, buffer, sizeof(buffer), MSG_NOSIGNAL);
		if (bytes < 0) {
			fprintf(stderr, "Warmup: Failed to receive data: %s\n", strerror(errno));
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
	struct sockaddr_storage serv_addr {};
	socklen_t serv_addr_len = sizeof(serv_addr);
	if (getsockname(this->m_tracked_client_fd, (struct sockaddr*)&serv_addr, &serv_addr_len) < 0) {
		fprintf(stderr, "Warmup: Failed getsockname: %s\n", strerror(errno));
		return;
	}
	std::string host(HOST_NAME_MAX, 0);
	std::string serv(PATH_MAX, 0);
	if (
		getnameinfo((struct sockaddr*)&serv_addr, serv_addr_len, host.data(), host.size(),
			serv.data(), serv.size(), NI_NUMERICHOST | NI_NUMERICSERV
		) < 0
	) {
		fprintf(stderr, "Warmup: Failed getnameinfo: %s\n", strerror(errno));
		return;
	}
	printf("Warming up the guest VM listening on %s:%s (%d threads * %u connections * %u requests)\n",
		host.c_str(), serv.c_str(), NUM_WARMUP_THREADS,
		config().warmup_connect_requests, config().warmup_intra_connect_requests);
	for (auto& thread : warmup_threads) {
		if (thread.joinable()) {
			thread.join();
		}
	}
	warmup_threads.reserve(NUM_WARMUP_THREADS);
	for (int t = 0; t < NUM_WARMUP_THREADS; ++t) {
		warmup_threads.emplace_back([this, t, serv_addr, serv_addr_len]()
		{
			if (config().verbose) {
				fprintf(stderr, "Warmup: Starting warmup client %d\n", t);
			}
			// Start a simple HTTP client that will send
			// a request to the VM in order to warm up the guest program.
			for (int c = 0; c < config().warmup_connect_requests; ++c) {
				if (!connect_and_send_requests((struct sockaddr*)&serv_addr, serv_addr_len)) {
					fprintf(stderr, "Warmup: Failure on connection %d\n", c);
					break;
				}
				if (warmup_thread_stop_please) {
					break;
				}
			}
			if (config().verbose) {
				fprintf(stderr, "Warmup: Finished sending requests on warmup client %d\n", t);
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
