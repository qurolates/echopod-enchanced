#include <stdint.h>

#include "core1_region.h"
#include "device/core1_control.h"
#include "device/core1_opus.h"
#include "registers.h"

void core1_main(void);

static OpusDecoderContext decoder;
static Core1Opus opus;
static Core1Stream stream;

enum {
    SYSTICK_RELOAD_1MS = 100000 - 1,
    SYSTICK_ENABLE_INTERRUPT_CORE_CLOCK = 0x7u
};

static void start_systick_fallback(void)
{
    register_write(SYSTICK_CTRL, 0);
    register_write(SYSTICK_RELOAD, SYSTICK_RELOAD_1MS);
    register_write(SYSTICK_CURRENT, 0);
    register_write(SYSTICK_CTRL, SYSTICK_ENABLE_INTERRUPT_CORE_CLOCK);
}

static void wait_for_work(void)
{
    register_write(SCB_ICSR, SCB_ICSR_PENDSTCLR);
    __asm__ volatile("wfe" ::: "memory");
}

void core1_main(void)
{
    register_write(CORE1_SHARED_READY, CORE1_READY_MAGIC);
    __asm__ volatile("dsb" ::: "memory");
    register_write(CORE1_MAILBOX_B2A_COMMAND, CORE1_READY_COMMAND);
    register_write(CORE1_MAILBOX_B2A_DATA, 0);
    if (register_read(CORE1_SHARED_COMMAND) != CORE1_COMMAND_STREAM) {
        return;
    }
    core1_stream_init(&stream, core1_region(), core1_opus_decoder(&opus, &decoder));
    start_systick_fallback();
    for (;;) {
        if (!core1_stream_step(&stream)) {
            wait_for_work();
        }
    }
}
