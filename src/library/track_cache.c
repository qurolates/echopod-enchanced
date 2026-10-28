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
    uint8_t buffer[512];
    uint32_t index = 0;
    while (index < count) {
        uint32_t chunk_entries = count - index < 64 ? count - index : 64;
        uint32_t chunk_bytes = chunk_entries * TRACK_CACHE_INDEX_ENTRY_SIZE;
        if (!source.read(source.context, index_offset + index * TRACK_CACHE_INDEX_ENTRY_SIZE,
                         buffer, chunk_bytes)) {
            return false;
        }
        for (uint32_t i = 0; i < chunk_entries; i++) {
            index_memory[index + i].path_hash = get32(buffer + i * TRACK_CACHE_INDEX_ENTRY_SIZE);
            index_memory[index + i].offset = get32(buffer + i * TRACK_CACHE_INDEX_ENTRY_SIZE + 4);
        }
        index += chunk_entries;
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
        if (offset + TRACK_CACHE_RECORD_HEADER > reader->size) {
            continue;
        }
        uint8_t buffer[1024];
        uint32_t to_read =
            reader->size - offset < sizeof buffer ? reader->size - offset : (uint32_t)sizeof buffer;
        if (!reader->source.read(reader->source.context, offset, buffer, to_read)) {
            continue;
        }
        if (get32(buffer) != size || get32(buffer + 4) != modified) {
            continue;
        }
        uint16_t stored_path_len = get16(buffer + 26);
        if (stored_path_len != path_len ||
            (uint32_t)TRACK_CACHE_RECORD_HEADER + (uint32_t)stored_path_len > to_read) {
            continue;
        }
        if (!same_text((const char *)buffer + TRACK_CACHE_RECORD_HEADER, path, path_len)) {
            continue;
        }
        uint16_t title_len = get16(buffer + 28);
        uint16_t artist_len = get16(buffer + 30);
        uint16_t album_artist_len = get16(buffer + 32);
        uint16_t album_len = get16(buffer + 34);
        uint16_t genre_len = get16(buffer + 36);
        uint32_t total_strings =
            (uint32_t)title_len + (uint32_t)artist_len + (uint32_t)album_artist_len +
            (uint32_t)album_len + (uint32_t)genre_len + 5u;
        uint32_t record_len = (uint32_t)TRACK_CACHE_RECORD_HEADER + (uint32_t)stored_path_len +
                              (uint32_t)title_len + (uint32_t)artist_len +
                              (uint32_t)album_artist_len + (uint32_t)album_len + (uint32_t)genre_len;
        if (total_strings > text_capacity || offset + record_len > reader->size) {
            return false;
        }
        const uint8_t *payload = buffer;
        uint8_t overflow[2048];
        if (record_len > to_read) {
            if (record_len > sizeof overflow ||
                !reader->source.read(reader->source.context, offset, overflow, record_len)) {
                return false;
            }
            payload = overflow;
        }
        uint32_t text_offset = TRACK_CACHE_RECORD_HEADER + stored_path_len;
        char *cursor = text_buffer;

        track->title = cursor;
        for (uint32_t i = 0; i < title_len; i++) {
            cursor[i] = (char)payload[text_offset + i];
        }
        cursor[title_len] = '\0';
        cursor += title_len + 1;
        text_offset += title_len;

        track->artist = cursor;
        for (uint32_t i = 0; i < artist_len; i++) {
            cursor[i] = (char)payload[text_offset + i];
        }
        cursor[artist_len] = '\0';
        cursor += artist_len + 1;
        text_offset += artist_len;

        track->album_artist = cursor;
        for (uint32_t i = 0; i < album_artist_len; i++) {
            cursor[i] = (char)payload[text_offset + i];
        }
        cursor[album_artist_len] = '\0';
        cursor += album_artist_len + 1;
        text_offset += album_artist_len;

        track->album = cursor;
        for (uint32_t i = 0; i < album_len; i++) {
            cursor[i] = (char)payload[text_offset + i];
        }
        cursor[album_len] = '\0';
        cursor += album_len + 1;
        text_offset += album_len;

        track->genre = cursor;
        for (uint32_t i = 0; i < genre_len; i++) {
            cursor[i] = (char)payload[text_offset + i];
        }
        cursor[genre_len] = '\0';

        track->size = size;
        track->modified = modified;
        track->duration_ms = get32(buffer + 8);
        track->year = get16(buffer + 12);
        track->disc = buffer[14];
        track->number = buffer[15];
        track->flags = buffer[38];
        track->picture = (CachedPicture){.kind = buffer[16],
                                         .front = buffer[17],
                                         .offset = get32(buffer + 18),
                                         .length = get32(buffer + 22)};
        track->path = path;
        return true;
    }
    return false;
}

