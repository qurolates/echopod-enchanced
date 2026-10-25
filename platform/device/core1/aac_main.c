#include <stdint.h>

#include "core1_region.h"
#include "device/core1_control.h"
#include "device/core1_aac.h"
#include "registers.h"

void core1_main(void);

static AacDecoder decoder;
static Core1Aac aac;
static Core1Stream stream;

void core1_main(void)
{
    register_write(CORE1_SHARED_READY, CORE1_READY_MAGIC);
    __asm__ volatile("dsb" ::: "memory");
    register_write(CORE1_MAILBOX_B2A_COMMAND, CORE1_READY_COMMAND);
    register_write(CORE1_MAILBOX_B2A_DATA, 0);
    if (register_read(CORE1_SHARED_COMMAND) != CORE1_COMMAND_STREAM) {
        return;
    }
    core1_stream_init(&stream, core1_region(), core1_aac_decoder(&aac, &decoder, true));
    for (;;) {
        core1_stream_step(&stream);
    }
}
