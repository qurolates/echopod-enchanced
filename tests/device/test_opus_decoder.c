#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "check.h"
#include "device/opus_decoder.h"
#include "device/opus_probe.h"

typedef struct Fixture {
    uint8_t *bytes;
    uint32_t size;
    uint32_t limit;
} Fixture;

static Fixture fixture;

static void load(const char *rel_path)
{
    free(fixture.bytes);
    fixture = (Fixture){NULL, 0, 0};
    char path[256];
    snprintf(path, sizeof path, "%s/%s", ECHO_FIXTURE_DIR, rel_path);
    FILE *file = fopen(path, "rb");
    if (file == NULL) {
        return;
    }
    fseek(file, 0, SEEK_END);
    long size = ftell(file);
    fseek(file, 0, SEEK_SET);
    fixture.bytes = malloc((size_t)size);
    if (fixture.bytes != NULL && fread(fixture.bytes, 1, (size_t)size, file) == (size_t)size) {
        fixture.size = (uint32_t)size;
        fixture.limit = (uint32_t)size;
    }
    fclose(file);
}

static bool read_fixture(void *context, uint32_t offset, void *destination, uint32_t length)
{
    (void)context;
    uint32_t effective_limit = fixture.limit < fixture.size ? fixture.limit : fixture.size;
    if (offset > effective_limit || length > effective_limit - offset) {
        return false;
    }
    memcpy(destination, fixture.bytes + offset, length);
    return true;
}

static void decode_stereo_to_completion(void)
{
    load("opus/stereo.opus");
    CHECK(fixture.size > 0);
    FlacInput input = {.read = read_fixture, .context = NULL, .size = fixture.size};
    OpusStream stream;
    CHECK(opus_probe(input, &stream));

    OpusDecoderContext decoder;
    opus_decoder_start(&decoder, input, stream.channels, stream.sample_rate);

    uint32_t total_decoded = 0;
    uint32_t frames_count = 0;
    for (;;) {
        OpusResult res = opus_decoder_step(&decoder);
        if (res == OPUS_FINISHED) {
            break;
        }
        CHECK_EQUAL((int)OPUS_DECODED, (int)res);
        uint32_t samples = opus_decoder_samples(&decoder);
        CHECK(samples > 0);
        total_decoded += samples;
        frames_count++;
    }
    CHECK(frames_count > 10);
    CHECK(total_decoded > 40000);
}

static void decode_mono_to_completion(void)
{
    load("opus/mono.opus");
    CHECK(fixture.size > 0);
    FlacInput input = {.read = read_fixture, .context = NULL, .size = fixture.size};
    OpusStream stream;
    CHECK(opus_probe(input, &stream));
    CHECK_EQUAL(1, stream.channels);

    OpusDecoderContext decoder;
    opus_decoder_start(&decoder, input, stream.channels, stream.sample_rate);

    uint32_t total_decoded = 0;
    for (;;) {
        OpusResult res = opus_decoder_step(&decoder);
        if (res == OPUS_FINISHED) {
            break;
        }
        CHECK_EQUAL((int)OPUS_DECODED, (int)res);
        total_decoded += opus_decoder_samples(&decoder);
    }
    CHECK(total_decoded > 40000);
}

static void buffer_refill_during_packet_assembly(void)
{
    load("opus/stereo.opus");
    CHECK(fixture.size > 0);
    FlacInput input = {.read = read_fixture, .context = NULL, .size = fixture.size};
    OpusStream stream;
    CHECK(opus_probe(input, &stream));

    OpusDecoderContext decoder;
    opus_decoder_start(&decoder, input, stream.channels, stream.sample_rate);

    fixture.limit = 100;
    uint32_t starved_count = 0;
    uint32_t total_decoded = 0;

    while (fixture.limit < fixture.size) {
        OpusResult res = opus_decoder_step(&decoder);
        if (res == OPUS_STARVED) {
            starved_count++;
            fixture.limit += 150;
            if (fixture.limit > fixture.size) {
                fixture.limit = fixture.size;
            }
            continue;
        }
        if (res == OPUS_FINISHED) {
            break;
        }
        CHECK_EQUAL((int)OPUS_DECODED, (int)res);
        total_decoded += opus_decoder_samples(&decoder);
    }
    while (opus_decoder_step(&decoder) == OPUS_DECODED) {
        total_decoded += opus_decoder_samples(&decoder);
    }
    CHECK(starved_count > 5);
    CHECK(total_decoded > 40000);
}

int main(void)
{
    decode_stereo_to_completion();
    decode_mono_to_completion();
    buffer_refill_during_packet_assembly();
    free(fixture.bytes);
    return 0;
}
