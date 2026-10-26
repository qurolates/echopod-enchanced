#include "library/library_scan.h"

#include <stddef.h>
#include <stdint.h>

#include "library/playlist_reader.h"
#include "library/track_cache.h"
#include "library/track_fields.h"
#include "library/track_tags.h"

enum {
    LISTING_SHARE = 8,
    MINIMUM_LISTING = 16 * 1024,
    MAXIMUM_LISTING = 96 * 1024,
    ALIGNMENT = 8,
    RESUME_BYTES = (2 + SCAN_NAME_CAPACITY + 3) & ~3,
    NO_RESUME = 0xFFFF,
    MINIMUM_BATCH = 2,
    ENTRY_OVERHEAD = 8,
    RECORD_META_SIZE = 9
};

typedef struct Level {
    uint32_t start;
    uint32_t first;
    uint32_t count;
    uint32_t next;
    uint32_t path_length;
    bool partial;
} Level;

typedef struct ScanState {
    LibraryBuilder builder;
    TrackTags tags;
    TrackFields fields;
    ScanEntry entry;
    Level levels[SCAN_MAX_DEPTH];
    uint32_t depth;
    char path[SCAN_PATH_CAPACITY + 2];
    uint32_t path_length;
    uint8_t *listing;
    uint32_t listing_size;
    uint32_t listing_used;
    uint32_t playlist_floor;
    uint32_t playlist_count;
    PlaylistReader playlist;
    char entry_text[PLAYLIST_ENTRY_CAPACITY];
    char playlist_name[SCAN_NAME_CAPACITY];
    char cutoff[SCAN_NAME_CAPACITY];
    TrackCacheReader cache_reader;
    TrackCacheWriter cache_writer;
    bool has_cache;
    bool has_writer;
    char cache_text[2048];
    TrackCacheIndexEntry *read_index;
    TrackCacheIndexEntry *write_index;
} ScanState;

typedef struct Listing {
    uint32_t *offsets;
    uint32_t *scratch;
} Listing;

static uint32_t text_length(const char *text)
{
    uint32_t length = 0;
    while (text[length] != '\0') {
        length++;
    }
    return length;
}

static const char *entry_name(const ScanState *state, uint32_t offset)
{
    return (const char *)state->listing + offset + RECORD_META_SIZE;
}

static bool entry_is_directory(const ScanState *state, uint32_t offset)
{
    return state->listing[offset] != 0;
}

static uint32_t entry_size(const ScanState *state, uint32_t offset)
{
    const uint8_t *p = state->listing + offset + 1;
    return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24;
}

static uint32_t entry_modified(const ScanState *state, uint32_t offset)
{
    const uint8_t *p = state->listing + offset + 5;
    return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24;
}

static uint32_t entry_record_length(const ScanState *state, uint32_t offset)
{
    return RECORD_META_SIZE + text_length(entry_name(state, offset)) + 1;
}

static int compare_texts(const char *left, const char *right)
{
    const unsigned char *a = (const unsigned char *)left;
    const unsigned char *b = (const unsigned char *)right;
    while (*a != '\0' && *a == *b) {
        a++;
        b++;
    }
    return (int)*a - (int)*b;
}

static int compare_names(const ScanState *state, uint32_t left, uint32_t right)
{
    return compare_texts(entry_name(state, left), entry_name(state, right));
}

static void merge_names(const ScanState *state, const uint32_t *from, uint32_t *to, uint32_t start,
                        uint32_t middle, uint32_t end)
{
    uint32_t left = start;
    uint32_t right = middle;
    for (uint32_t out = start; out < end; out++) {
        bool take_left =
            left < middle && (right >= end || compare_names(state, from[left], from[right]) <= 0);
        to[out] = take_left ? from[left++] : from[right++];
    }
}

static void sort_names(const ScanState *state, uint32_t *items, uint32_t *scratch, uint32_t count)
{
    uint32_t *from = items;
    uint32_t *to = scratch;
    for (uint32_t width = 1; width < count; width *= 2) {
        for (uint32_t start = 0; start < count; start += 2 * width) {
            uint32_t middle = start + width < count ? start + width : count;
            uint32_t end = start + 2 * width < count ? start + 2 * width : count;
            merge_names(state, from, to, start, middle, end);
        }
        uint32_t *swap = from;
        from = to;
        to = swap;
    }
    for (uint32_t index = 0; from != items && index < count; index++) {
        items[index] = from[index];
    }
}

