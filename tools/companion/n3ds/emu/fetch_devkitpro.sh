#!/bin/bash
# Fetches devkitARM + libctru + citro2d/3d + 3dsx/smdh tools WITHOUT root and without pkg.devkitpro.org (which answers Cloudflare 403 to curl):
# the layers of the official Docker Hub image devkitpro/devkitarm are downloaded with curl, each blob is checked against its sha256 digest,
# every layer is extracted into its own empty directory (only opt/devkitpro is kept) and merged in image order.  No docker daemon is needed.
#
#   fetch_devkitpro.sh <dest-dir> [tag]        then:  export DEVKITPRO=<dest-dir>/devkitpro
#
# Provenance: the files come from Docker Hub, not from a signed pacman repository.  The blobs are untrusted until the digests match.
set -euo pipefail
DEST=${1:?usage: fetch_devkitpro.sh <dest-dir> [tag]}
TAG=${2:-latest}
REPO=devkitpro/devkitarm
[ -e "$DEST" ] && [ -n "$(ls -A "$DEST" 2>/dev/null)" ] && { echo "$DEST is not empty"; exit 1; }
mkdir -p "$DEST/dl" "$DEST/devkitpro"
jget() { python3 -I -c "$1"; }

TOKEN=$(curl -sf "https://auth.docker.io/token?service=registry.docker.io&scope=repository:$REPO:pull" | jget 'import json,sys;print(json.load(sys.stdin)["token"])')
reg() { curl -sfL -H "Authorization: Bearer $TOKEN" -H "Accept: $2" "https://registry-1.docker.io/v2/$REPO/$1"; }
ACCEPT_IDX="application/vnd.oci.image.index.v1+json,application/vnd.docker.distribution.manifest.list.v2+json"
ACCEPT_MAN="application/vnd.oci.image.manifest.v1+json,application/vnd.docker.distribution.manifest.v2+json"

AMD64=$(reg "manifests/$TAG" "$ACCEPT_IDX" | jget '
import json,sys
d=json.load(sys.stdin)
for m in d["manifests"]:
    p=m.get("platform",{})
    if p.get("os")=="linux" and p.get("architecture")=="amd64": print(m["digest"]); break')
reg "manifests/$AMD64" "$ACCEPT_MAN" > "$DEST/dl/manifest.json"
python3 -I -c 'import json,sys
for l in json.load(open(sys.argv[1]))["layers"]: print(l["digest"])' "$DEST/dl/manifest.json" > "$DEST/dl/layers.txt"

n=0
while read -r digest; do
  n=$((n + 1))
  blob="$DEST/dl/layer$n.tgz"
  curl -sfL -H "Authorization: Bearer $TOKEN" -o "$blob" "https://registry-1.docker.io/v2/$REPO/blobs/$digest"
  got=$(sha256sum "$blob" | cut -d' ' -f1)
  [ "sha256:$got" = "$digest" ] || { echo "digest mismatch for layer $n"; exit 1; }
  mkdir "$DEST/dl/layer$n.d"
  tar -xzf "$blob" -C "$DEST/dl/layer$n.d" --no-same-owner --no-same-permissions --exclude='.wh.*' opt/devkitpro 2>/dev/null || true   # image whiteout markers are not files
  [ -d "$DEST/dl/layer$n.d/opt/devkitpro" ] && chmod -R u+rwX "$DEST/dl/layer$n.d/opt/devkitpro" && cp -a "$DEST/dl/layer$n.d/opt/devkitpro/." "$DEST/devkitpro/"
  echo "layer $n ok ($digest)"
done < "$DEST/dl/layers.txt"
"$DEST/devkitpro/devkitARM/bin/arm-none-eabi-gcc" --version | head -1
echo "export DEVKITPRO=$DEST/devkitpro"
