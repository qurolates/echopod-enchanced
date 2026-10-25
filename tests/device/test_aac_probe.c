#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "check.h"
#include "device/aac_probe.h"

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

static bool probe(const char *rel_path, AacStream *stream)
{
    char full[512];
    snprintf(full, sizeof full, "%s/%s", ECHO_FIXTURE_DIR, rel_path);
    load(full);
    FlacInput input = {.read = read_fixture, .context = NULL, .size = fixture.size};
    return aac_probe(input, stream);
}

static void signature_recognises_adts_and_mp4(void)
{
    uint8_t adts[] = {0xFF, 0xF1, 0x50, 0x80, 0x00, 0x00, 0x00};
    CHECK(aac_signature(adts, sizeof adts));
    uint8_t mp4[] = {0x00, 0x00, 0x00, 0x1C, 'f', 't', 'y', 'p', 'M', '4', 'A', ' '};
    CHECK(aac_signature(mp4, sizeof mp4));
    uint8_t id3[] = {'I', 'D', '3', 0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
    CHECK(aac_signature(id3, sizeof id3));
    uint8_t flac[] = {'f', 'L', 'a', 'C', 0x00, 0x00, 0x00, 0x00};
    CHECK(!aac_signature(flac, sizeof flac));
    uint8_t zeros[12] = {0};
    CHECK(!aac_signature(zeros, sizeof zeros));
}

static void probe_adts_extracts_format(void)
{
    AacStream stream;
    CHECK(probe("aac/adts_44k_stereo.aac", &stream));
    CHECK(!stream.is_mp4);
    CHECK_EQUAL(44100u, stream.sample_rate);
    CHECK_EQUAL(2, stream.channels);
    CHECK_EQUAL(1024, stream.samples_per_frame);
    CHECK(stream.total_samples > 0);
    CHECK_EQUAL(0u, stream.audio_start);
    CHECK(stream.audio_end > stream.audio_start);
}

static void probe_mp4_extracts_format(void)
{
    AacStream stream;
    CHECK(probe("tags/aac.m4a", &stream));
    CHECK(stream.is_mp4);
    CHECK_EQUAL(44100u, stream.sample_rate);
    CHECK_EQUAL(2, stream.channels);
    CHECK_EQUAL(1024, stream.samples_per_frame);
    CHECK_EQUAL(45124u, stream.total_samples);
    CHECK(stream.audio_start > 0);
    CHECK(stream.audio_end > stream.audio_start);
}

static void seeking_maps_linearly_between_bounds(void)
{
    AacStream stream = {
        .sample_rate = 44100,
        .channels = 2,
        .samples_per_frame = 1024,
        .total_samples = 44100,
        .audio_start = 1000,
        .audio_end = 21000,
        .is_mp4 = false
    };
    FlacInput dummy = {NULL, NULL, 0};
    uint32_t actual = 0;
    CHECK_EQUAL(1000u, aac_seek_offset(dummy, &stream, 0, &actual));
    CHECK_EQUAL(0u, actual);
    CHECK_EQUAL(21000u, aac_seek_offset(dummy, &stream, 44100, &actual));
    CHECK_EQUAL(44100u, actual);
    uint32_t mid = aac_seek_offset(dummy, &stream, 22050, &actual);
    CHECK(mid > 1000 && mid < 21000);
}

static void corrupted_or_truncated_file_fails_cleanly(void)
{
    uint8_t garbage[] = {0x12, 0x34, 0x56, 0x78};
    free(fixture.bytes);
    fixture.bytes = garbage;
    fixture.size = sizeof garbage;
    FlacInput input = {.read = read_fixture, .context = NULL, .size = fixture.size};
    AacStream stream;
    CHECK(!aac_probe(input, &stream));
    fixture.bytes = NULL;
    fixture.size = 0;
}

int main(void)
{
    signature_recognises_adts_and_mp4();
    probe_adts_extracts_format();
    probe_mp4_extracts_format();
    seeking_maps_linearly_between_bounds();
    corrupted_or_truncated_file_fails_cleanly();
    free(fixture.bytes);
    return 0;
}
