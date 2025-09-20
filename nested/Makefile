.PHONY: bindir build clean qemu-docker qemu

build: bindir target/overlay.qcow2

clean:
	rm -rf target

IMG := ubuntu-24.04-minimal-cloudimg-amd64.img

target/$(IMG):
	mkdir -p target
	curl -fsSLo target/$(IMG) https://cloud-images.ubuntu.com/minimal/releases/noble/release/$(IMG)

target/seed.img: user-data.yaml
	mkdir -p target
	cloud-localds target/seed.img user-data.yaml

target/overlay.qcow2: target/seed.img target/$(IMG)
	qemu-img create -f qcow2 -F qcow2 -b $(IMG) target/overlay.qcow2 16G
	qemu-system-x86_64 \
		-machine accel=kvm,type=q35 \
		-cpu host \
		-m 4G \
		-nographic \
		-netdev id=net00,type=user \
		-device virtio-net-pci,netdev=net00 \
		-drive if=virtio,format=qcow2,file=target/overlay.qcow2 \
		-drive if=virtio,format=raw,file=target/seed.img

run: bindir target/overlay.qcow2
	exec qemu-system-x86_64 \
		-machine accel=kvm,type=q35 \
		-cpu host \
		-m 4G \
		-nographic \
		-snapshot \
		-virtfs local,readonly=on,path=..,mount_tag=mnt,security_model=none \
		-virtfs local,readonly=on,path=target/bin,mount_tag=bin,security_model=none \
		-virtfs local,readonly=on,path=$$NVM_DIR,mount_tag=nvm,security_model=none \
		-virtfs local,readonly=on,path=$$HOME/.local,mount_tag=local,security_model=none \
		-netdev id=net00,type=user,hostfwd=tcp::2222-:22 \
		-device virtio-net-pci,netdev=net00 \
		-drive if=virtio,format=qcow2,file=target/overlay.qcow2 \
		-drive if=virtio,format=raw,file=target/seed.img

bindir: target/bin/bun target/bin/deno target/bin/lune target/bin/oha target/bin/wasmtime target/bin/wasmtime-noreuse target/bin/wasmtime-reuse target/bin/uv target/bin/uvx

target/bin/bun:
	mkdir -p target/bin
	curl -fsSLo target/bun-linux-x64.zip https://github.com/oven-sh/bun/releases/download/bun-v1.2.21/bun-linux-x64.zip
	unzip -jo target/bun-linux-x64.zip -d target/bin/
	rm target/bun-linux-x64.zip

target/bin/deno:
	mkdir -p target/bin
	curl -fsSL https://github.com/denoland/deno/releases/download/v2.5.0/deno-x86_64-unknown-linux-gnu.zip \
		| gunzip -c > target/bin/deno
	chmod +x target/bin/deno

target/bin/lune:
	mkdir -p target/bin
	curl -fsSL https://github.com/lune-org/lune/releases/download/v0.10.1/lune-0.10.1-linux-x86_64.zip \
		| gunzip -c > target/bin/lune
	chmod +x target/bin/lune

target/bin/oha:
	mkdir -p target/bin
	curl -fsSLo target/bin/oha https://github.com/hatoo/oha/releases/download/v1.9.0/oha-linux-amd64-pgo
	chmod +x target/bin/oha

target/bin/wasmtime:
	mkdir -p target/bin
	curl -fsSL https://github.com/bytecodealliance/wasmtime/releases/download/v36.0.2/wasmtime-v36.0.2-x86_64-linux.tar.xz \
		| tar -xJC target/bin/ --strip-components=1 --wildcards "*/wasmtime"

target/bin/wasmtime-noreuse:
	mkdir -p target/bin
	curl -fsSLo target/bin/wasmtime-noreuse https://github.com/lrowe/wasmtime/releases/download/v31.0.0-instance-reuse-e985d36/wasmtime-noreuse
	chmod +x target/bin/wasmtime-noreuse

target/bin/wasmtime-reuse:
	mkdir -p target/bin
	curl -fsSLo target/bin/wasmtime-reuse https://github.com/lrowe/wasmtime/releases/download/v31.0.0-instance-reuse-e985d36/wasmtime-reuse
	chmod +x target/bin/wasmtime-reuse

target/bin/uv target/bin/uvx:
	mkdir -p target/bin
	curl -fsSL https://github.com/astral-sh/uv/releases/download/0.8.13/uv-x86_64-unknown-linux-gnu.tar.gz \
		| tar -xzC target/bin/ --strip-components=1
