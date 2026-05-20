#include "ext2.h"

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

    ext2_inode_t inode;
    if (ext2_read_inode(&fs, inode_num, &inode) != 0) {
        fprintf(stderr, "Error reading inode %u\n", inode_num);
        ext2_close(&fs);
        return 1;
    }

    size_t chunk_size = (size_t)fs.block_size;
    if (chunk_size < 65536)
        chunk_size = 65536;

    uint8_t *buf = malloc(chunk_size);
    if (!buf) {
        perror("malloc");
        ext2_close(&fs);
        return 1;
    }

    uint64_t file_size = ext2_inode_size_bytes(&inode, fs.inode_size);
    uint64_t offset = 0;

    while (offset < file_size) {
        size_t to_read = (size_t)(file_size - offset);
        if (to_read > chunk_size)
            to_read = chunk_size;

        int bytes_read = ext2_read_inode_data_at(&fs, inode_num, buf, to_read, offset);
        if (bytes_read < 0) {
            fprintf(stderr, "Error reading inode data\n");
            free(buf);
            ext2_close(&fs);
            return 1;
        }
        if (bytes_read == 0)
            break;

        if (fwrite(buf, 1, (size_t)bytes_read, stdout) != (size_t)bytes_read) {
            perror("fwrite");
            free(buf);
            ext2_close(&fs);
            return 1;
        }
        offset += (uint64_t)bytes_read;
    }

    free(buf);
    ext2_close(&fs);
    return 0;
}