static bool store_entry(ScanState *state, const ScanEntry *entry)
{
    uint32_t length = text_length(entry->name);
    uint32_t need = RECORD_META_SIZE + length + 1;
    if (need > state->playlist_floor - state->listing_used) {
        return false;
    }
    uint8_t *record = state->listing + state->listing_used;
    record[0] = entry->directory ? 1 : 0;
    record[1] = (uint8_t)entry->size;
    record[2] = (uint8_t)(entry->size >> 8);
    record[3] = (uint8_t)(entry->size >> 16);
    record[4] = (uint8_t)(entry->size >> 24);
    record[5] = (uint8_t)entry->modified;
    record[6] = (uint8_t)(entry->modified >> 8);
    record[7] = (uint8_t)(entry->modified >> 16);
    record[8] = (uint8_t)(entry->modified >> 24);
    for (uint32_t index = 0; index <= length; index++) {
        record[RECORD_META_SIZE + index] = (uint8_t)entry->name[index];
    }
    state->listing_used += need;
    return true;
}

static uint32_t align4(uint32_t value)
{
    return (value + 3) & ~3u;
}

static uint32_t *sorted_offsets(ScanState *state, uint32_t start, uint32_t count)
{
    uint32_t table = align4(state->listing_used);
    uint32_t *offsets = (uint32_t *)(void *)(state->listing + table);
    uint32_t cursor = start;
    for (uint32_t index = 0; index < count; index++) {
        offsets[index] = cursor;
        cursor += entry_record_length(state, cursor);
    }
    sort_names(state, offsets, offsets + count, count);
    return offsets;
}

static ScanResult index_listing(ScanState *state, uint32_t start, uint32_t count, Level *level)
{
    uint32_t table = align4(state->listing_used);
    if (table > state->playlist_floor || (uint64_t)count * 8 > state->playlist_floor - table) {
        return SCAN_FOLDER_TOO_BIG;
    }
    sorted_offsets(state, start, count);
    state->listing_used = table + count * 4;
    level->first = table;
    level->count = count;
    level->next = 0;
    level->path_length = state->path_length;
    return SCAN_OK;
}

static ScanResult read_whole(ScanState *state, const ScanFileSystem *files, Level *level,
                             bool *fits)
{
    if (!files->open_dir(files->context, state->path)) {
        return SCAN_NO_MUSIC_ROOT;
    }
    uint32_t start = state->listing_used;
    uint32_t count = 0;
    *fits = true;
    while (*fits && files->next_entry(files->context, &state->entry)) {
        if (state->entry.name[0] == '.') {
            continue;
        }
        *fits = store_entry(state, &state->entry) &&
                align4(state->listing_used) + (count + 1) * ENTRY_OVERHEAD <= state->playlist_floor;
        count++;
    }
    files->close_dir(files->context);
    if (!*fits) {
        state->listing_used = start;
        return SCAN_OK;
    }
    level->start = start;
    level->partial = false;
    return index_listing(state, start, count, level);
}

typedef struct Batch {
    uint32_t start;
    uint32_t end;
    uint32_t count;
    bool cut;
    const char *after;
} Batch;

static bool batch_fits(const ScanState *state, const Batch *batch, uint32_t length)
{
    uint64_t need = (uint64_t)align4(state->listing_used + length + 1 + RECORD_META_SIZE) +
                    (uint64_t)(batch->count + 1) * ENTRY_OVERHEAD;
    return need <= batch->end;
}

static void keep_smaller_half(ScanState *state, Batch *batch)
{
    uint32_t *offsets = sorted_offsets(state, batch->start, batch->count);
    uint32_t keep = batch->count / 2;
    const char *first_dropped = entry_name(state, offsets[keep]);
    uint32_t length = text_length(first_dropped);
    for (uint32_t index = 0; index <= length; index++) {
        state->cutoff[index] = first_dropped[index];
    }
    uint32_t write = batch->start;
    for (uint32_t read = batch->start; read < state->listing_used;) {
        uint32_t size = entry_record_length(state, read);
        if (compare_texts(entry_name(state, read), state->cutoff) < 0) {
            for (uint32_t index = 0; index < size; index++) {
                state->listing[write + index] = state->listing[read + index];
            }
            write += size;
        }
        read += size;
    }
    state->listing_used = write;
    batch->count = keep;
    batch->cut = true;
}

