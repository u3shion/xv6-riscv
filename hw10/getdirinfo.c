#include "ext2.h"
#include <ctype.h>
#include <stdint.h>

const char *get_file_type(uint8_t type) {
    switch (type) {
        case 0: return "unknown";
        case 1: return "regular file";
        case 2: return "directory";
        case 3: return "character device";
        case 4: return "block device";
        case 5: return "FIFO";
        case 6: return "socket";
        case 7: return "symbolic link";
        default: return "?";
    }
}

void print_safe_name(const char *name, size_t len) {
    for (size_t i = 0; i < len; i++) {
        if (isprint((unsigned char)name[i]))
            printf("%c", name[i]);
        else
            printf("\\x%02x", (unsigned char)name[i]);
    }
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

    ext2_inode_t inode;
    if (ext2_read_inode(&fs, inode_num, &inode) != 0) {
        fprintf(stderr, "Error reading inode %u\n", inode_num);
        ext2_close(&fs);
        return 1;
    }

    if ((inode.mode & EXT2_S_IFDIR) == 0) {
        fprintf(stderr, "Error: Inode %u is not a directory (mode=0%o)\n", inode_num, inode.mode);
        ext2_close(&fs);
        return 1;
    }

    uint64_t dir_size = ext2_inode_size_bytes(&inode, fs.inode_size);
    if (dir_size == 0 && inode.blocks > 0)
        dir_size = (uint64_t)inode.blocks * 512;
    if (dir_size == 0)
        dir_size = (size_t)fs.block_size;

    if (dir_size > SIZE_MAX) {
        fprintf(stderr, "Error: directory too large\n");
        ext2_close(&fs);
        return 1;
    }

    uint8_t *dir_buf = malloc((size_t)dir_size);
    if (!dir_buf) {
        perror("malloc");
        ext2_close(&fs);
        return 1;
    }

    int bytes_read = ext2_read_inode_data(&fs, inode_num, dir_buf, dir_size);
    if (bytes_read < 0) {
        fprintf(stderr, "Error reading directory data\n");
        free(dir_buf);
        ext2_close(&fs);
        return 1;
    }

    printf("=== Directory Entries for Inode %u ===\n\n", inode_num);
    printf("%-10s %-32s %s\n", "Inode", "Name", "Type");
    printf("%-10s %-32s %s\n", "----------", "--------------------------------", "-------------------");

    int has_filetype = (fs.sb->feature_incompat & EXT2_FEATURE_INCOMPAT_FILETYPE) != 0;

    size_t offset = 0;
    while (offset + 8 <= (size_t)bytes_read) {
        const uint8_t *ent = dir_buf + offset;
        uint32_t raw_inode = le32_to_cpu(*(const uint32_t *)(ent + 0));
        uint16_t rec_len = le16_to_cpu(*(const uint16_t *)(ent + 4));
        uint8_t name_len;
        uint8_t file_type;
        const char *name;

        if (has_filetype) {
            name_len = ent[6];
            file_type = ent[7];
            name = (const char *)(ent + 8);
        } else {
            name_len = (uint8_t)le16_to_cpu(*(const uint16_t *)(ent + 6));
            file_type = 0;
            name = (const char *)(ent + 8);
        }

        if (rec_len < 8 || offset + rec_len > (size_t)bytes_read)
            break;

        uint16_t min_rec = (uint16_t)(8 + ((name_len + 3) & ~3));
        if (rec_len < min_rec)
            break;

        if (raw_inode != 0 && name_len > 0) {
            uint32_t ent_inode = ext2_dirent_inode(&fs, raw_inode, file_type);

            if (file_type == 0) {
                ext2_inode_t child;
                if (ext2_read_inode(&fs, ent_inode, &child) == 0) {
                    if ((child.mode & EXT2_S_IFDIR) == EXT2_S_IFDIR)
                        file_type = 2;
                    else if ((child.mode & EXT2_S_IFREG) == EXT2_S_IFREG)
                        file_type = 1;
                }
            }

            printf("%-10u ", ent_inode);

            char padded_name[33];
            memset(padded_name, ' ', 32);
            size_t copy_len = name_len < 32 ? name_len : 32;
            memcpy(padded_name, name, copy_len);
            padded_name[32] = '\0';
            printf("%-32s ", padded_name);

            printf("%s\n", get_file_type(file_type));
        }

        offset += rec_len;
    }

    free(dir_buf);
    ext2_close(&fs);
    return 0;
}
