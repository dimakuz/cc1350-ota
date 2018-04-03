#ifndef OTA_H
#define OTA_H

#include <stdint.h>
#include <ti/sysbios/knl/Task.h>

//#define OTA_FLASH_BASE 0x17000
#define OTA_FLASH_BASE 0xd000
//#define OTA_FLASH_SIZE 0x8000
#define OTA_FLASH_SIZE 0x2000

#define OTA_ACTIVE_ZONE 0
#define OTA_INACTIVE_ZONE 1
#define NR_OTA_ZONES    2
#define OTA_ZONE_SIZE (OTA_FLASH_SIZE / NR_OTA_ZONES)
#define OTA_DONE_MAGIC 0x23513dce
#define OTA_SRAM_BASE   0x20000000


#define ota_entrypoint_t ti_sysbios_knl_Task_FuncPtr
#define DEFINE_ENTRYPOINT(sym)  const char * __attribute__((strong))  __ota_entrypoint_##sym = "sym";

struct ota_load {
    uintptr_t dest;
    size_t offset;
    size_t len;
};

#define OTA_MAX_LOADS 3

struct ota_metadata {
    unsigned long gen;
    ota_entrypoint_t entrypoint;
    size_t size;
    struct ota_load loads[OTA_MAX_LOADS];
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
    struct ota_load loads[OTA_MAX_LOADS];
};

struct ota_dl_state {
    struct ota_zone *target_zone;
    unsigned long target_gen;
    size_t dl_size;
    size_t dl_done;
    ota_entrypoint_t entrypoint;
    size_t sector_size;
    size_t nr_sectors;
    struct ota_load loads[OTA_MAX_LOADS];
    /* dl_csum */
};

void ota_startup(void);
void ota_dl_params_init(struct ota_dl_params *params);
void ota_dl_init(struct ota_dl_state *state, struct ota_dl_params *params);
int ota_dl_begin(struct ota_dl_state *state);
int ota_dl_process(struct ota_dl_state *state, uint8_t *buf, size_t len);
int ota_dl_finish(struct ota_dl_state *state);

int test_ota(uint8_t* buf, size_t data_len);
void test_json(void);

#endif // OTA_H