static bool wanted(const ScanState *state, const Batch *batch, const char *name)
{
    return name[0] != '.' && (batch->after == NULL || compare_texts(name, batch->after) > 0) &&
           (!batch->cut || compare_texts(name, state->cutoff) < 0);
}

static ScanResult add_to_batch(ScanState *state, Batch *batch)
{
    uint32_t length = text_length(state->entry.name);
    while (!batch_fits(state, batch, length)) {
        if (batch->count < MINIMUM_BATCH) {
            return SCAN_FOLDER_TOO_BIG;
        }
        keep_smaller_half(state, batch);
        if (compare_texts(state->entry.name, state->cutoff) >= 0) {
            return SCAN_OK;
        }
    }
    store_entry(state, &state->entry);
    batch->count++;
    return SCAN_OK;
}

static ScanResult read_batch(ScanState *state, const ScanFileSystem *files, Level *level,
                             uint32_t start, bool resume)
{
    uint8_t *header = state->listing + start;
    if (!resume) {
        header[0] = (uint8_t)NO_RESUME;
        header[1] = (uint8_t)(NO_RESUME >> 8);
    }
    bool has_after = (uint32_t)(header[0] | header[1] << 8) != NO_RESUME;
    Batch batch = {.start = start + RESUME_BYTES,
                   .end = start + (state->playlist_floor - start) / 2,
                   .count = 0,
                   .cut = false,
                   .after = has_after ? (const char *)header + 2 : NULL};
    if (batch.end < batch.start + 2 * (RESUME_BYTES + ENTRY_OVERHEAD)) {
        return SCAN_FOLDER_TOO_BIG;
    }
    if (!files->open_dir(files->context, state->path)) {
        return SCAN_NO_MUSIC_ROOT;
    }
    state->listing_used = batch.start;
    ScanResult result = SCAN_OK;
    while (result == SCAN_OK && files->next_entry(files->context, &state->entry)) {
        if (wanted(state, &batch, state->entry.name)) {
            result = add_to_batch(state, &batch);
        }
    }
    files->close_dir(files->context);
    if (result != SCAN_OK) {
        return result;
    }
    level->start = start;
    level->partial = batch.cut;
    return index_listing(state, batch.start, batch.count, level);
}

static ScanResult read_listing(ScanState *state, const ScanFileSystem *files, Level *level)
{
    bool fits = true;
    uint32_t start = state->listing_used;
    ScanResult result = read_whole(state, files, level, &fits);
    if (result != SCAN_OK || fits) {
        return result;
    }
    return read_batch(state, files, level, start, false);
}

static ScanResult read_next_batch(ScanState *state, const ScanFileSystem *files, Level *level)
{
    const uint32_t *offsets = (const uint32_t *)(const void *)(state->listing + level->first);
    const char *last = entry_name(state, offsets[level->count - 1]);
    uint32_t length = text_length(last);
    char *resume = (char *)state->listing + level->start + 2;
    for (uint32_t index = 0; index <= length; index++) {
        resume[index] = last[index];
    }
    state->listing[level->start] = (uint8_t)length;
    state->listing[level->start + 1] = (uint8_t)(length >> 8);
    return read_batch(state, files, level, level->start, true);
}

static bool append_path(ScanState *state, const char *name, bool directory)
{
    uint32_t length = text_length(name);
    if (state->path_length + length + 2 > SCAN_PATH_CAPACITY) {
        return false;
    }
    for (uint32_t index = 0; index < length; index++) {
        state->path[state->path_length + index] = name[index];
    }
    state->path_length += length;
    if (directory) {
        state->path[state->path_length++] = '/';
    }
    state->path[state->path_length] = '\0';
    return true;
}

static void truncate_path(ScanState *state, uint32_t length)
{
    state->path_length = length;
    state->path[length] = '\0';
}

static ScanResult builder_result(BuilderResult result)
{
    switch (result) {
    case BUILDER_OK:
        return SCAN_OK;
    case BUILDER_OUT_OF_MEMORY:
        return SCAN_OUT_OF_MEMORY;
    case BUILDER_TOO_MANY:
        return SCAN_TOO_MANY;
    case BUILDER_WRITE_FAILED:
        return SCAN_WRITE_FAILED;
    case BUILDER_NO_ROOM:
        return SCAN_NO_ROOM;
    }
    return SCAN_WRITE_FAILED;
}

