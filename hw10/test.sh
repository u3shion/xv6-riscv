IMG_NAME="ext2.img"
IMG_SIZE="256M"
MOUNT_POINT="ext2"
BLOCK_SIZE="2048"
USE_VALGRIND="${1:-}"

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m'

if [[ "$OSTYPE" == "darwin"* ]]; then
    IS_MACOS=1
else
    IS_MACOS=0
fi

log_info() {
    echo -e "${GREEN}[INFO]${NC} $*"
}

log_error() {
    echo -e "${RED}[ERROR]${NC} $*"
}

log_warn() {
    echo -e "${YELLOW}[WARN]${NC} $*"
}

cleanup() {
    log_info "Cleaning up..."
    
    if mountpoint -q "$MOUNT_POINT" 2>/dev/null; then
        log_warn "Unmounting $MOUNT_POINT..."
        sudo umount "$MOUNT_POINT" || true
    fi
    
    if [ -n "$LOOP_DEV" ] && [ -b "$LOOP_DEV" ]; then
        log_warn "Detaching loop device $LOOP_DEV..."
        sudo losetup -d "$LOOP_DEV" || true
    fi
    
    if [ -d "$MOUNT_POINT" ]; then
        rmdir "$MOUNT_POINT" 2>/dev/null || true
    fi
}

trap cleanup EXIT

run_tool() {
    local tool=$1
    shift
    
    if [ "$USE_VALGRIND" = "valgrind" ]; then
        valgrind --leak-check=full --show-leak-kinds=all "./$tool" "$@"
    else
        "./$tool" "$@"
    fi
}

log_info "=== EXT2 Filesystem Testing ==="

log_info "Checking requirements..."

if [ $IS_MACOS -eq 1 ]; then
    log_error "This test requires Linux with ext2 support."
    log_error ""
    log_error "To run tests on Linux:"
    log_error "  1. Copy this directory to a Linux machine"
    log_error "  2. Run: make test"
    log_error ""
    log_error "Test script will demonstrate utilities work with pre-existing ext2.img"
    exit 1
fi

if ! command -v mkfs.ext2 &> /dev/null; then
    log_error "Required tool not found: mkfs.ext2"
    exit 1
fi
if ! command -v mount &> /dev/null; then
    log_error "Required tool not found: mount"
    exit 1
fi
if ! command -v truncate &> /dev/null; then
    log_error "Required tool not found: truncate"
    exit 1
fi
if ! command -v sha512sum &> /dev/null; then
    log_error "Required tool not found: sha512sum"
    exit 1
fi
if ! command -v debugfs &> /dev/null; then
    log_error "Required tool not found: debugfs"
    exit 1
fi

