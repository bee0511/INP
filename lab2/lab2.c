#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <math.h>
#include <string.h>

#define DEBUG 0

typedef struct
{
    uint64_t magic;    /* 'BINFLAG\x00' */
    uint32_t datasize; /* in big-endian */
    uint16_t n_blocks; /* in big-endian */
    uint16_t zeros;
} __attribute((packed)) binflag_header_t;

typedef struct
{
    uint32_t offset; /* in big-endian */
    uint16_t cksum;  /* XOR'ed results of each 2-byte unit in payload */
    uint16_t length; /* ranges from 1KB - 3KB, in big-endian */
    uint8_t payload[0];
} __attribute((packed)) block_t;

typedef struct
{
    uint16_t length;    /* length of the offset array, in big-endian */
    uint32_t offset[0]; /* offset of the flags, in big-endian */
} __attribute((packed)) flag_t;

uint16_t convert_indian_u16(uint16_t num)
{
    uint16_t ret = 0;
    ret |= (num & 0x00ff) << 8;
    ret |= (num & 0xff00) >> 8;
    return ret;
}

uint32_t convert_indian_u32(uint32_t num)
{
    uint32_t ret = 0;
    ret |= (num & 0x000000ff) << 24;
    ret |= (num & 0x0000ff00) << 8;
    ret |= (num & 0x00ff0000) >> 8;
    ret |= (num & 0xff000000) >> 24;
    return ret;
}

uint64_t convert_indian_u64(uint64_t num)
{
    uint64_t ret = 0;
    ret |= (num & 0x00000000000000ff) << 56;
    ret |= (num & 0x000000000000ff00) << 40;
    ret |= (num & 0x0000000000ff0000) << 24;
    ret |= (num & 0x00000000ff000000) << 8;
    ret |= (num & 0x000000ff00000000) >> 8;
    ret |= (num & 0x0000ff0000000000) >> 24;
    ret |= (num & 0x00ff000000000000) >> 40;
    ret |= (num & 0xff00000000000000) >> 56;
    return ret;
}

int main(int argc, char *argv[])
{
    if (argc != 2)
    {
        printf("Argument Error\n");
        return 0;
    }

    int fp = open(argv[1], O_RDONLY);
    if (fp < 0)
    {
        printf("Open Error \n");
        return 0;
    }

    binflag_header_t hdr;
    int b_read = read(fp, &hdr, sizeof(binflag_header_t));
    hdr.n_blocks = convert_indian_u16(hdr.n_blocks);
    hdr.datasize = convert_indian_u32(hdr.datasize);
// Dump parsed hdr
#ifdef DEBUG
    printf("[DEBUG] ----------------------------------\n");
    // printf("[DEBUG] Magic: %lx\n", hdr.magic);
    // printf("[DEBUG] datasize: %d\n", hdr.datasize);
    // printf("[DEBUG] n_blocks: %x\n", hdr.n_blocks);
    // printf("[DEBUG] zeros: %x\n", hdr.zeros);
    // printf("[DEBUG] ----------------------------------\n");
#endif

    // Read and output files
    uint8_t *dict = (uint8_t *)malloc(hdr.datasize);
    memset(dict, 0, hdr.datasize);
    block_t *block_entries = (block_t *)malloc(sizeof(block_t));

    for (int i = 0; i < hdr.n_blocks; ++i)
    {
        b_read = read(fp, block_entries, sizeof(block_t));
        block_entries->length = convert_indian_u16(block_entries->length);
        block_entries->offset = convert_indian_u32(block_entries->offset);
        block_entries->cksum = convert_indian_u16(block_entries->cksum);
#ifdef DEBUG
// printf("[DEBUG] ----------------------------------\n");
// printf("[DEBUG] Block Entry %d\n", i);
// printf("[DEBUG] Block Offset: 0x%x\n", block_entries->offset);
// printf("[DEBUG] Block Checksum: 0x%x\n", block_entries->cksum);
// printf("[DEBUG] Block length: %d\n", block_entries->length);
// printf("[DEBUG] ----------------------------------\n");
#endif

        // Read payload data directly into the block_entries[i].payload field
        b_read = read(fp, block_entries->payload, block_entries->length);

#ifdef DEBUG
        // printf("[DEBUG] payload: ");
        // for (int j = 0; j < block_entries->length; j++)
        // {
        //     printf("%x ", block_entries->payload[j]);
        // }
        // printf("\n");
#endif
        uint16_t xored_val = 0;
        for (int j = 0; j < block_entries->length; j += 2)
        {
            xored_val ^= (block_entries->payload[j] << 8) + block_entries->payload[j + 1];
        }
#ifdef DEBUG
        // printf("[DEBUG] xored_val: 0x%x\n", xored_val);
#endif
        if (xored_val == block_entries->cksum)
        {
            // Copy the payload data to the dictionary buffer
            memcpy(dict + block_entries->offset, block_entries->payload, block_entries->length);
        }
    }
#ifdef DEBUG
    // printf("[DEBUG] dict: ");
    // for (int i = 0; i < hdr.datasize; i++)
    // {
    //     printf("%x ", dict[i]);
    // }
    // printf("\n");
#endif
    uint16_t flag;
    b_read = read(fp, &flag, sizeof(flag));
    flag = convert_indian_u16(flag);
#ifdef DEBUG
    // printf("[DEBUG] Flag Length: %d\n", flag);
    // // printf("[DEBUG] Flag Offset: %x\n", flag.offset);
    // printf("[DEBUG] ----------------------------------\n");
#endif
    uint32_t flag_payload[flag];
    memset(flag_payload, 0, flag * sizeof(uint32_t));
    for (int i = 0; i < flag; i++)
    {
        read(fp, &flag_payload[i], sizeof(uint32_t));
        flag_payload[i] = convert_indian_u32(flag_payload[i]);
        // printf("%d ", flag_payload[i]);
    }
    printf("\nflag:");
    uint16_t ans[flag];
    memset(ans, 0, flag * sizeof(uint16_t));
    for (int i = 0; i < flag; i++)
    {
        ans[i] = (dict[flag_payload[i]] << 8) + dict[flag_payload[i] + 1];
        // printf("%x %x\n", dict[flag_payload[i]],  dict[flag_payload[i] + 1]);
        printf("%04x", ans[i]);
    }
    printf("\n\n");

    close(fp);

    return 0;
}