static ScanResult add_file(ScanState *state, const ScanJob *job, ScanStats *stats, uint32_t size,
                           uint32_t modified)
{
    stats->files++;
    ScanResult result = SCAN_OK;
    CachedTrack cached;
    if (state->has_cache &&
        track_cache_find(&state->cache_reader, state->path + 1, size, modified, &cached,
                         state->cache_text, sizeof state->cache_text)) {
        BuilderTrack track = {
            .path = state->path + 1,
            .title = cached.title,
            .artist = cached.artist,
            .album_artist = cached.album_artist,
            .album = cached.album,
            .genre = cached.genre,
            .year = cached.year,
            .disc = cached.disc,
            .number = cached.number,
            .duration_ms = cached.duration_ms,
        };
        result = builder_result(library_builder_add_track(&state->builder, &track));
        stats->tracks += result == SCAN_OK;
        if (state->has_writer && result == SCAN_OK) {
            cached.flags = TRACK_FLAG_CACHE_HIT;
            track_cache_writer_put(&state->cache_writer, &cached);
        }
    } else {
        ByteSource source;
        uint32_t file_size = size;
        bool read = job->files.open_file(job->files.context, state->path, &source, &file_size);
        if (read) {
            TagFormat format = track_tags_format_for_name(state->path);
            read = track_tags_read(source, file_size, format, &state->tags);
            job->files.close_file(job->files.context);
        }
        if (read) {
            track_fields_from_tags(state->path + 1, &state->tags, &state->fields);
            result = builder_result(library_builder_add_track(&state->builder, &state->fields.track));
            stats->tracks += result == SCAN_OK;
            if (state->has_writer && result == SCAN_OK) {
                CachedTrack to_cache = {
                    .size = file_size,
                    .modified = modified,
                    .duration_ms = state->fields.track.duration_ms,
                    .year = state->fields.track.year,
                    .disc = state->fields.track.disc,
                    .number = state->fields.track.number,
                    .picture = (CachedPicture){
                        .kind = (uint8_t)state->tags.picture.kind,
                        .front = state->tags.picture.front ? 1 : 0,
                        .offset = state->tags.picture.offset,
                        .length = state->tags.picture.length,
                    },
                    .path = state->path + 1,
                    .title = state->fields.track.title,
                    .artist = state->fields.track.artist,
                    .album_artist = state->fields.track.album_artist,
                    .album = state->fields.track.album,
                    .genre = state->fields.track.genre,
                    .flags = 0,
                };
                track_cache_writer_put(&state->cache_writer, &to_cache);
            }
        } else {
            stats->skipped++;
        }
    }
    if (job->progress.report != NULL) {
        job->progress.report(job->progress.context, stats->files, stats->tracks);
    }
    return result;
}

static bool is_playlist_name(const char *name)
{
    uint32_t length = text_length(name);
    const char *dot = NULL;
    for (uint32_t index = 1; index < length; index++) {
        dot = name[index] == '.' ? name + index : dot;
    }
    if (dot == NULL) {
        return false;
    }
    char lower[6] = {0};
    uint32_t count = 0;
    for (const char *cursor = dot + 1; *cursor != '\0' && count < 5; cursor++) {
        lower[count++] = (char)(*cursor >= 'A' && *cursor <= 'Z' ? *cursor - 'A' + 'a' : *cursor);
    }
    bool m3u = count == 3 && lower[0] == 'm' && lower[1] == '3' && lower[2] == 'u';
    bool m3u8 =
        count == 4 && lower[0] == 'm' && lower[1] == '3' && lower[2] == 'u' && lower[3] == '8';
    return (m3u || m3u8) && dot[count + 1] == '\0';
}

static ScanResult note_playlist(ScanState *state, const char *name)
{
    uint32_t parent = state->path_length;
    uint32_t length = parent + text_length(name);
    uint32_t need = align4(length + 1 + 4);
    if (need > state->playlist_floor - align4(state->listing_used)) {
        return SCAN_FOLDER_TOO_BIG;
    }
    state->playlist_floor -= need;
    uint8_t *record = state->listing + state->playlist_floor;
    record[0] = (uint8_t)(length & 0xFF);
    record[1] = (uint8_t)(length >> 8);
    record[2] = (uint8_t)(parent & 0xFF);
    record[3] = (uint8_t)(parent >> 8);
    for (uint32_t index = 0; index < parent; index++) {
        record[4 + index] = (uint8_t)state->path[index];
    }
    for (uint32_t index = parent; index < length; index++) {
        record[4 + index] = (uint8_t)name[index - parent];
    }
    record[4 + length] = 0;
    state->playlist_count++;
    return SCAN_OK;
}

