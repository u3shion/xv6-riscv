#include "ext2.h"
#include <time.h>

void print_mode(uint16_t mode) {
    printf("Mode: 0%o (", mode & 0777);
    if ((mode & EXT2_S_IFREG) == EXT2_S_IFREG) printf("regular file");
    else if ((mode & EXT2_S_IFDIR) == EXT2_S_IFDIR) printf("directory");
    else if ((mode & EXT2_S_IFLNK) == EXT2_S_IFLNK) printf("symlink");
    else printf("unknown");
    printf(")\n");
}

void print_time(const char *label, uint32_t timestamp) {
    struct tm *tm_info;
    char buffer[64];
    time_t t = (time_t)timestamp;
    tm_info = localtime(&t);
    strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", tm_info);
    printf("%s: %s (unix: %u)\n", label, buffer, timestamp);
}

int main(int argc, char *argv[]) {
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <device/image> <inode_number>\n", argv[0]);
        return 1;
    }

    const char *device = argv[1];
    uint32_t inode_num = atoi(argv[2]);

    ext2_fs_t fs;
    if (ext2_open(device, &fs) != 0) {
        fprintf(stderr, "Error opening filesystem\n");
        return 1;
    }

    printf("=== Inode Information ===\n");
    printf("Filesystem: %s\n", device);
    printf("Block size: %d bytes\n", fs.block_size);
    printf("Inode size: %d bytes\n", fs.inode_size);
    printf("Total inodes: %u\n\n", fs.sb->inodes_count);

    ext2_inode_t inode;
    if (ext2_read_inode(&fs, inode_num, &inode) != 0) {
        fprintf(stderr, "Error reading inode %u\n", inode_num);
        ext2_close(&fs);
        return 1;
    }

    printf("Inode: %u\n", inode_num);
    print_mode(inode.mode);
    printf("UID: %u\n", inode.uid);
    printf("GID: %u\n", inode.gid);
    printf("Size: %llu bytes\n", (unsigned long long)ext2_inode_size_bytes(&inode, fs.inode_size));
    printf("Hard links: %u\n", inode.links_count);
    printf("Blocks allocated (512-byte blocks): %u\n", inode.blocks);
    
    print_time("Created", inode.ctime);
    print_time("Modified", inode.mtime);
    print_time("Accessed", inode.atime);
    if (inode.dtime != 0)
        print_time("Deleted", inode.dtime);

    printf("\n=== Block Addresses ===\n");
    printf("Direct blocks (0-11):\n");
    for (int i = 0; i < EXT2_DIRECT_BLOCKS; i++) {
        if (inode.block[i] != 0)
            printf("  [%d]: %u\n", i, inode.block[i]);
    }

    if (inode.block[EXT2_IND_BLOCK] != 0)
        printf("Single indirect block [12]: %u\n", inode.block[EXT2_IND_BLOCK]);

    if (inode.block[EXT2_DIND_BLOCK] != 0)
        printf("Double indirect block [13]: %u\n", inode.block[EXT2_DIND_BLOCK]);

    if (inode.block[EXT2_TIND_BLOCK] != 0)
        printf("Triple indirect block [14]: %u\n", inode.block[EXT2_TIND_BLOCK]);

    printf("\nFlags: 0x%x\n", inode.flags);
    printf("Generation: %u\n", inode.generation);

    ext2_close(&fs);
    return 0;
}
