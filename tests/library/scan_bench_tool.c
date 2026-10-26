#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "hal/storage.h"
#include "library/cover_build.h"
#include "library/library_scan.h"
#include "library/music_fingerprint.h"
#include "sim_storage.h"

enum {
    MEMORY_SIZE = 384 * 1024,
    LIBRARY_CAPACITY = 4 * 1024 * 1024,
    COVERS_CAPACITY = 8 * 1024 * 1024,
    CACHE_CAPACITY = 4 * 1024 * 1024,
    SCRATCH_CAPACITY = 32 * 1024 * 1024
};

typedef struct IoStats {
    uint32_t dir_opens;
    uint32_t dir_nexts;
    uint32_t file_opens;
    uint32_t file_reads;
    uint64_t file_bytes_read;
    uint32_t scratch_reads;
    uint64_t scratch_bytes_read;
    uint32_t scratch_writes;
    uint64_t scratch_bytes_written;
    uint32_t sink_writes;
    uint64_t sink_bytes_written;
} IoStats;

typedef struct HostFiles {
    HalDir dir;
    HalFile file;
    HalDirEntry entry;
    IoStats *stats;
    uint32_t modify_counter;
} HostFiles;

typedef struct Output {
    uint8_t *bytes;
    uint32_t capacity;
    uint32_t length;
    IoStats *stats;
} Output;

static uint64_t now_ns(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ull + (uint64_t)ts.tv_nsec;
}

static bool open_dir(void *context, const char *path)
{
    HostFiles *files = context;
    if (files->stats != NULL) {
        files->stats->dir_opens++;
    }
    return hal_dir_open(&files->dir, path);
}

static bool next_entry(void *context, ScanEntry *entry)
{
    HostFiles *files = context;
    if (files->stats != NULL) {
        files->stats->dir_nexts++;
    }
    if (!hal_dir_next(&files->dir, &files->entry)) {
        return false;
    }
    entry->directory = files->entry.directory;
    entry->size = files->entry.size;
    entry->modified = files->entry.modified;
    if (!files->entry.directory && files->modify_counter > 0) {
        entry->modified += 1000;
        files->modify_counter--;
    }
    memcpy(entry->name, files->entry.name, sizeof entry->name);
    return true;
}

static void close_dir(void *context)
{
    HostFiles *files = context;
    hal_dir_close(&files->dir);
}

static bool read_host(void *context, uint32_t offset, void *buffer, uint32_t length)
{
    HostFiles *files = context;
    if (files->stats != NULL) {
        files->stats->file_reads++;
        files->stats->file_bytes_read += length;
    }
    return hal_file_read(files->file, offset, buffer, length);
}

static bool open_file(void *context, const char *path, ByteSource *source, uint32_t *size)
{
    HostFiles *files = context;
    if (files->stats != NULL) {
        files->stats->file_opens++;
    }
    if (!hal_file_open(&files->file, path)) {
        return false;
    }
    *source = (ByteSource){.read = read_host, .context = files};
    *size = files->file.size;
    return true;
}

static void close_file(void *context)
{
    HostFiles *files = context;
    hal_file_close(&files->file);
}

static bool write_output(void *context, uint32_t offset, const void *bytes, uint32_t length)
{
    Output *output = context;
    if (output->stats != NULL) {
        output->stats->sink_writes++;
        output->stats->sink_bytes_written += length;
    }
    if (offset > output->capacity || length > output->capacity - offset) {
        return false;
    }
    memcpy(output->bytes + offset, bytes, length);
    output->length = offset + length > output->length ? offset + length : output->length;
    return true;
}

static bool read_output(void *context, uint32_t offset, void *buffer, uint32_t length)
{
    const Output *output = context;
    if (offset > output->capacity || length > output->capacity - offset) {
        return false;
    }
    memcpy(buffer, output->bytes + offset, length);
    return true;
}

static bool scratch_read_stat(void *context, uint32_t offset, void *buffer, uint32_t length)
{
    Output *scratch = context;
    if (scratch->stats != NULL) {
        scratch->stats->scratch_reads++;
        scratch->stats->scratch_bytes_read += length;
    }
    return read_output(scratch, offset, buffer, length);
}

static bool scratch_write_stat(void *context, uint32_t offset, const void *bytes, uint32_t length)
{
    Output *scratch = context;
    if (scratch->stats != NULL) {
        scratch->stats->scratch_writes++;
        scratch->stats->scratch_bytes_written += length;
    }
    return write_output(scratch, offset, bytes, length);
}