bool track_cache_find_picture_and_flags(const TrackCacheReader *reader, const char *path,
                                        CachedPicture *picture, uint8_t *flags)
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
        if (offset + TRACK_CACHE_RECORD_HEADER > reader->size) {
            continue;
        }
        uint8_t buffer[512];
        uint32_t to_read =
            reader->size - offset < sizeof buffer ? reader->size - offset : (uint32_t)sizeof buffer;
        if (!reader->source.read(reader->source.context, offset, buffer, to_read)) {
            continue;
        }
        uint16_t stored_path_len = get16(buffer + 26);
        if (stored_path_len != path_len ||
            (uint32_t)TRACK_CACHE_RECORD_HEADER + (uint32_t)stored_path_len > to_read) {
            continue;
        }
        if (!same_text((const char *)buffer + TRACK_CACHE_RECORD_HEADER, path, path_len)) {
            continue;
        }
        if (picture != NULL) {
            *picture = (CachedPicture){.kind = buffer[16],
                                       .front = buffer[17],
                                       .offset = get32(buffer + 18),
                                       .length = get32(buffer + 22)};
        }
        if (flags != NULL) {
            *flags = buffer[38];
        }
        return true;
    }
    return false;
}

bool track_cache_find_picture(const TrackCacheReader *reader, const char *path,
                              CachedPicture *picture)
{
    return track_cache_find_picture_and_flags(reader, path, picture, NULL);
}

bool track_cache_find_flags(const TrackCacheReader *reader, const char *path, uint8_t *flags)
{
    return track_cache_find_picture_and_flags(reader, path, NULL, flags);
}

static bool flush_writer(TrackCacheWriter *writer)
{
    if (writer->failed || writer->buffer_used == 0) {
        return !writer->failed;
    }
    if (!writer->sink.write(writer->sink.context, writer->buffer_offset, writer->buffer,
                            writer->buffer_used)) {
        writer->failed = true;
        return false;
    }
    writer->buffer_offset += writer->buffer_used;
    writer->buffer_used = 0;
    return true;
}

static bool writer_append(TrackCacheWriter *writer, const void *bytes, uint32_t length)
{
    const uint8_t *from = bytes;
    while (length > 0) {
        uint32_t room = sizeof(writer->buffer) - writer->buffer_used;
        uint32_t take = length < room ? length : room;
        for (uint32_t index = 0; index < take; index++) {
            writer->buffer[writer->buffer_used + index] = from[index];
        }
        writer->buffer_used += take;
        from += take;
        length -= take;
        if (writer->buffer_used == sizeof(writer->buffer) && !flush_writer(writer)) {
            return false;
        }
    }
    return true;
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
    writer->buffer_offset = TRACK_CACHE_HEADER_SIZE;
    writer->buffer_used = 0;
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

    if (!writer_append(writer, header, sizeof header)) {
        writer->failed = true;
        return false;
    }

    const char *strings[6] = {track->path,         track->title, track->artist,
                              track->album_artist, track->album, track->genre};
    uint32_t lengths[6] = {path_len, title_len, artist_len, album_artist_len, album_len, genre_len};
    for (int index = 0; index < 6; index++) {
        if (lengths[index] > 0 && !writer_append(writer, strings[index], lengths[index])) {
            writer->failed = true;
            return false;
        }
    }
    writer->index[writer->count].path_hash = path_hash_of(track->path);
    writer->index[writer->count].offset = writer->offset;
    writer->count++;
    writer->offset += record_len;
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
    if (!flush_writer(writer)) {
        return false;
    }
    sort_cache_index(writer->index, writer->count);
    uint32_t index_offset = writer->offset;
    writer->buffer_offset = index_offset;
    for (uint32_t index = 0; index < writer->count; index++) {
        uint8_t entry[TRACK_CACHE_INDEX_ENTRY_SIZE];
        put32(entry, writer->index[index].path_hash);
        put32(entry + 4, writer->index[index].offset);
        if (!writer_append(writer, entry, sizeof entry)) {
            writer->failed = true;
            return false;
        }
    }
    if (!flush_writer(writer)) {
        return false;
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
