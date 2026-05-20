#include "ext2.h"
#include <errno.h>
#include <unistd.h>

int ext2_open(const char *path, ext2_fs_t *fs) {
    FILE *fp = fopen(path, "rb");
    if (!fp) {
        perror("fopen");
        return -1;
    }

    fs->fp = fp;
    fs->sb = malloc(sizeof(ext2_superblock_t));
    if (!fs->sb) {
        perror("malloc");
        fclose(fp);
        return -1;
    }

    if (fseek(fp, 1024, SEEK_SET) != 0) {
        perror("fseek");
        free(fs->sb);
        fclose(fp);
        return -1;
    }

    if (fread(fs->sb, sizeof(ext2_superblock_t), 1, fp) != 1) {
        perror("fread superblock");
        free(fs->sb);
        fclose(fp);
        return -1;
    }

    ext2_superblock_t *sb = fs->sb;
    sb->inodes_count = le32_to_cpu(sb->inodes_count);
    sb->blocks_count = le32_to_cpu(sb->blocks_count);
    sb->reserved_blocks_count = le32_to_cpu(sb->reserved_blocks_count);
    sb->free_blocks_count = le32_to_cpu(sb->free_blocks_count);
    sb->free_inodes_count = le32_to_cpu(sb->free_inodes_count);
    sb->first_data_block = le32_to_cpu(sb->first_data_block);
    sb->block_size_code = le32_to_cpu(sb->block_size_code);
    sb->fragment_size_code = le32_to_cpu(sb->fragment_size_code);
    sb->blocks_per_group = le32_to_cpu(sb->blocks_per_group);
    sb->fragments_per_group = le32_to_cpu(sb->fragments_per_group);
    sb->inodes_per_group = le32_to_cpu(sb->inodes_per_group);
    sb->mtime = le32_to_cpu(sb->mtime);
    sb->wtime = le32_to_cpu(sb->wtime);
    sb->mount_count = le16_to_cpu(sb->mount_count);
    sb->max_mount_count = le16_to_cpu(sb->max_mount_count);
    sb->magic = le16_to_cpu(sb->magic);
    sb->state = le16_to_cpu(sb->state);
    sb->errors = le16_to_cpu(sb->errors);
    sb->minor_revision = le16_to_cpu(sb->minor_revision);
    sb->lastcheck = le32_to_cpu(sb->lastcheck);
    sb->checkinterval = le32_to_cpu(sb->checkinterval);
    sb->creator_os = le32_to_cpu(sb->creator_os);
    sb->revision_level = le32_to_cpu(sb->revision_level);
    sb->reserved_uid = le16_to_cpu(sb->reserved_uid);
    sb->reserved_gid = le16_to_cpu(sb->reserved_gid);
    sb->first_inode = le32_to_cpu(sb->first_inode);
    sb->inode_size = le16_to_cpu(sb->inode_size);
    sb->block_group_number = le16_to_cpu(sb->block_group_number);
    sb->feature_compat = le32_to_cpu(sb->feature_compat);
    sb->feature_incompat = le32_to_cpu(sb->feature_incompat);
    sb->feature_ro_compat = le32_to_cpu(sb->feature_ro_compat);
    sb->algorithm_usage_bitmap = le32_to_cpu(sb->algorithm_usage_bitmap);
    sb->desc_size = le16_to_cpu(sb->desc_size);

    if (sb->magic != EXT2_SUPER_MAGIC) {
        fprintf(stderr, "Error: Invalid ext2 magic number: 0x%x\n", sb->magic);
        free(fs->sb);
        fclose(fp);
        return -1;
    }

    fs->block_size = EXT2_MIN_BLOCK_SIZE << sb->block_size_code;

    if (sb->inode_size >= EXT2_GOOD_OLD_INODE_SIZE)
        fs->inode_size = sb->inode_size;
    else
        fs->inode_size = EXT2_GOOD_OLD_INODE_SIZE;

    fs->bgd_size = EXT2_GOOD_OLD_DESC_SIZE;
    if (sb->feature_incompat & EXT2_FEATURE_INCOMPAT_64BIT) {
        if (sb->desc_size >= EXT2_GOOD_OLD_DESC_SIZE)
            fs->bgd_size = sb->desc_size;
        else
            fs->bgd_size = 64;
    }

    return 0;
}

static int ext2_mode_matches_type(uint16_t mode, uint8_t file_type) {
    uint16_t ft = mode & 0xF000;
    switch (file_type) {
        case 1: return ft == EXT2_S_IFREG;
        case 2: return ft == EXT2_S_IFDIR;
        case 3: return ft == 0x2000;
        case 4: return ft == 0x6000;
        case 5: return ft == 0x1000;
        case 6: return ft == 0xC000;
        case 7: return ft == EXT2_S_IFLNK;
        default: return 1;
    }
}

