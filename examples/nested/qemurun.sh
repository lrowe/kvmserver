#!/bin/bash
# https://documentation.ubuntu.com/public-images/public-images-how-to/launch-qcow-with-qemu/
# https://powersj.io/posts/ubuntu-qemu-cli/
set -e
trap 'kill 0' EXIT
mkdir -p target
IMG=ubuntu-24.04-minimal-cloudimg-amd64.img
if [ ! -f "target/$IMG" ]; then
  curl -L -o "target/$IMG" https://cloud-images.ubuntu.com/minimal/releases/noble/release/$IMG
fi
if [ ! -f "target/oha" ]; then
  curl -L -o target/oha https://github.com/hatoo/oha/releases/download/v1.8.0/oha-linux-amd64-pgo
  chmod +x target/oha
fi
if [ ! -f "target/deno" ]; then
  curl -L https://github.com/denoland/deno/releases/download/v2.4.0/deno-x86_64-unknown-linux-gnu.zip | gunzip > target/deno
  chmod +x target/deno
fi
cloud-localds target/seed.img user-data.yaml metadata.yaml
exec qemu-system-x86_64  \
  -machine accel=kvm,type=q35 \
  -cpu host \
  -m 4G \
  -nographic \
  -snapshot \
  -virtfs local,readonly=on,path=../..,mount_tag=myshare,security_model=mapped-xattr \
  -netdev id=net00,type=user,hostfwd=tcp::2222-:22 \
  -device virtio-net-pci,netdev=net00 \
  -drive if=virtio,format=qcow2,file=target/$IMG \
  -drive if=virtio,format=raw,file=target/seed.img