typedef struct FpState {
    HalDir dirs[FINGERPRINT_MAX_DEPTH];
    HalDirEntry entry;
    IoStats *stats;
} FpState;

static bool fp_open_dir(void *context, uint32_t level, const char *path)
{
    FpState *state = context;
    if (state->stats != NULL) {
        state->stats->dir_opens++;
    }
    return hal_dir_open(&state->dirs[level], path);
}

static bool fp_next_entry(void *context, uint32_t level, FingerprintEntry *entry)
{
    FpState *state = context;
    if (state->stats != NULL) {
        state->stats->dir_nexts++;
    }
    if (!hal_dir_next(&state->dirs[level], &state->entry)) {
        return false;
    }
    entry->directory = state->entry.directory;
    entry->size = state->entry.size;
    entry->modified = state->entry.modified;
    memcpy(entry->name, state->entry.name, sizeof entry->name);
    return true;
}

static void fp_close_dir(void *context, uint32_t level)
{
    FpState *state = context;
    hal_dir_close(&state->dirs[level]);
}

int main(int argc, char **argv)
{
    if (argc < 2) {
        fprintf(stderr, "usage: scan_bench_tool MUSIC_DIR\n");
        return 1;
    }
    const char *music_dir = argv[1];
    sim_storage_configure(music_dir, NULL);

    IoStats fp_stats = {0};
    FpState fp_state = {.stats = &fp_stats};
    FingerprintFiles fp_files = {fp_open_dir, fp_next_entry, fp_close_dir, &fp_state};
    FingerprintWork fp_work;

    uint64_t t0 = now_ns();
    MusicFingerprint fp = music_fingerprint(&fp_files, &fp_work);
    uint64_t t_fp = now_ns() - t0;

    printf("=== Fingerprint ===\n");
    printf("Time: %.3f ms\n", (double)t_fp / 1e6);
    printf("Complete: %d, entries: %u, hash: %08x / %08x\n", fp.complete, fp.entries, fp.first,
           fp.second);
    printf("Dir opens: %u, Dir nexts: %u\n\n", fp_stats.dir_opens, fp_stats.dir_nexts);

    static uint8_t memory[MEMORY_SIZE];
    IoStats scan_stats_io = {0};
    HostFiles files = {.stats = &scan_stats_io};
    ScanFileSystem system = {open_dir, next_entry, close_dir, open_file, close_file, &files};

    Output library = {calloc(LIBRARY_CAPACITY, 1), LIBRARY_CAPACITY, 0, &scan_stats_io};
    Output covers = {calloc(COVERS_CAPACITY, 1), COVERS_CAPACITY, 0, NULL};
    Output covers_b = {calloc(COVERS_CAPACITY, 1), COVERS_CAPACITY, 0, NULL};
    Output scratch = {calloc(SCRATCH_CAPACITY, 1), SCRATCH_CAPACITY, 0, &scan_stats_io};
    Output cache_a = {calloc(CACHE_CAPACITY, 1), CACHE_CAPACITY, 0, NULL};
    Output cache_b = {calloc(CACHE_CAPACITY, 1), CACHE_CAPACITY, 0, NULL};

    ScanJob scan = {.files = system,
                    .memory = memory,
                    .size = sizeof memory,
                    .scratch = {scratch_read_stat, scratch_write_stat, &scratch, SCRATCH_CAPACITY},
                    .sink = {write_output, &library},
                    .capacity = LIBRARY_CAPACITY,
                    .cache_source = {NULL, NULL},
                    .cache_size = 0,
                    .cache_sink = {write_output, &cache_a},
                    .cache_capacity = CACHE_CAPACITY};

    ScanStats stats;
    t0 = now_ns();
    ScanResult result = library_scan(&scan, &stats);
    uint64_t t_scan = now_ns() - t0;

    if (result != SCAN_OK) {
        fprintf(stderr, "scan failed: %d\n", result);
        return 2;
    }

    printf("=== Pass 1 (Cold Scan) ===\n");
    printf("Time: %.3f ms\n", (double)t_scan / 1e6);
    printf("Tracks scanned: %u, files: %u, skipped: %u, db size: %u, cache size: %u\n", stats.tracks,
           stats.files, stats.skipped, stats.length, cache_a.length);
    printf("Dir opens: %u, Dir nexts: %u\n", scan_stats_io.dir_opens, scan_stats_io.dir_nexts);
    printf("File opens: %u, File reads: %u, Bytes read: %llu KB\n", scan_stats_io.file_opens,
           scan_stats_io.file_reads, (unsigned long long)scan_stats_io.file_bytes_read / 1024);
    printf("Scratch reads: %u (%llu KB), Scratch writes: %u (%llu KB)\n",
           scan_stats_io.scratch_reads, (unsigned long long)scan_stats_io.scratch_bytes_read / 1024,
           scan_stats_io.scratch_writes,
           (unsigned long long)scan_stats_io.scratch_bytes_written / 1024);
    printf("DB writes: %u (%llu KB)\n\n", scan_stats_io.sink_writes,
           (unsigned long long)scan_stats_io.sink_bytes_written / 1024);

    IoStats cover_stats_io = {0};
    files.stats = &cover_stats_io;
    covers.stats = &cover_stats_io;

    CoverStats cstats;
    CoverJob cjob = {
        system,
        {read_output, &library},
        library.length,
        {write_output, &covers},
        COVERS_CAPACITY,
        memory,
        sizeof memory,
        {NULL, NULL},
        {read_output, &cache_a},
        cache_a.length,
        {NULL, NULL},
        0,
    };

    t0 = now_ns();
    result = cover_build(&cjob, &cstats);
    uint64_t t_covers = now_ns() - t0;

    if (result != SCAN_OK) {
        fprintf(stderr, "cover_build failed: %d\n", result);
        return 3;
    }

    printf("=== Pass 1 Cover Build ===\n");
    printf("Time: %.3f ms\n", (double)t_covers / 1e6);
    printf("Albums: %u, Covers built: %u, Missing: %u\n", cstats.albums, cstats.covers,
           cstats.missing);
    printf("Dir opens: %u, Dir nexts: %u\n", cover_stats_io.dir_opens, cover_stats_io.dir_nexts);
    printf("File opens: %u, File reads: %u, Bytes read: %llu KB\n", cover_stats_io.file_opens,
           cover_stats_io.file_reads, (unsigned long long)cover_stats_io.file_bytes_read / 1024);
    printf("Covers writes: %u (%llu KB)\n", cover_stats_io.sink_writes,
           (unsigned long long)cover_stats_io.sink_bytes_written / 1024);

    printf("\n=== TOTAL Pass 1 (Cold) ===\n");
    printf("Total time: %.3f ms\n\n", (double)(t_fp + t_scan + t_covers) / 1e6);

    library.length = 0;
    scratch.length = 0;
    memset(&scan_stats_io, 0, sizeof scan_stats_io);
    memset(&cover_stats_io, 0, sizeof cover_stats_io);
    files.stats = &scan_stats_io;

    scan.cache_source = (ByteSource){read_output, &cache_a};
    scan.cache_size = cache_a.length;
    scan.cache_sink = (ByteSink){write_output, &cache_b};
    scan.cache_capacity = CACHE_CAPACITY;

    t0 = now_ns();
    result = library_scan(&scan, &stats);
    uint64_t t_scan_warm = now_ns() - t0;

    printf("=== Pass 2 (Warm Rescan with Cache) ===\n");
    printf("Time: %.3f ms\n", (double)t_scan_warm / 1e6);
    printf("Tracks scanned: %u, files: %u, skipped: %u, db size: %u\n", stats.tracks, stats.files,
           stats.skipped, stats.length);
    printf("Dir opens: %u, Dir nexts: %u\n", scan_stats_io.dir_opens, scan_stats_io.dir_nexts);
    printf("File opens: %u, File reads: %u, Bytes read: %llu KB\n", scan_stats_io.file_opens,
           scan_stats_io.file_reads, (unsigned long long)scan_stats_io.file_bytes_read / 1024);
    printf("Scratch reads: %u (%llu KB), Scratch writes: %u (%llu KB)\n",
           scan_stats_io.scratch_reads, (unsigned long long)scan_stats_io.scratch_bytes_read / 1024,
           scan_stats_io.scratch_writes,
           (unsigned long long)scan_stats_io.scratch_bytes_written / 1024);
    printf("DB writes: %u (%llu KB)\n\n", scan_stats_io.sink_writes,
           (unsigned long long)scan_stats_io.sink_bytes_written / 1024);

    files.stats = &cover_stats_io;
    cjob.library_size = library.length;
    cjob.cache = (ByteSource){read_output, &cache_b};
    cjob.cache_size = cache_b.length;
    cjob.existing_covers = (ByteSource){read_output, &covers};
    cjob.existing_covers_size = covers.length;

    t0 = now_ns();
    result = cover_build(&cjob, &cstats);
    uint64_t t_covers_warm = now_ns() - t0;

    printf("=== Pass 2 Cover Build ===\n");
    printf("Time: %.3f ms\n", (double)t_covers_warm / 1e6);
    printf("Albums: %u, Covers built: %u, Missing: %u\n", cstats.albums, cstats.covers,
           cstats.missing);
    printf("Dir opens: %u, Dir nexts: %u\n", cover_stats_io.dir_opens, cover_stats_io.dir_nexts);
    printf("File opens: %u, File reads: %u, Bytes read: %llu KB\n", cover_stats_io.file_opens,
           cover_stats_io.file_reads, (unsigned long long)cover_stats_io.file_bytes_read / 1024);
    printf("Covers writes: %u (%llu KB)\n", cover_stats_io.sink_writes,
           (unsigned long long)cover_stats_io.sink_bytes_written / 1024);

    printf("\n=== TOTAL Pass 2 (Warm) ===\n");
    printf("Total time: %.3f ms\n\n", (double)(t_scan_warm + t_covers_warm) / 1e6);

    library.length = 0;
    scratch.length = 0;
    covers_b.length = 0;
    memset(&scan_stats_io, 0, sizeof scan_stats_io);
    memset(&cover_stats_io, 0, sizeof cover_stats_io);
    files.stats = &scan_stats_io;
    files.modify_counter = 3;

    scan.cache_source = (ByteSource){read_output, &cache_b};
    scan.cache_size = cache_b.length;
    scan.cache_sink = (ByteSink){write_output, &cache_a};
    scan.cache_capacity = CACHE_CAPACITY;

    t0 = now_ns();
    result = library_scan(&scan, &stats);
    uint64_t t_scan_mod = now_ns() - t0;

    printf("=== Pass 3 (Rescan with 3 Modified Tracks) ===\n");
    printf("Time: %.3f ms\n", (double)t_scan_mod / 1e6);
    printf("Tracks scanned: %u, files: %u, skipped: %u, db size: %u\n", stats.tracks, stats.files,
           stats.skipped, stats.length);
    printf("Dir opens: %u, Dir nexts: %u\n", scan_stats_io.dir_opens, scan_stats_io.dir_nexts);
    printf("File opens: %u, File reads: %u, Bytes read: %llu KB\n", scan_stats_io.file_opens,
           scan_stats_io.file_reads, (unsigned long long)scan_stats_io.file_bytes_read / 1024);
    printf("Scratch reads: %u (%llu KB), Scratch writes: %u (%llu KB)\n",
           scan_stats_io.scratch_reads, (unsigned long long)scan_stats_io.scratch_bytes_read / 1024,
           scan_stats_io.scratch_writes,
           (unsigned long long)scan_stats_io.scratch_bytes_written / 1024);
    printf("DB writes: %u (%llu KB)\n\n", scan_stats_io.sink_writes,
           (unsigned long long)scan_stats_io.sink_bytes_written / 1024);

    files.stats = &cover_stats_io;
    covers_b.stats = &cover_stats_io;
    cjob.library_size = library.length;
    cjob.sink = (ByteSink){write_output, &covers_b};
    cjob.cache = (ByteSource){read_output, &cache_a};
    cjob.cache_size = cache_a.length;
    cjob.existing_covers = (ByteSource){read_output, &covers};
    cjob.existing_covers_size = covers.length;

    t0 = now_ns();
    result = cover_build(&cjob, &cstats);
    uint64_t t_covers_mod = now_ns() - t0;

    printf("=== Pass 3 Cover Build ===\n");
    printf("Time: %.3f ms\n", (double)t_covers_mod / 1e6);
    printf("Albums: %u, Covers built: %u, Missing: %u\n", cstats.albums, cstats.covers,
           cstats.missing);
    printf("Dir opens: %u, Dir nexts: %u\n", cover_stats_io.dir_opens, cover_stats_io.dir_nexts);
    printf("File opens: %u, File reads: %u, Bytes read: %llu KB\n", cover_stats_io.file_opens,
           cover_stats_io.file_reads, (unsigned long long)cover_stats_io.file_bytes_read / 1024);
    printf("Covers writes: %u (%llu KB)\n", cover_stats_io.sink_writes,
           (unsigned long long)cover_stats_io.sink_bytes_written / 1024);

    printf("\n=== TOTAL Pass 3 (3 modified) ===\n");
    printf("Total time: %.3f ms\n", (double)(t_scan_mod + t_covers_mod) / 1e6);

    free(library.bytes);
    free(covers.bytes);
    free(covers_b.bytes);
    free(scratch.bytes);
    free(cache_a.bytes);
    free(cache_b.bytes);
    return 0;
}