uint64_t ext2_inode_size_bytes(const ext2_inode_t *inode, int inode_size) {
    uint64_t sz = inode->size;

    if (inode_size > EXT2_GOOD_OLD_INODE_SIZE && (inode->mode & EXT2_S_IFREG) == EXT2_S_IFREG)
        sz |= (uint64_t)inode->dir_acl << 32;

    return sz;
}

uint32_t ext2_dirent_inode(ext2_fs_t *fs, uint32_t raw_inode, uint8_t file_type) {
    uint32_t candidates[3];
    int n = 0;

    if (raw_inode == 0)
        return 0;

    candidates[n++] = raw_inode;

    if (raw_inode > fs->sb->inodes_count) {
        uint32_t shifted = raw_inode >> 12;
        if (shifted >= 1 && shifted <= fs->sb->inodes_count)
            candidates[n++] = shifted;
    }

    for (int i = 0; i < n; i++) {
        ext2_inode_t in;
        if (ext2_read_inode(fs, candidates[i], &in) != 0)
            continue;
        if (file_type != 0 && !ext2_mode_matches_type(in.mode, file_type))
            continue;
        return candidates[i];
    }

    return raw_inode;
}

void ext2_close(ext2_fs_t *fs) {
    if (fs->sb) {
        free(fs->sb);
        fs->sb = NULL;
    }
    if (fs->fp) {
        fclose(fs->fp);
        fs->fp = NULL;
    }
}

static off_t ext2_bgd_offset(ext2_fs_t *fs, uint32_t group) {
    off_t base = (off_t)(fs->sb->first_data_block + 1) * fs->block_size;
    return base + (off_t)group * fs->bgd_size;
}

static uint32_t ext2_bg_inode_table(ext2_fs_t *fs, const ext2_bgd_t *bgd) {
    uint64_t table = le32_to_cpu(bgd->inode_table);
    if (fs->bgd_size >= 64)
        table |= (uint64_t)le32_to_cpu(bgd->inode_table_hi) << 32;
    return (uint32_t)table;
}

static int ext2_read_group_desc(ext2_fs_t *fs, uint32_t group, ext2_bgd_t *bgd) {
    off_t bgd_offset = ext2_bgd_offset(fs, group);

    if (fseek(fs->fp, bgd_offset, SEEK_SET) != 0) {
        perror("fseek bgd");
        return -1;
    }

    size_t read_sz = (size_t)fs->bgd_size;
    if (read_sz > sizeof(ext2_bgd_t))
        read_sz = sizeof(ext2_bgd_t);

    memset(bgd, 0, sizeof(*bgd));
    if (fread(bgd, read_sz, 1, fs->fp) != 1) {
        perror("fread bgd");
        return -1;
    }

    return 0;
}

static uint16_t ext2_peek_inode_mode(ext2_fs_t *fs, uint32_t inode_table, uint32_t inode_num) {
    uint32_t index = inode_num - 1;
    uint32_t offset = (index % fs->sb->inodes_per_group) * fs->inode_size;
    off_t inode_offset = (off_t)inode_table * fs->block_size + offset;
    uint16_t mode = 0;

    if (fseek(fs->fp, inode_offset, SEEK_SET) != 0)
        return 0;
    if (fread(&mode, sizeof(mode), 1, fs->fp) != 1)
        return 0;
    return le16_to_cpu(mode);
}

static uint32_t ext2_resolve_inode_table(ext2_fs_t *fs, uint32_t group) {
    ext2_bgd_t bgd;
    uint32_t table;

    if (ext2_read_group_desc(fs, group, &bgd) != 0)
        return 0;

    table = ext2_bg_inode_table(fs, &bgd);
    if (table != 0 && ext2_peek_inode_mode(fs, table, 2) != 0)
        return table;

    off_t bases[] = {
        (off_t)(fs->sb->first_data_block + 1) * fs->block_size,
        2 * (off_t)fs->block_size,
        (off_t)fs->block_size + 1024,
    };

    for (size_t i = 0; i < sizeof(bases) / sizeof(bases[0]); i++) {
        if (fseek(fs->fp, bases[i] + (off_t)group * fs->bgd_size, SEEK_SET) != 0)
            continue;
        size_t gd_read = (size_t)fs->bgd_size;
        if (gd_read > sizeof(bgd))
            gd_read = sizeof(bgd);
        if (fread(&bgd, gd_read, 1, fs->fp) != 1)
            continue;
        table = ext2_bg_inode_table(fs, &bgd);
        if (table != 0 && ext2_peek_inode_mode(fs, table, 2) != 0)
            return table;
    }

    return ext2_bg_inode_table(fs, &bgd);
}

