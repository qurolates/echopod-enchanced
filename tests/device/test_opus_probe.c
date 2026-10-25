#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "check.h"
#include "device/opus_probe.h"

typedef struct Fixture {
    uint8_t *bytes;
    uint32_t size;
} Fixture;

static Fixture fixture;

static void load(const char *path)
{
    free(fixture.bytes);
    fixture = (Fixture){NULL, 0};
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
    }
    fclose(file);
}

static bool read_fixture(void *context, uint32_t offset, void *destination, uint32_t length)
{
    (void)context;
    if (offset > fixture.size || length > fixture.size - offset) {
        return false;
    }
    memcpy(destination, fixture.bytes + offset, length);
    return true;
}

static bool probe(const char *rel_path, OpusStream *stream)
{
    char path[256];
    snprintf(path, sizeof path, "%s/%s", ECHO_FIXTURE_DIR, rel_path);
    load(path);
    if (fixture.bytes == NULL) {
        return false;
    }
    FlacInput input = {.read = read_fixture, .context = NULL, .size = fixture.size};
    return opus_probe(input, stream);
}

static void signature_recognises_opus(void)
{
    char path[256];
    snprintf(path, sizeof path, "%s/opus/stereo.opus", ECHO_FIXTURE_DIR);
    load(path);
    CHECK(fixture.size >= 36);
    CHECK(opus_signature(fixture.bytes, fixture.size));

    uint8_t non_opus[36] = {0};
    CHECK(!opus_signature(non_opus, sizeof non_opus));
}

static void probe_stereo_extracts_format(void)
{
    OpusStream stream;
    CHECK(probe("opus/stereo.opus", &stream));
    CHECK_EQUAL(48000u, stream.sample_rate);
    CHECK_EQUAL(2, stream.channels);
    CHECK(stream.total_samples > 0);
    CHECK_EQUAL(0u, stream.audio_start);
    CHECK(stream.audio_end > stream.audio_start);
}

static void probe_mono_extracts_format(void)
{
    OpusStream stream;
    CHECK(probe("opus/mono.opus", &stream));
    CHECK_EQUAL(48000u, stream.sample_rate);
    CHECK_EQUAL(1, stream.channels);
    CHECK(stream.total_samples > 0);
}

static void seeking_maps_linearly_between_bounds(void)
{
    OpusStream stream = {
        .sample_rate = 48000,
        .channels = 2,
        .pre_skip = 312,
        .total_samples = 48000,
        .audio_start = 0,
        .audio_end = 20000
    };
    FlacInput dummy = {NULL, NULL, 20000};
    uint32_t actual = 0;
    CHECK_EQUAL(0u, opus_seek_offset(dummy, &stream, 0, &actual));
    CHECK_EQUAL(0u, actual);
    CHECK_EQUAL(20000u, opus_seek_offset(dummy, &stream, 48000, &actual));
    CHECK_EQUAL(48000u, actual);
    uint32_t mid = opus_seek_offset(dummy, &stream, 24000, &actual);
    CHECK(mid > 0 && mid < 20000);
}

static void corrupted_or_truncated_file_fails_cleanly(void)
{
    uint8_t garbage[] = {0x12, 0x34, 0x56, 0x78};
    free(fixture.bytes);
    fixture.bytes = garbage;
    fixture.size = sizeof garbage;
    FlacInput input = {.read = read_fixture, .context = NULL, .size = fixture.size};
    OpusStream stream;
    CHECK(!opus_probe(input, &stream));
    fixture.bytes = NULL;
    fixture.size = 0;
}

int main(void)
{
    signature_recognises_opus();
    probe_stereo_extracts_format();
    probe_mono_extracts_format();
    seeking_maps_linearly_between_bounds();
    corrupted_or_truncated_file_fails_cleanly();
    free(fixture.bytes);
    return 0;
}
