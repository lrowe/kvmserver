# Running the KVM Server benchmark under nested virtualization

TODO: Work out how to run under podman machine.

## Setup

> [!WARNING]
> The script is written under the assumption your UID is 1000 (i.e. the first
> created user account on Debian derived distros) and you have installed `uv`
> and `nvm` in their default locations and installed `node` with `nvm`.

- Install required packages:

```sh
apt-get install cloud-localds qemu-system-x86
```

- Download dependencies and build the VM image:

```sh
make build
```

## Run the VM ephemerally and log in

It has read-only access to the filesystem mounts with overlayfs to allow local
execution.

```sh
make run
```

> [!CAUTION]
> All state from the VM will be lost at shutdown.

- Login as ubuntu user (password ubuntu) at terminal or ssh into
  `ssh -p 2222 ubuntu@127.0.0.1`.

> [!TIP]
> To exit use `ctrl-a c` to escape to the qemu monitor and `quit`.

## Accessing your local checkout under /mnt

Your repo checkout directory is mapped to /mnt with overlayfs so any changes
made in the VM are not persisted.

> [!NOTE]
> Make sure you have run `make build` outside the VM in the repo root and
> examples directory as the VM lacks tools to build all examples.

- Run `make build` to fix missing dependencies on the VM.

```sh
cd /mnt/examples
make build
```

- Run the tests and benchmarks:

```sh
cd /mnt/examples
make test
make bench
```

## Run docker containers from GitHub Actions runs

GitHub Actions runs the tests in docker containers under nested virtualisation.
We can recreate that to an extent in the VM:

```sh
export IMAGE=ghcr.io/libriscv/kvmserver
export BIN_TAG=bin-run.123.1
export TEST_TAG=test-run.123.1
docker pull $IMAGE:$BIN_TAG
docker pull $IMAGE:$TEST_TAG
docker run --rm --device /dev/kvm --group-add $(stat -c %g /dev/kvm) --mount type=image,dst=/mnt,src=$IMAGE:$BIN_TAG $IMAGE:$TEST_TAG make test
```
