#include "language.h"
#include "fsdrive.h"
#include "fsutil.h"
#include "support.h"
#include "ui.h"

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

int compLanguage(const void* e1, const void* e2) {
    const Language* entry2 = (const Language*) e2;
    const Language* entry1 = (const Language*) e1;
    return strncasecmp(entry1->name, entry2->name, 32);
}

bool LanguageMenu(char* result, const char* title) {
    DirStruct* langDir = (DirStruct*)malloc(sizeof(DirStruct));
    if (!langDir) return false;

    char path[256];
    if (!GetSupportDir(path, LANGUAGES_DIR)) return false;
    GetDirContents(langDir, path);

    char* header = (char*)malloc(0x2C0);
    Language* langs = (Language*)malloc(langDir->n_entries * sizeof(Language));
    int langCount = 0;

    // Find all valid files and get their language names
    for (u32 i = 0; i < langDir->n_entries; i++) {
        if (langDir->entry[i].type == T_FILE) {
            size_t fsize = FileGetSize(langDir->entry[i].path);
            FileGetData(langDir->entry[i].path, header, 0x2C0, 0);
            if (GetLanguage(header, fsize, NULL, NULL, langs[langCount].name)) {
                memcpy(langs[langCount].path, langDir->entry[i].path, 256);
                langCount++;
            }
        }
    }

    free(langDir);
    free(header);

    qsort(langs, langCount, sizeof(Language), compLanguage);

    // Make an array of just the names for the select promt
    const char* langNames[langCount];
    for (int i = 0; i < langCount; i++) {
        langNames[i] = langs[i].name;
    }

    u32 selected = ShowSelectPrompt(langCount, langNames, "%s", title);
    if (selected > 0 && result) {
        memcpy(result, langs[selected - 1].path, 256);
    }

    return selected > 0;
}