static ScanResult visit_entry(ScanState *state, const ScanJob *job, ScanStats *stats,
                              uint32_t offset)
{
    const char *name = entry_name(state, offset);
    bool directory = entry_is_directory(state, offset);
    if (!directory && is_playlist_name(name)) {
        return note_playlist(state, name);
    }
    if (!directory && track_tags_format_for_name(name) == TAG_FORMAT_UNKNOWN) {
        return SCAN_OK;
    }
    if (!append_path(state, name, directory)) {
        return SCAN_PATH_TOO_LONG;
    }
    if (!directory) {
        uint32_t size = entry_size(state, offset);
        uint32_t modified = entry_modified(state, offset);
        ScanResult result = add_file(state, job, stats, size, modified);
        truncate_path(state, state->levels[state->depth - 1].path_length);
        return result;
    }
    if (state->depth >= SCAN_MAX_DEPTH) {
        return SCAN_PATH_TOO_LONG;
    }
    ScanResult result = read_listing(state, &job->files, &state->levels[state->depth]);
    state->depth += result == SCAN_OK;
    return result == SCAN_NO_MUSIC_ROOT ? SCAN_OK : result;
}

static ScanResult walk(ScanState *state, const ScanJob *job, ScanStats *stats)
{
    ScanResult result = read_listing(state, &job->files, &state->levels[0]);
    state->depth = result == SCAN_OK ? 1 : 0;
    while (result == SCAN_OK && state->depth > 0) {
        Level *level = &state->levels[state->depth - 1];
        if (level->next >= level->count && level->partial && level->count > 0) {
            truncate_path(state, level->path_length);
            result = read_next_batch(state, &job->files, level);
            continue;
        }
        if (level->next >= level->count) {
            state->listing_used = level->start;
            state->depth--;
            truncate_path(state,
                          state->depth > 0 ? state->levels[state->depth - 1].path_length : 1);
            continue;
        }
        const uint32_t *offsets = (const uint32_t *)(const void *)(state->listing + level->first);
        result = visit_entry(state, job, stats, offsets[level->next++]);
    }
    return result;
}

static uint8_t *take(uint8_t **cursor, uint8_t *end, uint32_t bytes)
{
    uintptr_t address = (uintptr_t)*cursor;
    uint8_t *start = *cursor + (ALIGNMENT - address % ALIGNMENT) % ALIGNMENT;
    if (start > end || (uint32_t)(end - start) < bytes) {
        return NULL;
    }
    *cursor = start + bytes;
    return start;
}

static bool split_memory(ScanState **state, const ScanJob *job)
{
    uintptr_t start = (uintptr_t)job->memory;
    uint32_t skip = (uint32_t)((ALIGNMENT - start % ALIGNMENT) % ALIGNMENT);
    uint32_t header = (uint32_t)((sizeof(ScanState) + ALIGNMENT - 1) & ~(size_t)(ALIGNMENT - 1));
    bool separate = job->listing != NULL && job->listing_size >= MINIMUM_LISTING;
    if (job->size < skip + header + MINIMUM_LISTING * (separate ? 1u : 2u)) {
        return false;
    }
    uint8_t *memory = (uint8_t *)job->memory + skip;
    uint8_t *cursor = memory + header;
    uint8_t *end = (uint8_t *)job->memory + job->size;

    *state = (ScanState *)(void *)memory;
    (*state)->read_index = NULL;
    if (job->cache_source.read != NULL) {
        (*state)->read_index = (TrackCacheIndexEntry *)take(
            &cursor, end, TRACK_CACHE_MAX_ENTRIES * (uint32_t)sizeof(TrackCacheIndexEntry));
    }
    (*state)->write_index = NULL;
    if (job->cache_sink.write != NULL) {
        (*state)->write_index = (TrackCacheIndexEntry *)take(
            &cursor, end, TRACK_CACHE_MAX_ENTRIES * (uint32_t)sizeof(TrackCacheIndexEntry));
    }

    uint32_t remaining = cursor < end ? (uint32_t)(end - cursor) : 0;
    uint32_t listing = separate ? job->listing_size : remaining / LISTING_SHARE;
    listing = listing < MINIMUM_LISTING ? MINIMUM_LISTING : listing;
    listing = listing > MAXIMUM_LISTING ? MAXIMUM_LISTING : listing;
    uint32_t taken = separate ? 0 : listing;
    if (remaining < taken + MINIMUM_LISTING) {
        return false;
    }
    (*state)->listing = separate ? (uint8_t *)job->listing : cursor;
    (*state)->listing_size = listing;
    (*state)->listing_used = 0;
    (*state)->playlist_floor = listing;
    (*state)->playlist_count = 0;
    library_builder_init(&(*state)->builder, cursor + taken, remaining - taken, job->scratch);
    return true;
}

