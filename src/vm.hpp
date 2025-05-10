#pragma once
#include <tinykvm/machine.hpp>
#include "config.hpp"

struct VirtualMachine
{
	using gaddr_t = uint64_t;
	using machine_t = tinykvm::Machine;
	using on_reset_t = std::function<void()>;
	enum class BinaryType : uint8_t {
		Static,
		StaticPie,
		Dynamic,
	};

	void wait_for_requests_paused();
	bool is_waiting_for_requests() const noexcept { return m_waiting_for_requests; }
	void set_waiting_for_requests(bool waiting) noexcept { m_waiting_for_requests = waiting; }

	auto& machine() { return m_machine; }
	const auto& machine() const { return m_machine; }

	const auto& name() const noexcept { return m_config.filename; }
	const auto& config() const { return m_config; }
	BinaryType binary_type() const noexcept { return m_binary_type; }
	std::string binary_type_string() const noexcept;
	void set_on_reset_callback(on_reset_t callback) noexcept { m_on_reset_callback = std::move(callback); }
	void set_ephemeral(bool ephemeral) noexcept { m_ephemeral = ephemeral; }

	void open_debugger();

	VirtualMachine(const std::vector<uint8_t>& binary, const Configuration& config);
	VirtualMachine(const VirtualMachine& other, unsigned reqid);
	~VirtualMachine();
	void initialize(std::function<void()> = nullptr);
	void reset_to(const VirtualMachine&);
	static void init_kvm();

private:
	tinykvm::Machine m_machine;
	const Configuration& m_config;
	const std::vector<uint8_t>& m_original_binary;
	BinaryType m_binary_type = BinaryType::Static;
	bool m_ephemeral = false;
	bool m_reset_needed = false;
	bool m_waiting_for_requests = false;
	// The tracked client fd for ephemeral VMs
	int m_tracked_client_fd = -1;
	on_reset_t m_on_reset_callback = nullptr;
	const VirtualMachine* m_master_instance = nullptr;
};
