#include <stddef.h>
#include <Include/ota.h>
#include <driverlib/flash.h>
#include <driverlib/vims.h>
#include <ti/sysbios/hal/Hwi.h>
#include <ti/drivers/PWM.h>

#include <../boards/_CC1350_LAUNCHXL/_Board.h>

#define _NEED_DISABLE_CACHE 1
#define _NEED_DISABLE_HWI 1

struct ota_region *OTA_REGION = (struct ota_region *) OTA_FLASH_BASE;

#define INVALID_GEN ((unsigned long) -1)

#if _NEED_DISABLE_CACHE == 1
static uint8_t cache_state(void) {
    return VIMSModeGet(VIMS_BASE);
}

static void set_cache_state(uint32_t mode) {
    VIMSModeSet(VIMS_BASE, mode);
}

static uint32_t disable_cache(void) {
    uint32_t state = cache_state();
    if (state != VIMS_MODE_DISABLED) {
        set_cache_state(VIMS_MODE_DISABLED);
        while (cache_state() != VIMS_MODE_DISABLED)
            ;
    }
    return state;
}

static void enable_cache(uint32_t state) {
    if (state != VIMS_MODE_DISABLED) {
        set_cache_state(VIMS_MODE_ENABLED);
        while (cache_state() != VIMS_MODE_ENABLED)
            ;
    }
}
#endif

#if _NEED_DISABLE_HWI == 1
#define DISABLE_HWI() uint32_t _hwi = Hwi_disable()
#define RESTORE_HWI() Hwi_restore(_hwi)
#else
#define DISABLE_HWI() ((void) 0)
#define RESTORE_HWI() ((void) 0)
#endif


#if _NEED_DISABLE_CACHE == 1
#define DISABLE_CACHE() uint32_t _cache = disable_cache()
#define RESTORE_CACHE() enable_cache(_cache)
#else
#define DISABLE_CACHE() ((void) 0)
#define RESTORE_CACHE() ((void) 0)
#endif

static uint32_t ota_FlashProgram(
        uint8_t *pui8DataBuffer,
        uint32_t ui32Address,
        uint32_t ui32Count)
{
    DISABLE_HWI();
    DISABLE_CACHE();

    uint32_t rc = FlashProgram(pui8DataBuffer, ui32Address, ui32Address);

    RESTORE_CACHE();
    RESTORE_HWI();
    return rc;
}

static void ota_FlashProtectionSet(uint32_t ui32SectorAddress,
                               uint32_t ui32ProtectMode) {
    DISABLE_HWI();
    DISABLE_CACHE();

    FlashProtectionSet(ui32SectorAddress, ui32ProtectMode);

    RESTORE_CACHE();
    RESTORE_HWI();
}


static uint32_t ota_FlashSectorErase(uint32_t ui32SectorAddress) {
    DISABLE_HWI();
    DISABLE_CACHE();

    uint32_t rc = FlashSectorErase(ui32SectorAddress);

    RESTORE_CACHE();
    RESTORE_HWI();

    return rc;
}

static int get_valid_ota_zone(void) {
    unsigned long max_gen = INVALID_GEN;
    int cand_idx = -1;

    for (int i = 0; i < NR_OTA_ZONES; i++) {
        struct ota_zone *cur = &OTA_REGION->zones[i];

        if (cur->metadata.done != OTA_DONE_MAGIC || cur->metadata.gen == INVALID_GEN)
            continue;

        if (max_gen == INVALID_GEN || max_gen < cur->metadata.gen) {
            max_gen = cur->metadata.gen;
            cand_idx = i;
        }
    }

    return cand_idx;
}

void ota_startup(void) {
#if 0
    int idx = get_valid_ota_zone();

    if (idx >= 0)
        OTA_REGION->zones[idx].metadata.entrypoint();
#endif
}

void ota_dl_init(struct ota_dl_state *state, struct ota_dl_params *params) {
    int cur_zone_idx = get_valid_ota_zone();
    if (cur_zone_idx < 0) {
        state->target_zone = &OTA_REGION->zones[0];
        state->target_gen = 0;
    } else {
        state->target_zone = &OTA_REGION->zones[(cur_zone_idx + 1) % NR_OTA_ZONES];
        state->target_gen = OTA_REGION->zones[cur_zone_idx].metadata.gen + 1;
    }
    state->dl_done = 0;
    state->dl_size = params->dl_size;
    state->entrypoint = params->entrypoint;
    state->sector_size = FlashSectorSizeGet();
    state->nr_sectors = sizeof (struct ota_zone) / state->sector_size;
    state->first_sector = ((uint32_t) state->target_zone) / state->sector_size;
}

int ota_dl_begin(struct ota_dl_state *state) {
    for (uint32_t i = state->first_sector; i < state->first_sector + state->nr_sectors; i++) {
        uint32_t rc;

        ota_FlashProtectionSet(i * state->sector_size, FLASH_NO_PROTECT);

        rc = ota_FlashSectorErase(i * state->sector_size);
        if (rc != FAPI_STATUS_SUCCESS)
            return (int) rc;
    }
    return 0;
}

int ota_dl_process(struct ota_dl_state *state, uint8_t *buf, size_t len)  {
        int rc = ota_FlashProgram(
                buf,
                (uint32_t) &state->target_zone->payload[state->dl_done],
                len);

        if (rc != FAPI_STATUS_SUCCESS)
            return rc;

        state->dl_done += len;
        return 0;
}

int ota_dl_finish(struct ota_dl_state *state) {
    unsigned long magic = OTA_DONE_MAGIC;
    size_t flash_size = FlashSizeGet();

    int rc = ota_FlashProgram(
            (uint8_t *) &state->entrypoint,
            (uint32_t) &state->target_zone->metadata.entrypoint,
            sizeof (ota_entrypoint_t));

    if (rc != FAPI_STATUS_SUCCESS)
        return (int) rc;

    rc = ota_FlashProgram(
            (uint8_t *) &state->target_gen,
            (uint32_t) &state->target_zone->metadata.gen,
            sizeof (unsigned long));

    if (rc != FAPI_STATUS_SUCCESS)
        return (int) rc;

    rc = ota_FlashProgram(
            (uint8_t *) &magic,
            (uint32_t) &state->target_zone->metadata.done,
            sizeof (unsigned long));

    if (rc != FAPI_STATUS_SUCCESS)
        return (int) rc;

    for (uint32_t i = state->first_sector; i < state->first_sector + state->nr_sectors; i++) {
        ota_FlashProtectionSet(i * state->sector_size, FLASH_WRITE_PROTECT);
    }
    return 0;
}

void test_ota(void) {
    struct ota_dl_params p;
    struct ota_dl_state s;
    p.dl_size = 10;
    p.entrypoint = 0;


    ota_dl_init(&s, &p);
#if 1
    ota_dl_begin(&s);
    ota_dl_finish(&s);
#endif

    PWM_Handle pwm;
    PWM_Params pwm_p;
    int pwm_id;

    if (get_valid_ota_zone() == 0)
        pwm_id = Board_PWM0;
    else
        pwm_id = Board_PWM1;

    PWM_init();
    PWM_Params_init(&pwm_p);
    pwm_p.dutyUnits = PWM_DUTY_US;
    pwm_p.dutyValue = 100;
    pwm_p.periodUnits = PWM_PERIOD_US;
    pwm_p.periodValue = 100;
    pwm = PWM_open(pwm_id, &pwm_p);
    while (pwm == NULL)
        ;
    PWM_start(pwm);
}