static void name_and_folder(ScanState *state, const uint8_t *record, char *folder)
{
    uint32_t length = (uint32_t)record[0] | (uint32_t)record[1] << 8;
    uint32_t parent = (uint32_t)record[2] | (uint32_t)record[3] << 8;
    const char *path = (const char *)record + 4;
    uint32_t folder_length = parent > 1 ? parent - 2 : 0;
    for (uint32_t index = 0; index < folder_length; index++) {
        folder[index] = path[index + 1];
    }
    folder[folder_length] = '\0';
    uint32_t stem_end = length;
    for (uint32_t index = parent + 1; index < length; index++) {
        stem_end = path[index] == '.' ? index : stem_end;
    }
    uint32_t count = 0;
    for (uint32_t index = parent; index < stem_end; index++) {
        state->playlist_name[count++] = path[index];
    }
    state->playlist_name[count] = '\0';
}

static ScanResult read_playlist(ScanState *state, const ScanJob *job, const uint8_t *record)
{
    ByteSource source;
    uint32_t size = 0;
    const char *path = (const char *)record + 4;
    if (!job->files.open_file(job->files.context, path, &source, &size)) {
        return SCAN_OK;
    }
    name_and_folder(state, record, state->path);
    ScanResult result = builder_result(
        library_builder_begin_playlist(&state->builder, state->playlist_name, state->path));
    playlist_reader_open(&state->playlist, source, size);
    while (result == SCAN_OK &&
           playlist_reader_next(&state->playlist, state->entry_text, sizeof state->entry_text)) {
        result = builder_result(library_builder_playlist_entry(&state->builder, state->entry_text));
    }
    job->files.close_file(job->files.context);
    return result;
}

static uint32_t record_length(const ScanState *state, uint32_t offset)
{
    uint32_t length = (uint32_t)state->listing[offset] | (uint32_t)state->listing[offset + 1] << 8;
    return align4(length + 1 + 4);
}

static uint32_t noted_playlist(const ScanState *state, uint32_t index)
{
    uint32_t offset = state->playlist_floor;
    for (uint32_t skip = 0; skip + 1 + index < state->playlist_count; skip++) {
        offset += record_length(state, offset);
    }
    return offset;
}

static ScanResult read_playlists(ScanState *state, const ScanJob *job)
{
    ScanResult result = builder_result(library_builder_prepare(&state->builder));
    for (uint32_t index = 0; result == SCAN_OK && index < state->playlist_count; index++) {
        result = read_playlist(state, job, state->listing + noted_playlist(state, index));
    }
    return result;
}

ScanResult library_scan(const ScanJob *job, ScanStats *stats)
{
    *stats = (ScanStats){.files = 0, .tracks = 0, .skipped = 0, .length = 0};
    ScanState *state = NULL;
    if (!split_memory(&state, job)) {
        return SCAN_OUT_OF_MEMORY;
    }
    state->has_cache =
        state->read_index != NULL &&
        track_cache_open(&state->cache_reader, job->cache_source, job->cache_size,
                         state->read_index, TRACK_CACHE_MAX_ENTRIES);
    state->has_writer = job->cache_sink.write != NULL && state->write_index != NULL;
    if (state->has_writer) {
        track_cache_writer_init(&state->cache_writer, job->cache_sink, job->cache_capacity,
                                state->write_index, TRACK_CACHE_MAX_ENTRIES);
    }
    state->path[0] = '/';
    truncate_path(state, 1);
    ScanResult result = walk(state, job, stats);
    if (state->has_writer && result == SCAN_OK) {
        uint32_t written = 0;
        track_cache_writer_finish(&state->cache_writer, &written);
    }
    if (result == SCAN_OK) {
        result = read_playlists(state, job);
    }
    if (result != SCAN_OK) {
        return result;
    }
    return builder_result(
        library_builder_finish(&state->builder, job->sink, job->capacity, &stats->length));
}
