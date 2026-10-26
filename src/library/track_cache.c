#include "library/track_cache.h"

#include <stddef.h>

#include "core/hash.h"

static uint16_t get16(const uint8_t *bytes)
{
    return (uint16_t)(bytes[0] | (uint16_t)bytes[1] << 8);
}

static uint32_t get32(const uint8_t *bytes)
{
    return (uint32_t)bytes[0] | (uint32_t)bytes[1] << 8 | (uint32_t)bytes[2] << 16 |
           (uint32_t)bytes[3] << 24;
}

static void put16(uint8_t *bytes, uint16_t value)
{
    bytes[0] = (uint8_t)value;
    bytes[1] = (uint8_t)(value >> 8);
}

static void put32(uint8_t *bytes, uint32_t value)
{
    put16(bytes, (uint16_t)value);
    put16(bytes + 2, (uint16_t)(value >> 16));
}

static uint32_t text_length(const char *text)
{
    uint32_t length = 0;
    while (text[length] != '\0') {
        length++;
    }
    return length;
}

static uint32_t path_hash_of(const char *path)
{
    return hash_update(HASH_START, path, text_length(path));
}

static bool same_text(const char *left, const char *right, uint32_t length)
{
    for (uint32_t index = 0; index < length; index++) {
        if (left[index] != right[index]) {
            return false;
        }
    }
    return true;
}

bool track_cache_open(TrackCacheReader *reader, ByteSource source, uint32_t size,
                      TrackCacheIndexEntry *index_memory, uint32_t index_capacity)
{
    reader->source = source;
    reader->size = size;
    reader->count = 0;
    reader->index = index_memory;
    reader->index_capacity = index_capacity;
    if (source.read == NULL || size < TRACK_CACHE_HEADER_SIZE || index_memory == NULL ||
        index_capacity == 0) {
        return false;
    }
    uint8_t header[TRACK_CACHE_HEADER_SIZE];
    if (!source.read(source.context, 0, header, sizeof header)) {
        return false;
    }
    if (get32(header) != TRACK_CACHE_MAGIC || get16(header + 4) != TRACK_CACHE_VERSION) {
        return false;
    }
    uint32_t count = get16(header + 6);
    uint32_t index_offset = get32(header + 8);
    uint32_t index_bytes = count * TRACK_CACHE_INDEX_ENTRY_SIZE;
    if (count > index_capacity || index_offset > size || index_bytes > size - index_offset) {
        return false;
    }
    for (uint32_t index = 0; index < count; index++) {
        uint8_t entry[TRACK_CACHE_INDEX_ENTRY_SIZE];
        if (!source.read(source.context, index_offset + index * TRACK_CACHE_INDEX_ENTRY_SIZE,
                         entry, sizeof entry)) {
            return false;
        }
        index_memory[index].path_hash = get32(entry);
        index_memory[index].offset = get32(entry + 4);
    }
    reader->count = count;
    return true;
}

static int32_t find_index_slot(const TrackCacheReader *reader, uint32_t path_hash)
{
    if (reader->count == 0 || reader->index == NULL) {
        return -1;
    }
    int32_t low = 0;
    int32_t high = (int32_t)reader->count - 1;
    while (low <= high) {
        int32_t mid = low + (high - low) / 2;
        uint32_t current = reader->index[mid].path_hash;
        if (current == path_hash) {
            while (mid > 0 && reader->index[mid - 1].path_hash == path_hash) {
                mid--;
            }
            return mid;
        }
        if (current < path_hash) {
            low = mid + 1;
        } else {
            high = mid - 1;
        }
    }
    return -1;
}

static bool read_record_header(const TrackCacheReader *reader, uint32_t offset, uint8_t *header)
{
    return offset + TRACK_CACHE_RECORD_HEADER <= reader->size &&
           reader->source.read(reader->source.context, offset, header,
                               TRACK_CACHE_RECORD_HEADER);
}

