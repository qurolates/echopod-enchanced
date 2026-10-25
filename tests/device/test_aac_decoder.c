#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "check.h"
#include "device/aac_decoder.h"
#include "device/aac_probe.h"

typedef struct Fixture {
    uint8_t *bytes;
    uint32_t size;
} Fixture;

static Fixture fixture;

static void load(const char *rel_path)
{
    char full[512];
    snprintf(full, sizeof full, "%s/%s", ECHO_FIXTURE_DIR, rel_path);
    free(fixture.bytes);
    fixture = (Fixture){NULL, 0};
    FILE *file = fopen(full, "rb");
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

static void decode_adts_stream_to_completion(void)
{
    load("aac/adts_44k_stereo.aac");
    CHECK(fixture.size > 0);
    FlacInput input = {.read = read_fixture, .context = NULL, .size = fixture.size};
    AacDecoder decoder;
    aac_decoder_start(&decoder, input, false, 2, 44100);
    uint32_t frames = 0;
    uint32_t total_samples = 0;
    for (;;) {
        AacResult res = aac_decode(&decoder);
        if (res == AAC_FINISHED) {
            break;
        }
        CHECK_EQUAL(AAC_DECODED, res);
        uint32_t samples = aac_decoder_samples(&decoder);
        CHECK_EQUAL(1024u, samples);
        CHECK_EQUAL(2, aac_decoder_channels(&decoder));
        const int32_t *left = aac_decoder_channel(&decoder, 0);
        const int32_t *right = aac_decoder_channel(&decoder, 1);
        CHECK(left != NULL && right != NULL);
        frames++;
        total_samples += samples;
    }
    CHECK(frames > 40);
    CHECK(total_samples > 0);
    CHECK_EQUAL(fixture.size, aac_decoder_position(&decoder));
}

static void decode_mp4_raw_frames(void)
{
    load("tags/aac.m4a");
    CHECK(fixture.size > 0);
    FlacInput full_input = {.read = read_fixture, .context = NULL, .size = fixture.size};
    AacStream probe_stream;
    CHECK(aac_probe(full_input, &probe_stream));
    CHECK(probe_stream.is_mp4);

    FlacInput mdat_input = {
        .read = read_fixture,
        .context = NULL,
        .size = probe_stream.audio_end
    };
    AacDecoder decoder;
    aac_decoder_start(&decoder, mdat_input, true, probe_stream.channels, probe_stream.sample_rate);
    decoder.buffer_start = probe_stream.audio_start;
    decoder.buffered = 0;
    decoder.consumed_in_buffer = 0;

    uint32_t frames = 0;
    AacResult res = AAC_DECODED;
    for (;;) {
        res = aac_decode(&decoder);
        if (res == AAC_FINISHED || res == AAC_FAILED) {
            break;
        }
        CHECK_EQUAL(AAC_DECODED, res);
        CHECK_EQUAL(1024u, aac_decoder_samples(&decoder));
        frames++;
    }
    CHECK_EQUAL(AAC_FINISHED, res);
    CHECK(frames > 40);
}

static bool album_file_read(void *context, uint32_t offset, void *destination, uint32_t length)
{
    FILE *file = context;
    if (fseek(file, (long)offset, SEEK_SET) != 0) {
        return false;
    }
    return fread(destination, 1, length, file) == length;
}

static void decode_album_track_to_completion(void)
{
    const char *path =
        "/Users/qura/Documents/dev/echopod/test_audio/Acid Mammoth - Under Acid Hoof/01 - Them!.m4a";
    FILE *file = fopen(path, "rb");
    if (file == NULL) {
        return;
    }
    fseek(file, 0, SEEK_END);
    uint32_t size = (uint32_t)ftell(file);
    fseek(file, 0, SEEK_SET);
    FlacInput full = {.read = album_file_read, .context = file, .size = size};
    AacStream stream;
    CHECK(aac_probe(full, &stream));
    CHECK(stream.is_mp4);
    CHECK_EQUAL(44100u, stream.sample_rate);
    CHECK_EQUAL(2, stream.channels);
    CHECK_EQUAL(11983u * 1024u, stream.total_samples);

    FlacInput mdat = {.read = album_file_read, .context = file, .size = stream.audio_end};
    AacDecoder dec;
    aac_decoder_start(&dec, mdat, true, stream.channels, stream.sample_rate);
    dec.buffer_start = stream.audio_start;
    dec.buffered = 0;
    dec.consumed_in_buffer = 0;

    uint32_t frames = 0;
    AacResult res = AAC_DECODED;
    while ((res = aac_decode(&dec)) == AAC_DECODED) {
        CHECK_EQUAL(1024u, aac_decoder_samples(&dec));
        frames++;
    }
    CHECK_EQUAL(AAC_FINISHED, res);
    CHECK_EQUAL(11983u, frames);
    CHECK_EQUAL(stream.audio_end, aac_decoder_position(&dec));
    fclose(file);
}

static void seek_album_track_starts_at_chunk_boundary(void)
{
    const char *path =
        "/Users/qura/Documents/dev/echopod/test_audio/Acid Mammoth - Under Acid Hoof/01 - Them!.m4a";
    FILE *file = fopen(path, "rb");
    if (file == NULL) {
        return;
    }
    fseek(file, 0, SEEK_END);
    uint32_t size = (uint32_t)ftell(file);
    fseek(file, 0, SEEK_SET);
    FlacInput full = {.read = album_file_read, .context = file, .size = size};
    AacStream stream;
    CHECK(aac_probe(full, &stream));
    uint32_t actual = 0;
    uint32_t target_sample = 44100 * 30;
    uint32_t offset = aac_seek_offset(full, &stream, target_sample, &actual);
    CHECK(offset > stream.audio_start && offset < stream.audio_end);
    CHECK(actual > 0 && actual <= target_sample);

    FlacInput mdat = {.read = album_file_read, .context = file, .size = stream.audio_end};
    AacDecoder dec;
    aac_decoder_start(&dec, mdat, true, stream.channels, stream.sample_rate);
    dec.buffer_start = offset;
    dec.buffered = 0;
    dec.consumed_in_buffer = 0;

    for (int i = 0; i < 20; i++) {
        CHECK_EQUAL(AAC_DECODED, aac_decode(&dec));
        CHECK_EQUAL(1024u, aac_decoder_samples(&dec));
    }
    fclose(file);
}

static void corrupted_stream_fails_gracefully(void)
{
    uint8_t garbage[512];
    for (size_t i = 0; i < sizeof garbage; i++) {
        garbage[i] = (uint8_t)(i * 37 + 13);
    }
    free(fixture.bytes);
    fixture.bytes = garbage;
    fixture.size = sizeof garbage;
    FlacInput input = {.read = read_fixture, .context = NULL, .size = fixture.size};
    AacDecoder decoder;
    aac_decoder_start(&decoder, input, true, 2, 44100);
    AacResult res = aac_decode(&decoder);
    CHECK(res == AAC_FAILED || res == AAC_FINISHED);
    fixture.bytes = NULL;
    fixture.size = 0;
}

int main(void)
{
    decode_adts_stream_to_completion();
    decode_mp4_raw_frames();
    decode_album_track_to_completion();
    seek_album_track_starts_at_chunk_boundary();
    corrupted_stream_fails_gracefully();
    free(fixture.bytes);
    return 0;
}
