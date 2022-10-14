#include "language.h"

#define STRING(what, def) const char* STR_##what = NULL;
#include "language.inl"
#undef STRING

static const char** translation_ptrs[] = {
    #define STRING(what, def) &STR_##what,
    #include "language.inl"
    #undef STRING
};

static const char* translation_fallbacks[] = {
    #define STRING(what, def) def,
    #include "language.inl"
    #undef STRING
};

static char* translation_data = NULL;

typedef struct {
    char name[32];
    char path[256];
} Language;

typedef struct {
    char chunk_id[4]; // NOT null terminated
    u32 size;
} RiffChunkHeader;

typedef struct {
    u32 version;
    u32 count;
    char languageName[32];
} LanguageMeta;
STATIC_ASSERT(sizeof(LanguageMeta) == 40);

bool SetLanguage(const void* translation, u32 translation_size) {
    u32 str_count;
    const void* ptr = translation;
    const RiffChunkHeader* riff_header;
    const RiffChunkHeader* chunk_header;

    // Free old translation data
    if (translation_data) {
        free(translation_data);
        translation_data = NULL;
    }

    if ((ptr = GetLanguage(translation, translation_size, NULL, &str_count, NULL))) {
        // load total size
        riff_header = translation;

        while ((u32)(ptr - translation) < riff_header->size + sizeof(RiffChunkHeader)) {
            chunk_header = ptr;

            if (memcmp(chunk_header->chunk_id, "SDAT", 4) == 0) { // string data
                if (chunk_header->size > 0) {
                    translation_data = malloc(chunk_header->size);
                    if (!translation_data) goto fallback;

                    memcpy(translation_data, ptr + sizeof(RiffChunkHeader), chunk_header->size);
                }
            } else if (memcmp(chunk_header->chunk_id, "SMAP", 4) == 0) { // string map
                // string data must come before the map
                if (!translation_data && str_count > 0) goto fallback;

                u16* string_map = (u16*)(ptr + sizeof(RiffChunkHeader));

                // Load all the strings
                for (u32 i = 0; i < countof(translation_ptrs); i++) {
                    if (i < str_count) {
                        *translation_ptrs[i] = (translation_data + string_map[i]);
                    } else {
                        *translation_ptrs[i] = translation_fallbacks[i];
                    }
                }
            }

            ptr += sizeof(RiffChunkHeader) + chunk_header->size;
        }

        return true;
    }

fallback:
    if (translation_data) {
        free(translation_data);
        translation_data = NULL;
    }

    for (u32 i = 0; i < countof(translation_ptrs); i++) {
        *translation_ptrs[i] = translation_fallbacks[i];
    }

    return false;
}

const void* GetLanguage(const void* riff, const u32 riff_size, u32* version, u32* count, char* language_name) {
    const void* ptr = riff;
    const RiffChunkHeader* riff_header;
    const RiffChunkHeader* chunk_header;

    // check header magic and load size
    if (!ptr) return NULL;
    riff_header = ptr;
    if (memcmp(riff_header->chunk_id, "RIFF", 4) != 0) return NULL;

    // ensure enough space is allocated
    if (riff_header->size > riff_size) return NULL;

    ptr += sizeof(RiffChunkHeader);

    while ((u32)(ptr - riff) < riff_header->size + sizeof(RiffChunkHeader)) {
        chunk_header = ptr;

        // check for and load META section
        if (memcmp(chunk_header->chunk_id, "META", 4) == 0) {
            if (chunk_header->size != sizeof(LanguageMeta)) return NULL;

            const LanguageMeta *meta = ptr + sizeof(RiffChunkHeader);
            if (meta->version != TRANSLATION_VER || meta->count > countof(translation_ptrs)) return NULL;

            // all good
            if (version) *version = meta->version;
            if (count) *count = meta->count;
            if (language_name) strcpy(language_name, meta->languageName);
            return ptr;
        }

        ptr += sizeof(RiffChunkHeader) + chunk_header->size;
    }

    return NULL;
}