bool track_cache_find(const TrackCacheReader *reader, const char *path, uint32_t size,
                      uint32_t modified, CachedTrack *track, char *text_buffer,
                      uint32_t text_capacity)
{
    uint32_t hash = path_hash_of(path);
    int32_t slot = find_index_slot(reader, hash);
    if (slot < 0) {
        return false;
    }
    uint32_t path_len = text_length(path);
    for (uint32_t index = (uint32_t)slot;
         index < reader->count && reader->index[index].path_hash == hash; index++) {
        uint32_t offset = reader->index[index].offset;
        uint8_t header[TRACK_CACHE_RECORD_HEADER];
        if (!read_record_header(reader, offset, header)) {
            continue;
        }
        if (get32(header) != size || get32(header + 4) != modified) {
            continue;
        }
        uint16_t stored_path_len = get16(header + 26);
        if (stored_path_len != path_len) {
            continue;
        }
        uint32_t text_offset = offset + TRACK_CACHE_RECORD_HEADER;
        char disk_path[SCAN_PATH_CAPACITY];
        if (stored_path_len >= sizeof disk_path ||
            !reader->source.read(reader->source.context, text_offset, disk_path,
                                 stored_path_len)) {
            continue;
        }
        disk_path[stored_path_len] = '\0';
        if (!same_text(disk_path, path, path_len)) {
            continue;
        }
        uint16_t title_len = get16(header + 28);
        uint16_t artist_len = get16(header + 30);
        uint16_t album_artist_len = get16(header + 32);
        uint16_t album_len = get16(header + 34);
        uint16_t genre_len = get16(header + 36);
        uint32_t total_strings =
            (uint32_t)title_len + artist_len + album_artist_len + album_len + genre_len + 5u;
        if (total_strings > text_capacity ||
            text_offset + stored_path_len + total_strings > reader->size) {
            return false;
        }
        text_offset += stored_path_len;
        char *cursor = text_buffer;
        track->title = cursor;
        if (title_len > 0 &&
            !reader->source.read(reader->source.context, text_offset, cursor, title_len)) {
            return false;
        }
        cursor[title_len] = '\0';
        cursor += title_len + 1;
        text_offset += title_len;

        track->artist = cursor;
        if (artist_len > 0 &&
            !reader->source.read(reader->source.context, text_offset, cursor, artist_len)) {
            return false;
        }
        cursor[artist_len] = '\0';
        cursor += artist_len + 1;
        text_offset += artist_len;

        track->album_artist = cursor;
        if (album_artist_len > 0 &&
            !reader->source.read(reader->source.context, text_offset, cursor, album_artist_len)) {
            return false;
        }
        cursor[album_artist_len] = '\0';
        cursor += album_artist_len + 1;
        text_offset += album_artist_len;

        track->album = cursor;
        if (album_len > 0 &&
            !reader->source.read(reader->source.context, text_offset, cursor, album_len)) {
            return false;
        }
        cursor[album_len] = '\0';
        cursor += album_len + 1;
        text_offset += album_len;

        track->genre = cursor;
        if (genre_len > 0 &&
            !reader->source.read(reader->source.context, text_offset, cursor, genre_len)) {
            return false;
        }
        cursor[genre_len] = '\0';

        track->size = size;
        track->modified = modified;
        track->duration_ms = get32(header + 8);
        track->year = get16(header + 12);
        track->disc = header[14];
        track->number = header[15];
        track->flags = header[38];
        track->picture = (CachedPicture){.kind = header[16],
                                         .front = header[17],
                                         .offset = get32(header + 18),
                                         .length = get32(header + 22)};
        track->path = path;
        return true;
    }
    return false;
}

bool track_cache_find_picture(const TrackCacheReader *reader, const char *path,
                              CachedPicture *picture)
{
    uint32_t hash = path_hash_of(path);
    int32_t slot = find_index_slot(reader, hash);
    if (slot < 0) {
        return false;
    }
    uint32_t path_len = text_length(path);
    for (uint32_t index = (uint32_t)slot;
         index < reader->count && reader->index[index].path_hash == hash; index++) {
        uint32_t offset = reader->index[index].offset;
        uint8_t header[TRACK_CACHE_RECORD_HEADER];
        if (!read_record_header(reader, offset, header)) {
            continue;
        }
        uint16_t stored_path_len = get16(header + 26);
        if (stored_path_len != path_len) {
            continue;
        }
        char disk_path[SCAN_PATH_CAPACITY];
        if (stored_path_len >= sizeof disk_path ||
            offset + TRACK_CACHE_RECORD_HEADER + stored_path_len > reader->size ||
            !reader->source.read(reader->source.context, offset + TRACK_CACHE_RECORD_HEADER,
                                 disk_path, stored_path_len)) {
            continue;
        }
        disk_path[stored_path_len] = '\0';
        if (!same_text(disk_path, path, path_len)) {
            continue;
        }
        *picture = (CachedPicture){.kind = header[16],
                                   .front = header[17],
                                   .offset = get32(header + 18),
                                   .length = get32(header + 22)};
        return true;
    }
    return false;
}

bool track_cache_find_flags(const TrackCacheReader *reader, const char *path, uint8_t *flags)
{
    uint32_t hash = path_hash_of(path);
    int32_t slot = find_index_slot(reader, hash);
    if (slot < 0) {
        return false;
    }
    uint32_t path_len = text_length(path);
    for (uint32_t index = (uint32_t)slot;
         index < reader->count && reader->index[index].path_hash == hash; index++) {
        uint32_t offset = reader->index[index].offset;
        uint8_t header[TRACK_CACHE_RECORD_HEADER];
        if (!read_record_header(reader, offset, header)) {
            continue;
        }
        uint16_t stored_path_len = get16(header + 26);
        if (stored_path_len != path_len) {
            continue;
        }
        char disk_path[SCAN_PATH_CAPACITY];
        if (stored_path_len >= sizeof disk_path ||
            offset + TRACK_CACHE_RECORD_HEADER + stored_path_len > reader->size ||
            !reader->source.read(reader->source.context, offset + TRACK_CACHE_RECORD_HEADER,
                                 disk_path, stored_path_len)) {
            continue;
        }
        disk_path[stored_path_len] = '\0';
        if (!same_text(disk_path, path, path_len)) {
            continue;
        }
        *flags = header[38];
        return true;
    }
    return false;
}

