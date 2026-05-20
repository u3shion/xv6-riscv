#ifndef EXT2_H
#define EXT2_H

#include <stdint.h>
#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(__BYTE_ORDER__)
    #define EXT2_BYTE_ORDER __BYTE_ORDER__
    #define EXT2_ORDER_LITTLE __ORDER_LITTLE_ENDIAN__
    #define EXT2_ORDER_BIG __ORDER_BIG_ENDIAN__
#elif defined(__LITTLE_ENDIAN__)
    #define EXT2_BYTE_ORDER __LITTLE_ENDIAN__
    #define EXT2_ORDER_LITTLE __LITTLE_ENDIAN__
    #define EXT2_ORDER_BIG __BIG_ENDIAN__
#elif defined(__BIG_ENDIAN__)
    #define EXT2_BYTE_ORDER __BIG_ENDIAN__
    #define EXT2_ORDER_LITTLE __LITTLE_ENDIAN__
    #define EXT2_ORDER_BIG __BIG_ENDIAN__
#else
    #define EXT2_BYTE_ORDER 1234
    #define EXT2_ORDER_LITTLE 1234
    #define EXT2_ORDER_BIG 4321
#endif

#define EXT2_SUPER_MAGIC     0xEF53
#define EXT2_DYNAMIC_REV     1
#define EXT2_FEATURE_INCOMPAT_64BIT 0x0080
#define EXT2_FEATURE_INCOMPAT_FILETYPE 0x0002
#define EXT2_GOOD_OLD_INODE_SIZE 128
#define EXT2_GOOD_OLD_DESC_SIZE  32
#define EXT2_MIN_BLOCK_SIZE  1024
#define EXT2_MAX_BLOCK_SIZE  65536

#define EXT2_DIRECT_BLOCKS   12
#define EXT2_IND_BLOCK       12
#define EXT2_DIND_BLOCK      13
#define EXT2_TIND_BLOCK      14

#define EXT2_S_IFREG  0x8000
#define EXT2_S_IFDIR  0x4000
#define EXT2_S_IFLNK  0xA000

typedef uint8_t  u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

#define IS_BIG_ENDIAN (EXT2_BYTE_ORDER == EXT2_ORDER_BIG)

static inline u16 le16_to_cpu(u16 val) {
#if EXT2_BYTE_ORDER == EXT2_ORDER_BIG
    return __builtin_bswap16(val);
#else
    return val;
#endif
}

static inline u32 le32_to_cpu(u32 val) {
#if EXT2_BYTE_ORDER == EXT2_ORDER_BIG
    return __builtin_bswap32(val);
#else
    return val;
#endif
}

static inline u64 le64_to_cpu(u64 val) {
#if EXT2_BYTE_ORDER == EXT2_ORDER_BIG
    return __builtin_bswap64(val);
#else
    return val;
#endif
}

static inline u16 cpu_to_le16(u16 val) {
#if EXT2_BYTE_ORDER == EXT2_ORDER_BIG
    return __builtin_bswap16(val);
#else
    return val;
#endif
}

static inline u32 cpu_to_le32(u32 val) {
#if EXT2_BYTE_ORDER == EXT2_ORDER_BIG
    return __builtin_bswap32(val);
#else
    return val;
#endif
}

typedef struct {
    u32 inodes_count;
    u32 blocks_count;
    u32 reserved_blocks_count;
    u32 free_blocks_count;
    u32 free_inodes_count;
    u32 first_data_block;
    u32 block_size_code;
    u32 fragment_size_code;
    u32 blocks_per_group;
    u32 fragments_per_group;
    u32 inodes_per_group;
    u32 mtime;
    u32 wtime;
    u16 mount_count;
    u16 max_mount_count;
    u16 magic;
    u16 state;
    u16 errors;
    u16 minor_revision;
    u32 lastcheck;
    u32 checkinterval;
    u32 creator_os;
    u32 revision_level;
    u16 reserved_uid;
    u16 reserved_gid;
    u32 first_inode;
    u16 inode_size;
    u16 block_group_number;
    u32 feature_compat;
    u32 feature_incompat;
    u32 feature_ro_compat;
    u8  uuid[16];
    char volume_name[16];
    char last_mounted[64];
    u32 algorithm_usage_bitmap;
    u8  prealloc_blocks;
    u8  prealloc_dir_blocks;
    u16 reserved_gdt_blocks;
    u8  _pad2[46];
    u16 desc_size;
    u8  _unused[768];
} ext2_superblock_t;

typedef struct {
    u32 block_bitmap;
    u32 inode_bitmap;
    u32 inode_table;
    u16 free_blocks_count;
    u16 free_inodes_count;
    u16 used_dirs_count;
    u16 _pad;
    u32 _reserved[4];
    u32 block_bitmap_hi;
    u32 inode_bitmap_hi;
    u32 inode_table_hi;
    u16 _pad2;
    u16 _unused2;
    u32 _reserved2[3];
} ext2_bgd_t;

typedef struct {
    u16 mode;
    u16 uid;
    u32 size;
    u32 atime;
    u32 ctime;
    u32 mtime;
    u32 dtime;
    u16 gid;
    u16 links_count;
    u32 blocks;
    u32 flags;
    u32 osd1;
    u32 block[15];
    u32 generation;
    u32 file_acl;
    u32 dir_acl;
    u32 faddr;
    u32 osd2_0;
    u32 osd2_1;
    u32 osd2_2;
} ext2_inode_t;

typedef struct {
    u32 inode;
    u16 rec_len;
    u8  name_len;
    u8  file_type;
    char name[255];
} ext2_direntry_t;

typedef struct {
    FILE *fp;
    int block_size;
    int inode_size;
    int bgd_size;
    ext2_superblock_t *sb;
} ext2_fs_t;

int ext2_open(const char *path, ext2_fs_t *fs);
void ext2_close(ext2_fs_t *fs);
int ext2_read_inode(ext2_fs_t *fs, uint32_t inode_num, ext2_inode_t *inode);
int ext2_read_block(ext2_fs_t *fs, uint32_t block_num, void *buf);
int ext2_read_inode_data(ext2_fs_t *fs, uint32_t inode_num, void *buf, size_t len);
int ext2_read_inode_data_at(ext2_fs_t *fs, uint32_t inode_num, void *buf,
                            size_t len, size_t offset);
uint32_t ext2_dirent_inode(ext2_fs_t *fs, uint32_t raw_inode, uint8_t file_type);
uint64_t ext2_inode_size_bytes(const ext2_inode_t *inode, int inode_size);

#endif
