#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "library/byte_source.h"
#include "library/library_builder.h"
#include "library/library_scan.h"

enum {
    TRACK_CACHE_MAGIC = 0x43544D45u,
    TRACK_CACHE_VERSION = 1,
    TRACK_CACHE_HEADER_SIZE = 32,
    TRACK_CACHE_RECORD_HEADER = 40,
    TRACK_CACHE_INDEX_ENTRY_SIZE = 8,
    TRACK_CACHE_MAX_ENTRIES = 8192,
    TRACK_FLAG_CACHE_HIT = 1
};

typedef struct TrackCacheIndexEntry {
    uint32_t path_hash;
    uint32_t offset;
} TrackCacheIndexEntry;

typedef struct CachedPicture {
    uint8_t kind;
    uint8_t front;
    uint32_t offset;
    uint32_t length;
} CachedPicture;

typedef struct CachedTrack {
    uint32_t size;
    uint32_t modified;
    uint32_t duration_ms;
    uint16_t year;
    uint8_t disc;
    uint8_t number;
    uint8_t flags;
    CachedPicture picture;
    const char *path;
    const char *title;
    const char *artist;
    const char *album_artist;
    const char *album;
    const char *genre;
} CachedTrack;

typedef struct TrackCacheReader {
    ByteSource source;
    uint32_t size;
    uint32_t count;
    TrackCacheIndexEntry *index;
    uint32_t index_capacity;
} TrackCacheReader;

enum {
    TRACK_CACHE_WRITE_BUFFER = 2048
};

typedef struct TrackCacheWriter {
    ByteSink sink;
    uint32_t capacity;
    uint32_t offset;
    uint32_t count;
    TrackCacheIndexEntry *index;
    uint32_t index_capacity;
    bool failed;
    uint32_t buffer_offset;
    uint32_t buffer_used;
    uint8_t buffer[TRACK_CACHE_WRITE_BUFFER];
} TrackCacheWriter;

bool track_cache_open(TrackCacheReader *reader, ByteSource source, uint32_t size,
                      TrackCacheIndexEntry *index_memory, uint32_t index_capacity);

bool track_cache_find(const TrackCacheReader *reader, const char *path, uint32_t size,
                      uint32_t modified, CachedTrack *track, char *text_buffer,
                      uint32_t text_capacity);

bool track_cache_find_picture(const TrackCacheReader *reader, const char *path,
                              CachedPicture *picture);

bool track_cache_find_flags(const TrackCacheReader *reader, const char *path, uint8_t *flags);

bool track_cache_find_picture_and_flags(const TrackCacheReader *reader, const char *path,
                                        CachedPicture *picture, uint8_t *flags);

void track_cache_writer_init(TrackCacheWriter *writer, ByteSink sink, uint32_t capacity,
                             TrackCacheIndexEntry *index_memory, uint32_t index_capacity);

bool track_cache_writer_put(TrackCacheWriter *writer, const CachedTrack *track);

bool track_cache_writer_finish(TrackCacheWriter *writer, uint32_t *bytes_written);
