# Running the KVM Server benchmark under nested virtualization

TODO: Work out how to run under podman/docker:

- Run `make build` in repo root and examples directory.

- Install required packages:

```sh
apt-get install cloud-localds qemu-system-x86
```

- Run benchmark under qemu (script will download required images and binaries):

```sh
./qemurun.sh
```
