#include "vm.hpp"

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
static std::thread       warmup_thread;
static bool              warmup_thread_stop_please = false;

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
		if (freed_fds >= config().warmup_connect_requests) {
			fprintf(stderr, "Warmed up the JIT compiler\n");
			// If the listening socket is found, we are now waiting for
			// requests, so we can fork a new VM.
			this->set_waiting_for_requests(true);
			this->machine().stop();
			return false; // Don't call epoll_wait
		}
		return true; // Call epoll_wait
	};

	// Start the warmup client
	this->begin_warmup_client();

	// Run the VM until it stops
	machine().run();
	// Make sure the program is waiting for requests
	if (!this->is_waiting_for_requests()) {
		fprintf(stderr, "The program did not wait for requests after warmup\n");
		throw std::runtime_error("The program did not wait for requests after warmup");
	}
	// Restore the listening socket
	this->m_tracked_client_fd = old_listening_fd;

	// Stop the warmup client
	this->stop_warmup_client();
}

bool VirtualMachine::connect_and_send_request(const std::string& address, uint16_t port)
{
	const char* UNIX_PREFIX = "unix:";
	struct sockaddr_storage serv_addr;
	memset(&serv_addr, 0, sizeof(serv_addr));
	socklen_t serv_addr_len;
	if (address.rfind(UNIX_PREFIX, 0) == 0) {
		struct sockaddr_un* serv_addr_un = reinterpret_cast<struct sockaddr_un*>(&serv_addr);
		serv_addr_len = sizeof(*serv_addr_un);
		serv_addr_un->sun_family = AF_UNIX;
		// sun_path must be nul terminated.
		if (address.length() - strlen(UNIX_PREFIX) > sizeof(serv_addr_un->sun_path) - 1) {
			fprintf(stderr, "Warmup: Invalid address (path too long): %s\n", address.c_str());
			return false;
		}
		strncpy(serv_addr_un->sun_path, address.c_str() + strlen(UNIX_PREFIX), sizeof(serv_addr_un->sun_path) - 1);
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

	// Set a short send/recv timeout as the server will stop responding after
	// a certain amount of requests
	struct timeval timeout;
	timeout.tv_sec = 1;
	timeout.tv_usec = 0;
	if (setsockopt(sockfd, SOL_SOCKET, SO_SNDTIMEO, (const char*)&timeout, sizeof(timeout)) < 0) {
		fprintf(stderr, "Warmup: Failed to set socket options: %s\n", strerror(errno));
		close(sockfd);
		return false;
	}
	if (setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, (const char*)&timeout, sizeof(timeout)) < 0) {
		fprintf(stderr, "Warmup: Failed to set socket options: %s\n", strerror(errno));
		close(sockfd);
		return false;
	}

	int intra_connect_requests = config().warmup_intra_connect_requests;
	for (int i = 0; i < intra_connect_requests; ++i)
	{
		std::string request = "GET " + config().warmup_path + " HTTP/1.1\r\nHost: localhost\r\n\r\n";
		if (send(sockfd, request.c_str(), request.size(), 0) < 0) {
			fprintf(stderr, "Warmup: Failed to send request: %s\n", strerror(errno));
			break;
		}
		char buffer[1024] = {0};
		if (recv(sockfd, buffer, sizeof(buffer), 0) < 0) {
			fprintf(stderr, "Warmup: Failed to receive response: %s\n", strerror(errno));
			break;
		}
	}

	close(sockfd);
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
	if (warmup_thread.joinable()) {
		warmup_thread.join();
	}
	warmup_thread = std::thread([this]()
	{
		printf("Warmup: Starting warmup client address %s:%u, requests %u\n",
			config().warmup_address.c_str(), config().warmup_port, config().warmup_connect_requests);
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

void VirtualMachine::stop_warmup_client()
{
	warmup_thread_stop_please = true;
	if (warmup_thread.joinable()) {
		warmup_thread.join();
	}
	if (config().verbose) {
		fprintf(stderr, "Warmup: Stopped warmup server\n");
	}
}
