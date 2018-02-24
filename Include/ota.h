#ifndef OTA_H
#define OTA_H

#include <stdint.h>

#define OTA_FLASH_BASE 0x18000
#define OTA_FLASH_SIZE 0x8000
#define NR_OTA_ZONES    2
#define OTA_ZONE_SIZE (OTA_FLASH_SIZE / NR_OTA_ZONES)
#define OTA_DONE_MAGIC 0x23513dce

typedef void (*ota_entrypoint_t)(void);

struct ota_metadata {
    unsigned long gen;
    ota_entrypoint_t entrypoint;
    unsigned long done;
};

#define OTA_PAYLOAD_SIZE (OTA_ZONE_SIZE - sizeof (struct ota_metadata))

struct ota_zone {
    unsigned char payload[OTA_PAYLOAD_SIZE];
    struct ota_metadata metadata;
};

struct ota_region {
    struct ota_zone zones[NR_OTA_ZONES];
};

extern struct ota_region *OTA_REGION;

struct ota_dl_params {
    size_t dl_size;
    ota_entrypoint_t entrypoint;
};

struct ota_dl_state {
    struct ota_zone *target_zone;
    unsigned long target_gen;
    size_t dl_size;
    size_t dl_done;
    ota_entrypoint_t entrypoint;
    size_t sector_size;
    size_t first_sector;
    size_t nr_sectors;
    /* dl_csum */
};

void ota_startup(void);
void ota_dl_init(struct ota_dl_state *state, struct ota_dl_params *params);
int ota_dl_begin(struct ota_dl_state *state);
int ota_dl_process(struct ota_dl_state *state, uint8_t *buf, size_t len);
int ota_dl_finish(struct ota_dl_state *state);

void test_ota(void);

#endif // OTA_H