ext2_inode_for_path() {
    local path="$1"
    local dbg_path="/${path#/}"
    local inode parent part

    inode=$(debugfs -R "which ${dbg_path}" "$IMG_NAME" 2>/dev/null | awk '/^Inode/{print $2; exit}')
    if [ -n "$inode" ]; then
        echo "$inode"
        return
    fi

    parent=2
    IFS='/' read -ra parts <<< "$path"
    for part in "${parts[@]}"; do
        if [ -z "$part" ]; then
            continue
        fi
        inode=$(./getdirinfo "$IMG_NAME" "$parent" 2>/dev/null | awk -v n="$part" '
            {
                name = $2
                gsub(/[[:space:]]+$/, "", name)
                if (name == n) { print $1; exit }
            }')
        if [ -z "$inode" ]; then
            return
        fi
        parent=$inode
    done
    echo "$parent"
}

log_info "✓ All requirements met"

log_info "Step 1: Creating empty filesystem image ($IMG_SIZE)..."
if [ -f "$IMG_NAME" ]; then
    rm "$IMG_NAME"
fi
truncate --size "$IMG_SIZE" "$IMG_NAME"
log_info "✓ Created $IMG_NAME"

log_info "Step 2: Formatting as ext2 (block size: $BLOCK_SIZE)..."
mkfs.ext2 -b "$BLOCK_SIZE" -F "$IMG_NAME" > /dev/null 2>&1
log_info "✓ Formatted as ext2"

mkdir -p "$MOUNT_POINT"
log_info "✓ Created mount point $MOUNT_POINT"

log_info "Step 3: Mounting filesystem..."
sudo mount -t ext2 "$IMG_NAME" "$MOUNT_POINT"
log_info "✓ Mounted at $MOUNT_POINT"

log_info "Step 4: Creating test files and directories..."

mkdir -p "$MOUNT_POINT/dir1" "$MOUNT_POINT/dir2" "$MOUNT_POINT/subdir"
log_info "✓ Created directories"

echo "Hello, this is a test file." > "$MOUNT_POINT/file1.txt"
log_info "✓ Created regular file: file1.txt"

dd if=/dev/zero bs=1024 count=100 2>/dev/null | tr '\0' 'A' > "$MOUNT_POINT/file2.txt"
log_info "✓ Created file: file2.txt (100KB)"

dd if=/dev/zero bs=1024 count=256 2>/dev/null | tr '\0' 'B' > "$MOUNT_POINT/dir1/largefile.bin"
log_info "✓ Created file with indirect blocks: dir1/largefile.bin (256KB)"

dd if=/dev/zero bs=1 count=0 seek=10M 2>/dev/null > "$MOUNT_POINT/sparse_10m.bin"
log_info "✓ Created sparse file: sparse_10m.bin (10MB, mostly empty)"

log_info "Creating huge sparse file (5GB)... this may take a moment"
dd if=/dev/zero bs=1 count=0 seek=5G 2>/dev/null > "$MOUNT_POINT/sparse_5g.bin" || \
    log_warn "Could not create 5GB sparse file (may not be supported)"
log_info "✓ Created huge sparse file: sparse_5g.bin"

echo "Nested file content" > "$MOUNT_POINT/dir2/nested.txt"
log_info "✓ Created file in subdirectory: dir2/nested.txt"

dd if=/dev/urandom bs=512 count=10 2>/dev/null > "$MOUNT_POINT/subdir/binary.dat"
log_info "✓ Created binary file: subdir/binary.dat"

log_info "Step 5: Computing checksums (on mounted files)..."
> checksum_paths.txt
while IFS= read -r -d '' f; do
    path="${f#"$MOUNT_POINT"/}"
    sha=$(sha512sum "$f" | awk '{print $1}')
    echo "$path $sha" >> checksum_paths.txt
done < <(find "$MOUNT_POINT" -type f -print0)

log_info "Step 6: Unmounting filesystem..."
sudo umount "$MOUNT_POINT"
rmdir "$MOUNT_POINT"
log_info "✓ Unmounted"

log_info "Step 7: Resolving ext2 inode numbers from image..."
> checksum.txt
while IFS= read -r line; do
    path=$(echo "$line" | awk '{print $1}')
    sha=$(echo "$line" | awk '{print $2}')
    if [ -z "$path" ]; then
        continue
    fi
    inode=$(ext2_inode_for_path "$path")
    if [ -z "$inode" ]; then
        log_warn "  Could not resolve inode for: $path"
        continue
    fi
    echo "${inode}|${path}|${sha}" >> checksum.txt
    log_info "  Inode $inode: $path -> ${sha:0:16}..."
done < checksum_paths.txt

ROOT_INODE=2
DIR1_INODE=$(ext2_inode_for_path "dir1")
DIR2_INODE=$(ext2_inode_for_path "dir2")

log_info "Directory inodes (from ext2.img):"
log_info "  root: $ROOT_INODE"
log_info "  dir1: $DIR1_INODE"
log_info "  dir2: $DIR2_INODE"

log_info "Step 8: Testing utilities on image file..."

log_info "  Testing getinodeinfo..."
run_tool getinodeinfo "$IMG_NAME" 1 > /dev/null
log_info "  ✓ getinodeinfo works"

log_info "  Verifying file checksums..."
checksums_ok=0
checksums_total=0

while IFS='|' read -r inode_num path expected_sha; do
    if [ -z "$inode_num" ] || [ -z "$path" ] || [ -z "$expected_sha" ]; then
        continue
    fi
    checksums_total=$((checksums_total + 1))
    
    computed_sha=$(run_tool getinodedata "$IMG_NAME" "$inode_num" 2>/dev/null | sha512sum | awk '{print $1}')
    
    if [ "$computed_sha" = "$expected_sha" ]; then
        log_info "    ✓ Inode $inode_num ($path): checksum OK"
        checksums_ok=$((checksums_ok + 1))
    else
        log_error "    ✗ Inode $inode_num ($path): checksum MISMATCH"
        log_error "      Expected: $expected_sha"
        log_error "      Got:      $computed_sha"
    fi
done < checksum.txt

log_info "Checksum verification: $checksums_ok/$checksums_total OK"

log_info "  Testing getdirinfo..."
log_info "  Root directory ($ROOT_INODE):"
run_tool getdirinfo "$IMG_NAME" "$ROOT_INODE" > dirinfo.txt 2>/dev/null || true

if grep -q "file1.txt" dirinfo.txt; then
    log_info "    ✓ Found file1.txt"
else
    log_error "    ✗ Missing file1.txt in directory listing"
fi

if grep -q "dir1" dirinfo.txt; then
    log_info "    ✓ Found dir1"
else
    log_error "    ✗ Missing dir1 in directory listing"
fi

log_info "Step 9: Testing with loop device..."

LOOP_DEV=$(sudo losetup -f)
log_info "  Using loop device: $LOOP_DEV"

sudo losetup "$LOOP_DEV" "$IMG_NAME"
log_info "  ✓ Attached image to loop device"

log_info "  Testing getinodeinfo on loop device..."
run_tool getinodeinfo "$LOOP_DEV" "$ROOT_INODE" > /dev/null
log_info "  ✓ getinodeinfo works on loop device"

log_info "  Verifying checksums on loop device..."
checksums_ok_loop=0
while IFS='|' read -r inode_num path expected_sha; do
    if [ -z "$inode_num" ] || [ -z "$path" ] || [ -z "$expected_sha" ]; then
        continue
    fi
    
    computed_sha=$(run_tool getinodedata "$LOOP_DEV" "$inode_num" 2>/dev/null | sha512sum | awk '{print $1}')
    
    if [ "$computed_sha" = "$expected_sha" ]; then
        checksums_ok_loop=$((checksums_ok_loop + 1))
    else
        log_error "    Checksum mismatch for inode $inode_num on loop device"
    fi
done < checksum.txt

log_info "Loop device checksum verification: $checksums_ok_loop/$checksums_total OK"

sudo losetup -d "$LOOP_DEV"
LOOP_DEV=""
log_info "  ✓ Detached loop device"

log_info "Step 10: Test Summary"
log_info "=============================="
log_info "Image file: $IMG_NAME"
log_info "Block size: $BLOCK_SIZE"
log_info "Image size: $IMG_SIZE"
log_info "Files tested: $checksums_total"
log_info "Checksums OK (image): $checksums_ok/$checksums_total"
log_info "Checksums OK (loop): $checksums_ok_loop/$checksums_total"

if [ $checksums_total -eq 0 ]; then
    log_error "No checksums were verified (inode resolution failed?)"
    exit 1
fi

if [ $checksums_ok -eq $checksums_total ] && [ $checksums_ok_loop -eq $checksums_total ]; then
    log_info "=============================="
    log_info "✓ All tests PASSED"
    log_info "=============================="
    exit 0
else
    log_error "=============================="
    log_error "✗ Some tests FAILED"
    log_error "=============================="
    exit 1
fi