int ext2_read_inode(ext2_fs_t *fs, uint32_t inode_num, ext2_inode_t *inode) {
    if (inode_num < 1 || inode_num > fs->sb->inodes_count) {
        fprintf(stderr, "Error: Invalid inode number: %u\n", inode_num);
        return -1;
    }

    if (fs->sb->inodes_per_group == 0) {
        fprintf(stderr, "Error: invalid inodes_per_group in superblock\n");
        return -1;
    }

    uint32_t index = inode_num - 1;
    uint32_t group = index / fs->sb->inodes_per_group;
    uint32_t offset = (index % fs->sb->inodes_per_group) * fs->inode_size;

    uint32_t inode_table = ext2_resolve_inode_table(fs, group);
    if (inode_table == 0) {
        fprintf(stderr, "Error: could not locate inode table for group %u\n", group);
        return -1;
    }

    off_t inode_offset = (off_t)inode_table * fs->block_size + offset;

    if (fseek(fs->fp, inode_offset, SEEK_SET) != 0) {
        perror("fseek inode");
        return -1;
    }

    uint8_t inode_buf[256];
    size_t read_size = (size_t)fs->inode_size;
    if (read_size > sizeof(inode_buf))
        read_size = sizeof(inode_buf);

    if (fread(inode_buf, read_size, 1, fs->fp) != 1) {
        perror("fread inode");
        return -1;
    }

    memcpy(inode, inode_buf, sizeof(ext2_inode_t));

    inode->mode = le16_to_cpu(*(uint16_t *)(inode_buf + 0));
    inode->uid = le16_to_cpu(*(uint16_t *)(inode_buf + 2));
    inode->size = le32_to_cpu(*(uint32_t *)(inode_buf + 4));
    inode->atime = le32_to_cpu(*(uint32_t *)(inode_buf + 8));
    inode->ctime = le32_to_cpu(*(uint32_t *)(inode_buf + 12));
    inode->mtime = le32_to_cpu(*(uint32_t *)(inode_buf + 16));
    inode->dtime = le32_to_cpu(*(uint32_t *)(inode_buf + 20));
    inode->gid = le16_to_cpu(*(uint16_t *)(inode_buf + 24));
    inode->links_count = le16_to_cpu(*(uint16_t *)(inode_buf + 26));
    inode->blocks = le32_to_cpu(*(uint32_t *)(inode_buf + 28));
    inode->flags = le32_to_cpu(*(uint32_t *)(inode_buf + 32));
    inode->osd1 = le32_to_cpu(*(uint32_t *)(inode_buf + 36));

    for (int i = 0; i < 15; i++)
        inode->block[i] = le32_to_cpu(*(uint32_t *)(inode_buf + 40 + i * 4));

    inode->generation = le32_to_cpu(*(uint32_t *)(inode_buf + 100));
    inode->file_acl = le32_to_cpu(*(uint32_t *)(inode_buf + 104));
    inode->dir_acl = le32_to_cpu(*(uint32_t *)(inode_buf + 108));
    inode->faddr = le32_to_cpu(*(uint32_t *)(inode_buf + 112));

    return 0;
}

int ext2_read_block(ext2_fs_t *fs, uint32_t block_num, void *buf) {
    if (block_num >= fs->sb->blocks_count) {
        fprintf(stderr, "Error: Invalid block number: %u\n", block_num);
        return -1;
    }

    off_t block_offset = (off_t)block_num * fs->block_size;

    if (fseek(fs->fp, block_offset, SEEK_SET) != 0) {
        perror("fseek block");
        return -1;
    }

    if (fread(buf, fs->block_size, 1, fs->fp) != 1) {
        perror("fread block");
        return -1;
    }

    return 0;
}

