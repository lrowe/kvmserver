#!/bin/bash
# https://documentation.ubuntu.com/public-images/public-images-how-to/launch-qcow-with-qemu/
# https://powersj.io/posts/ubuntu-qemu-cli/
# Reuses your local binary so you must use an image with a glibc of at least that.
set -e
trap 'kill 0' EXIT
mkdir -p target
IMG=ubuntu-24.04-minimal-cloudimg-amd64.img
if [ ! -f "target/$IMG" ]; then
  curl -L -o "target/$IMG" https://cloud-images.ubuntu.com/minimal/releases/noble/release/$IMG
fi

OVERLAY=target/overlay-docker.qcow2
if [ ! -f "$OVERLAY" ]; then
  qemu-img create -f qcow2 -F qcow2 -b $IMG $OVERLAY 16G
  cloud-localds target/seed-docker.img user-data-docker.yaml
  qemu-system-x86_64  \
    -machine accel=kvm,type=q35 \
    -cpu host \
    -m 4G \
    -nographic \
    -netdev id=net00,type=user \
    -device virtio-net-pci,netdev=net00 \
    -drive if=virtio,format=qcow2,file="$OVERLAY" \
    -drive if=virtio,format=raw,file=target/seed-docker.img
fi

escaped=()
for item in "$@"; do
  escaped+=( "$(printf %q "$item")" )
done
cat > target/command.sh << EOF
#!/bin/bash
${escaped[@]}
EOF
cloud-init devel make-mime -a target/command.sh:x-shellscript -a user-data-docker.yaml:cloud-config > target/seed-docker.mime
cloud-localds target/seed-docker.img target/seed-docker.mime

exec qemu-system-x86_64  \
  -machine accel=kvm,type=q35 \
  -cpu host \
  -m 4G \
  -nographic \
  -snapshot \
  -netdev id=net00,type=user,hostfwd=tcp::2222-:22 \
  -device virtio-net-pci,netdev=net00 \
  -drive if=virtio,format=qcow2,file="$OVERLAY" \
  -drive if=virtio,format=raw,file=target/seed-docker.img