void track_cache_writer_init(TrackCacheWriter *writer, ByteSink sink, uint32_t capacity,
                             TrackCacheIndexEntry *index_memory, uint32_t index_capacity)
{
    writer->sink = sink;
    writer->capacity = capacity;
    writer->offset = TRACK_CACHE_HEADER_SIZE;
    writer->count = 0;
    writer->index = index_memory;
    writer->index_capacity = index_capacity;
    writer->failed = sink.write == NULL || index_memory == NULL || index_capacity == 0;
    if (!writer->failed) {
        uint8_t blank[TRACK_CACHE_HEADER_SIZE] = {0};
        if (!writer->sink.write(writer->sink.context, 0, blank, sizeof blank)) {
            writer->failed = true;
        }
    }
}

bool track_cache_writer_put(TrackCacheWriter *writer, const CachedTrack *track)
{
    if (writer->failed || writer->count >= writer->index_capacity) {
        writer->failed = true;
        return false;
    }
    uint32_t path_len = text_length(track->path);
    uint32_t title_len = text_length(track->title);
    uint32_t artist_len = text_length(track->artist);
    uint32_t album_artist_len = text_length(track->album_artist);
    uint32_t album_len = text_length(track->album);
    uint32_t genre_len = text_length(track->genre);
    uint32_t record_len = TRACK_CACHE_RECORD_HEADER + path_len + title_len + artist_len +
                          album_artist_len + album_len + genre_len;
    if (writer->offset + record_len > writer->capacity) {
        writer->failed = true;
        return false;
    }
    uint8_t header[TRACK_CACHE_RECORD_HEADER];
    put32(header, track->size);
    put32(header + 4, track->modified);
    put32(header + 8, track->duration_ms);
    put16(header + 12, track->year);
    header[14] = track->disc;
    header[15] = track->number;
    header[16] = track->picture.kind;
    header[17] = track->picture.front;
    put32(header + 18, track->picture.offset);
    put32(header + 22, track->picture.length);
    put16(header + 26, (uint16_t)path_len);
    put16(header + 28, (uint16_t)title_len);
    put16(header + 30, (uint16_t)artist_len);
    put16(header + 32, (uint16_t)album_artist_len);
    put16(header + 34, (uint16_t)album_len);
    put16(header + 36, (uint16_t)genre_len);
    header[38] = track->flags;
    header[39] = 0;

    uint32_t offset = writer->offset;
    if (!writer->sink.write(writer->sink.context, offset, header, sizeof header)) {
        writer->failed = true;
        return false;
    }
    offset += sizeof header;

    const char *strings[6] = {track->path,         track->title, track->artist,
                              track->album_artist, track->album, track->genre};
    uint32_t lengths[6] = {path_len, title_len, artist_len, album_artist_len, album_len, genre_len};
    for (int index = 0; index < 6; index++) {
        if (lengths[index] > 0) {
            if (!writer->sink.write(writer->sink.context, offset, strings[index], lengths[index])) {
                writer->failed = true;
                return false;
            }
            offset += lengths[index];
        }
    }
    writer->index[writer->count].path_hash = path_hash_of(track->path);
    writer->index[writer->count].offset = writer->offset;
    writer->count++;
    writer->offset = offset;
    return true;
}

static void sort_cache_index(TrackCacheIndexEntry *entries, uint32_t count)
{
    for (uint32_t index = 1; index < count; index++) {
        TrackCacheIndexEntry entry = entries[index];
        uint32_t position = index;
        while (position > 0 && entries[position - 1].path_hash > entry.path_hash) {
            entries[position] = entries[position - 1];
            position--;
        }
        entries[position] = entry;
    }
}

bool track_cache_writer_finish(TrackCacheWriter *writer, uint32_t *bytes_written)
{
    if (writer->failed) {
        return false;
    }
    sort_cache_index(writer->index, writer->count);
    uint32_t index_offset = writer->offset;
    for (uint32_t index = 0; index < writer->count; index++) {
        uint8_t entry[TRACK_CACHE_INDEX_ENTRY_SIZE];
        put32(entry, writer->index[index].path_hash);
        put32(entry + 4, writer->index[index].offset);
        if (!writer->sink.write(writer->sink.context,
                                index_offset + index * TRACK_CACHE_INDEX_ENTRY_SIZE, entry,
                                sizeof entry)) {
            writer->failed = true;
            return false;
        }
    }
    uint32_t total_size = index_offset + writer->count * TRACK_CACHE_INDEX_ENTRY_SIZE;
    uint8_t header[TRACK_CACHE_HEADER_SIZE] = {0};
    put32(header, TRACK_CACHE_MAGIC);
    put16(header + 4, TRACK_CACHE_VERSION);
    put16(header + 6, (uint16_t)writer->count);
    put32(header + 8, index_offset);
    put32(header + 12, total_size);
    if (!writer->sink.write(writer->sink.context, 0, header, sizeof header)) {
        writer->failed = true;
        return false;
    }
    if (bytes_written != NULL) {
        *bytes_written = total_size;
    }
    return true;
}