static int ext2_map_block(ext2_fs_t *fs, const ext2_inode_t *inode, uint32_t logical_blk,
                          uint32_t *block_num, uint32_t *ptr_buf, uint32_t *ind_buf) {
    uint32_t per_ind = fs->block_size / (uint32_t)sizeof(uint32_t);

    if (logical_blk < EXT2_DIRECT_BLOCKS) {
        *block_num = inode->block[logical_blk];
        return 0;
    }
    logical_blk -= EXT2_DIRECT_BLOCKS;

    if (logical_blk < per_ind) {
        if (inode->block[EXT2_IND_BLOCK] == 0) {
            *block_num = 0;
            return 0;
        }
        if (ext2_read_block(fs, inode->block[EXT2_IND_BLOCK], ptr_buf) != 0)
            return -1;
        *block_num = le32_to_cpu(ptr_buf[logical_blk]);
        return 0;
    }
    logical_blk -= per_ind;

    if (logical_blk < per_ind * per_ind) {
        uint32_t dind_idx = logical_blk / per_ind;
        uint32_t ind_idx = logical_blk % per_ind;

        if (inode->block[EXT2_DIND_BLOCK] == 0) {
            *block_num = 0;
            return 0;
        }
        if (ext2_read_block(fs, inode->block[EXT2_DIND_BLOCK], ptr_buf) != 0)
            return -1;
        uint32_t ind_ptr = le32_to_cpu(ptr_buf[dind_idx]);
        if (ind_ptr == 0) {
            *block_num = 0;
            return 0;
        }
        if (ext2_read_block(fs, ind_ptr, ind_buf) != 0)
            return -1;
        *block_num = le32_to_cpu(ind_buf[ind_idx]);
        return 0;
    }
    logical_blk -= per_ind * per_ind;

    if (logical_blk < per_ind * per_ind * per_ind) {
        uint32_t tind_idx = logical_blk / (per_ind * per_ind);
        uint32_t dind_idx = (logical_blk / per_ind) % per_ind;
        uint32_t ind_idx = logical_blk % per_ind;

        if (inode->block[EXT2_TIND_BLOCK] == 0) {
            *block_num = 0;
            return 0;
        }
        if (ext2_read_block(fs, inode->block[EXT2_TIND_BLOCK], ptr_buf) != 0)
            return -1;
        uint32_t dind_ptr = le32_to_cpu(ptr_buf[tind_idx]);
        if (dind_ptr == 0) {
            *block_num = 0;
            return 0;
        }
        if (ext2_read_block(fs, dind_ptr, ind_buf) != 0)
            return -1;
        uint32_t ind_ptr = le32_to_cpu(ind_buf[dind_idx]);
        if (ind_ptr == 0) {
            *block_num = 0;
            return 0;
        }
        if (ext2_read_block(fs, ind_ptr, ptr_buf) != 0)
            return -1;
        *block_num = le32_to_cpu(ptr_buf[ind_idx]);
        return 0;
    }

    *block_num = 0;
    return 0;
}

int ext2_read_inode_data_at(ext2_fs_t *fs, uint32_t inode_num, void *buf, size_t len, size_t offset) {
    ext2_inode_t inode;

    if (ext2_read_inode(fs, inode_num, &inode) != 0)
        return -1;

    uint64_t file_size = ext2_inode_size_bytes(&inode, fs->inode_size);

    if ((uint64_t)offset >= file_size)
        return 0;

    if ((uint64_t)len > file_size - offset)
        len = (size_t)(file_size - offset);

    size_t bytes_read = 0;
    uint32_t block_size = fs->block_size;
    uint8_t *buf_ptr = (uint8_t *)buf;
    uint32_t logical_blk = (uint32_t)(offset / block_size);
    size_t skip = offset % block_size;
    uint8_t *block_buf = malloc(block_size);
    uint32_t *ptr_buf = malloc(block_size);
    uint32_t *ind_buf = malloc(block_size);
    if (!block_buf || !ptr_buf || !ind_buf) {
        free(block_buf);
        free(ptr_buf);
        free(ind_buf);
        perror("malloc");
        return -1;
    }

    while (bytes_read < len) {
        uint32_t block_num;

        if (ext2_map_block(fs, &inode, logical_blk, &block_num, ptr_buf, ind_buf) != 0)
            goto fail;

        size_t chunk_len = len - bytes_read;
        if (chunk_len > block_size - skip)
            chunk_len = block_size - skip;

        if (block_num == 0) {
            memset(buf_ptr + bytes_read, 0, chunk_len);
            bytes_read += chunk_len;
        } else {
            if (ext2_read_block(fs, block_num, block_buf) != 0)
                goto fail;
            memcpy(buf_ptr + bytes_read, block_buf + skip, chunk_len);
            bytes_read += chunk_len;
        }

        skip = 0;
        logical_blk++;
    }

    free(block_buf);
    free(ptr_buf);
    free(ind_buf);
    return (int)bytes_read;

fail:
    free(block_buf);
    free(ptr_buf);
    free(ind_buf);
    return -1;
}

int ext2_read_inode_data(ext2_fs_t *fs, uint32_t inode_num, void *buf, size_t len) {
    return ext2_read_inode_data_at(fs, inode_num, buf, len, 0);
}
