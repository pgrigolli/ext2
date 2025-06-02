#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdint.h> // For uint32_t, uint16_t

// Base Superblock Fields from Ext2 documentation
// All values are little-endian on disk.
struct ext2_super_block {
    uint32_t s_inodes_count;      // Total inode count
    uint32_t s_blocks_count;      // Total block count
    uint32_t s_r_blocks_count;    // Reserved block count
    uint32_t s_free_blocks_count; // Free block count
    uint32_t s_free_inodes_count; // Free inode count
    uint32_t s_first_data_block;  // First Data Block
    uint32_t s_log_block_size;    // Block size (log2 of block size minus 10)
    uint32_t s_log_frag_size;     // Fragment size (log2 of fragment size minus 10)
    uint32_t s_blocks_per_group;  // Blocks per group
    uint32_t s_frags_per_group;   // Fragments per group
    uint32_t s_inodes_per_group;  // Inodes per group
    uint32_t s_mtime;             // Mount time
    uint32_t s_wtime;             // Write time
    uint16_t s_mnt_count;         // Mount count
    uint16_t s_max_mnt_count;     // Maximal mount count
    uint16_t s_magic;             // Magic signature (0xEF53)
    uint16_t s_state;             // File system state
    uint16_t s_errors;            // Behaviour when detecting errors
    uint16_t s_minor_rev_level;   // Minor revision level
    uint32_t s_lastcheck;         // Time of last check
    uint32_t s_checkinterval;     // Max. time between checks
    uint32_t s_creator_os;        // Creator OS
    uint32_t s_rev_level;         // Revision level
    uint16_t s_def_resuid;        // Default uid for reserved blocks
    uint16_t s_def_resgid;        // Default gid for reserved blocks
    // Extended Superblock Fields (if s_rev_level >= 1)
    uint32_t s_first_ino;         // First non-reserved inode
    uint16_t s_inode_size;        // Size of inode structure
    uint16_t s_block_group_nr;    // Block group # of this superblock
    uint32_t s_feature_compat;    // Compatible feature set
    uint32_t s_feature_incompat;  // Incompatible feature set
    uint32_t s_feature_ro_compat; // Readonly-compatible feature set
    uint8_t  s_uuid[16];          // 128-bit uuid for volume
    char     s_volume_name[16];   // Volume name
    char     s_last_mounted[64];  // Directory where last mounted
    uint32_t s_algo_bitmap;       // For compression (usage varies)
    // Skipping other fields for brevity for now
};

#define SUPERBLOCK_OFFSET 1024

// Function to read the superblock
int read_superblock(const char *device_path, struct ext2_super_block *sb) {
    int fd = open(device_path, O_RDONLY);
    if (fd < 0) {
        perror("Error opening device");
        return -1;
    }

    if (lseek(fd, SUPERBLOCK_OFFSET, SEEK_SET) < 0) {
        perror("Error seeking to superblock");
        close(fd);
        return -1;
    }

    if (read(fd, sb, sizeof(struct ext2_super_block)) != sizeof(struct ext2_super_block)) {
        perror("Error reading superblock");
        close(fd);
        return -1;
    }

    close(fd);
    return 0; // Success
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <ext2_disk_image>\n", argv[0]);
        return 1;
    }

    const char *disk_image_path = argv[1];
    struct ext2_super_block sb;

    printf("Attempting to read superblock from: %s\n", disk_image_path);

    if (read_superblock(disk_image_path, &sb) == 0) {
        printf("Superblock read successfully!\n");
        printf("  Magic number: 0x%X (Expected: 0xEF53)\n", sb.s_magic);
        printf("  Total inodes: %u\n", sb.s_inodes_count);
        printf("  Total blocks: %u\n", sb.s_blocks_count);
        printf("  Blocks per group: %u\n", sb.s_blocks_per_group);
        printf("  Inodes per group: %u\n", sb.s_inodes_per_group);
        unsigned int block_size = 1024 << sb.s_log_block_size;
        printf("  Block size: %u bytes\n", block_size);
        printf("  Volume name: %.16s\n", sb.s_volume_name);
    } else {
        fprintf(stderr, "Failed to read superblock.\n");
        return 1;
    }

    return 0;
} 