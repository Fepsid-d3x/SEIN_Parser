/*
 . sein.h  —  Single-header SEIN config parser (C99)
 .
 . Usage:
 .   #define SEIN_IMPLEMENTATION   (in exactly one .c file)
 .   #include "sein.h"
*/

#ifndef SEIN_H
#define SEIN_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

//////////// Public types ////////////////

#define SEIN_MAX_SECTIONS      32
#define SEIN_MAX_KEYS          64
#define SEIN_MAX_KEY_LEN      128
#define SEIN_MAX_VAL_LEN      512
#define SEIN_MAX_SEC_LEN      128
#define SEIN_MAX_INCLUDE_DEPTH  8

typedef struct {
    char key[SEIN_MAX_KEY_LEN];
    char val[SEIN_MAX_VAL_LEN];
} SeinEntry;

typedef struct {
    char       name[SEIN_MAX_SEC_LEN];
    SeinEntry  entries[SEIN_MAX_KEYS];
    int        entry_count;
} SeinSection;

typedef struct {
    SeinSection sections[SEIN_MAX_SECTIONS];
    int         section_count;
} SeinConfig;


/////////////// Public API ////////////////

int sein_parse(SeinConfig *cfg, const char *path);

void sein_free(SeinConfig *cfg);

SeinConfig *sein_alloc(void);
void        sein_destroy(SeinConfig *cfg);

const char *sein_get(const SeinConfig *cfg, const char *section,
                     const char *key,   const char *fallback);

float sein_get_float(const SeinConfig *cfg, const char *section,
                     const char *key,   float fallback);

int   sein_get_int  (const SeinConfig *cfg, const char *section,
                     const char *key,   int   fallback);

int   sein_get_bool (const SeinConfig *cfg, const char *section,
                     const char *key,   int   fallback);

int sein_get_array      (const SeinConfig *cfg, const char *section,
                         const char *key, char delim,
                         char out[][SEIN_MAX_VAL_LEN], int max_out);

int sein_get_float_array(const SeinConfig *cfg, const char *section,
                         const char *key, char delim,
                         float *out, int max_out);

int sein_get_int_array  (const SeinConfig *cfg, const char *section,
                         const char *key, char delim,
                         int *out,   int max_out);

////////////////// IMPLEMENTATION ///////////////////
#ifdef SEIN_IMPLEMENTATION

//////////////// internal helpers //////////////////

static char *sein__trim_inplace(char *s)
{
    while (*s && isspace((unsigned char)*s)) ++s;
    if (*s == '\0') return s;
    char *end = s + strlen(s) - 1;
    while (end > s && isspace((unsigned char)*end)) --end;
    *(end + 1) = '\0';
    return s;
}

static void sein__strip_comment(char *line)
{
    char *p = strchr(line, '#');
    if (p) *p = '\0';
}

static size_t sein__strlen_safe(const char *s)
{
    return s ? strlen(s) : 0;
}

static char *sein__strip_quotes(char *val)
{
    if (!val) return val;
    size_t len = sein__strlen_safe(val);
    if (len >= 2 && val[0] == '"' && val[len - 1] == '"') {
        val[len - 1] = '\0';
        return val + 1;
    }
    return val;
}

static SeinSection *sein__get_or_create_section(SeinConfig *cfg, const char *name)
{
    for (int i = 0; i < cfg->section_count; ++i)
        if (strcmp(cfg->sections[i].name, name) == 0)
            return &cfg->sections[i];

    if (cfg->section_count >= SEIN_MAX_SECTIONS) {
        fprintf(stderr, "[SEIN Parser]: section limit reached\n");
        return NULL;
    }
    SeinSection *s = &cfg->sections[cfg->section_count++];
    strncpy(s->name, name, SEIN_MAX_SEC_LEN - 1);
    s->name[SEIN_MAX_SEC_LEN - 1] = '\0';
    s->entry_count = 0;
    return s;
}

static void sein__set(SeinSection *sec, const char *key, const char *val)
{
    size_t key_len = key ? strlen(key) : 0;
    size_t val_len = val ? strlen(val) : 0;
    
    /// update existing key (optimize with early termination) ///
    for (int i = 0; i < sec->entry_count; ++i) {
        if (key_len == strlen(sec->entries[i].key) && strcmp(sec->entries[i].key, key) == 0) {
            if (val_len < SEIN_MAX_VAL_LEN) {
                memcpy(sec->entries[i].val, val, val_len + 1);
            } else {
                memcpy(sec->entries[i].val, val, SEIN_MAX_VAL_LEN - 1);
                sec->entries[i].val[SEIN_MAX_VAL_LEN - 1] = '\0';
            }
            return;
        }
    }
    /// insert new key ///
    if (sec->entry_count >= SEIN_MAX_KEYS) {
        fprintf(stderr, "[SEIN Parser]: key limit reached in section '%s'\n", sec->name);
        return;
    }
    SeinEntry *e = &sec->entries[sec->entry_count++];
    size_t copy_key_len = key_len < SEIN_MAX_KEY_LEN ? key_len + 1 : SEIN_MAX_KEY_LEN;
    memcpy(e->key, key, copy_key_len - 1);
    e->key[copy_key_len - 1] = '\0';
    size_t copy_val_len = val_len < SEIN_MAX_VAL_LEN ? val_len + 1 : SEIN_MAX_VAL_LEN;
    memcpy(e->val, val, copy_val_len - 1);
    e->val[copy_val_len - 1] = '\0';
}

static void sein__substitute_value(char *val, const SeinConfig *cfg)
{
    if (!val || *val == '\0') return;
    char temp[SEIN_MAX_VAL_LEN];
    size_t src = 0, dst = 0;
    
    while (src < strlen(val) && dst < SEIN_MAX_VAL_LEN - 1) {
        if (val[src] == '$' && src + 1 < strlen(val) && val[src + 1] == '{') {
            char *brace_end = strchr(val + src + 2, '}');
            if (brace_end) {
                size_t var_len = (size_t)(brace_end - (val + src + 2));
                char var_name[256];
                if (var_len < sizeof(var_name)) {
                    memcpy(var_name, val + src + 2, var_len);
                    var_name[var_len] = '\0';
                    
                    char *dot = strchr(var_name, '.');
                    if (dot) {
                        *dot = '\0';
                        const char *val_ref = sein_get(cfg, var_name, dot + 1, NULL);
                        if (val_ref) {
                            size_t val_len = strlen(val_ref);
                            if (dst + val_len < SEIN_MAX_VAL_LEN) {
                                memcpy(temp + dst, val_ref, val_len);
                                dst += val_len;
                            }
                        }
                    } else {
                        const char *env_val = getenv(var_name);
                        if (env_val) {
                            size_t env_len = strlen(env_val);
                            if (dst + env_len < SEIN_MAX_VAL_LEN) {
                                memcpy(temp + dst, env_val, env_len);
                                dst += env_len;
                            }
                        }
                    }
                    src = (size_t)(brace_end - val) + 1;
                    continue;
                }
            }
        }
        if (dst < SEIN_MAX_VAL_LEN - 1) temp[dst++] = val[src];
        src++;
    }
    temp[dst] = '\0';
    strncpy(val, temp, SEIN_MAX_VAL_LEN - 1);
    val[SEIN_MAX_VAL_LEN - 1] = '\0';
}

static void sein__parse_file(SeinConfig *cfg, const char *path, int depth);

static void sein__parse_file(SeinConfig *cfg, const char *path, int depth)
{
    if (depth > SEIN_MAX_INCLUDE_DEPTH) {
        fprintf(stderr, "[SEIN Parser]: @include depth limit reached at: %s\n", path);
        return;
    }

    FILE *f = fopen(path, "r");
    if (!f) {
        fprintf(stderr, "[SEIN Parser]: Failed to open: %s\n", path);
        return;
    }

    /// derive base directory for relative @includes ///
    char base_dir[512] = "";
    const char *slash = strrchr(path, '/');
#ifdef _WIN32
    const char *bslash = strrchr(path, '\\');
    if (!slash || (bslash && bslash > slash)) slash = bslash;
#endif
    if (slash) {
        size_t len = (size_t)(slash - path) + 1;
        if (len < sizeof(base_dir)) {
            memcpy(base_dir, path, len);
            base_dir[len] = '\0';
        }
    }

    char current_section[SEIN_MAX_SEC_LEN] = "Default";
    char line[SEIN_MAX_VAL_LEN * 2];

    /// backslash-multiline state //
    char ml_key[SEIN_MAX_KEY_LEN] = "";
    char ml_val[SEIN_MAX_VAL_LEN] = "";
    int  in_multiline = 0;

    /// raw-string R(...)R state ///
    char raw_key[SEIN_MAX_KEY_LEN] = "";
    char raw_val[SEIN_MAX_VAL_LEN] = "";
    int  in_raw = 0;

    while (fgets(line, (int)sizeof(line), f)) {

        /// raw-string block ///
        if (in_raw) {
            char *end_pos = strstr(line, ")R\"");
            if (end_pos) {
                *end_pos = '\0';
                size_t cur = strlen(raw_val);
                strncat(raw_val, line, SEIN_MAX_VAL_LEN - cur - 1);

                SeinSection *sec = sein__get_or_create_section(cfg, current_section);
                if (sec) {
                    sein__substitute_value(raw_val, cfg);
                    sein__set(sec, raw_key, raw_val);
                }

                raw_key[0] = raw_val[0] = '\0';
                in_raw = 0;
            } else {
                size_t ll = strlen(line);
                if (ll > 0 && line[ll-1] == '\n') line[ll-1] = '\0';
                size_t cur = strlen(raw_val);
                size_t rem = SEIN_MAX_VAL_LEN - cur - 1;
                strncat(raw_val, line, rem > 0 ? rem - 1 : 0);
                cur = strlen(raw_val);
                if (cur < SEIN_MAX_VAL_LEN - 1) { raw_val[cur] = '\n'; raw_val[cur+1] = '\0'; }
            }
            continue;
        }

        /// backslash multiline continuation ///
        if (in_multiline) {
            sein__strip_comment(line);
            char *cl = sein__trim_inplace(line);
            int continues = (strlen(cl) > 0 && cl[strlen(cl) - 1] == '\\');
            if (continues) cl[strlen(cl) - 1] = '\0';
            cl = sein__trim_inplace(cl);

            size_t cur = strlen(ml_val);
            size_t rem = SEIN_MAX_VAL_LEN - cur - 1;
            if (cur < SEIN_MAX_VAL_LEN - 2) {
                ml_val[cur] = ' '; ml_val[cur + 1] = '\0';
                strncat(ml_val, cl, rem - 1);
            }

            if (!continues) {
                SeinSection *sec = sein__get_or_create_section(cfg, current_section);
                if (sec) {
                    char *v = sein__strip_quotes(sein__trim_inplace(ml_val));
                    sein__substitute_value(v, cfg);
                    sein__set(sec, ml_key, v);
                }
                ml_key[0] = ml_val[0] = '\0';
                in_multiline = 0;
            }
            continue;
        }

        sein__strip_comment(line);
        char *clean = sein__trim_inplace(line);
        if (!clean || *clean == '\0') continue;

        /// @include "other.sein" ///
        if (strncmp(clean, "@include", 8) == 0) {
            char *inc = sein__trim_inplace(clean + 8);
            inc = sein__strip_quotes(inc);
            char inc_path[512];
            snprintf(inc_path, sizeof(inc_path), "%s%s", base_dir, inc);
            sein__parse_file(cfg, inc_path, depth + 1);
            continue;
        }

        /// [Section] ///
        size_t clen = strlen(clean);
        if (clean[0] == '[' && clean[clen - 1] == ']') {
            clean[clen - 1] = '\0';
            char *sec_name = sein__trim_inplace(clean + 1);
            strncpy(current_section, sec_name, SEIN_MAX_SEC_LEN - 1);
            current_section[SEIN_MAX_SEC_LEN - 1] = '\0';
            continue;
        }

        /// key = value ///
        char *sep = strchr(clean, '=');
        if (!sep) continue;

        *sep = '\0';
        char *key = sein__trim_inplace(clean);
        char *val = sein__trim_inplace(sep + 1);

        /////////// ---- raw string: value starts with "R( ////////
        if (strncmp(val, "\"R(", 3) == 0) {
            char *end_pos = strstr(val + 3, ")R\"");
            if (end_pos) {
                *end_pos = '\0';
                SeinSection *sec = sein__get_or_create_section(cfg, current_section);
                if (sec) {
                    char temp[SEIN_MAX_VAL_LEN];
                    strncpy(temp, val + 3, SEIN_MAX_VAL_LEN - 1);
                    temp[SEIN_MAX_VAL_LEN - 1] = '\0';
                    sein__substitute_value(temp, cfg);
                    sein__set(sec, key, temp);
                }
            } else {
                strncpy(raw_key, key, SEIN_MAX_KEY_LEN - 1);
                raw_key[SEIN_MAX_KEY_LEN - 1] = '\0';
                strncpy(raw_val, val + 3, SEIN_MAX_VAL_LEN - 1);
                raw_val[SEIN_MAX_VAL_LEN - 1] = '\0';
                size_t cur = strlen(raw_val);
                if (cur < SEIN_MAX_VAL_LEN - 1) { raw_val[cur] = '\n'; raw_val[cur+1] = '\0'; }
                in_raw = 1;
            }
            continue;
        }

        size_t vlen = strlen(val);
        if (vlen > 0 && val[vlen - 1] == '\\') {
            val[vlen - 1] = '\0';
            strncpy(ml_key, key, SEIN_MAX_KEY_LEN - 1);
            ml_key[SEIN_MAX_KEY_LEN - 1] = '\0';
            strncpy(ml_val, sein__trim_inplace(val), SEIN_MAX_VAL_LEN - 1);
            ml_val[SEIN_MAX_VAL_LEN - 1] = '\0';
            in_multiline = 1;
            continue;
        }

        SeinSection *sec = sein__get_or_create_section(cfg, current_section);
        if (sec) {
            char *v = sein__strip_quotes(val);
            sein__substitute_value(v, cfg);
            sein__set(sec, key, v);
        }
    }

    fclose(f);
}

//////////////// public API implementation ////////////////

int sein_parse(SeinConfig *cfg, const char *path)
{
    memset(cfg, 0, sizeof(*cfg));
    sein__parse_file(cfg, path, 0);
    return 1;
}

void sein_free(SeinConfig *cfg)
{
    (void)cfg;
}

SeinConfig *sein_alloc(void)
{
    SeinConfig *cfg = (SeinConfig *)calloc(1, sizeof(SeinConfig));
    if (!cfg) fprintf(stderr, "[SEIN Parser]: out of memory\n");
    return cfg;
}

void sein_destroy(SeinConfig *cfg)
{
    free(cfg);
}

const char *sein_get(const SeinConfig *cfg, const char *section,
                     const char *key,   const char *fallback)
{
    for (int i = 0; i < cfg->section_count; ++i) {
        if (strcmp(cfg->sections[i].name, section) == 0) {
            const SeinSection *s = &cfg->sections[i];
            for (int j = 0; j < s->entry_count; ++j)
                if (strcmp(s->entries[j].key, key) == 0)
                    return s->entries[j].val;
            return fallback;
        }
    }
    return fallback;
}

float sein_get_float(const SeinConfig *cfg, const char *section,
                     const char *key,   float fallback)
{
    const char *v = sein_get(cfg, section, key, NULL);
    if (!v || *v == '\0') return fallback;
    char *end;
    float r = strtof(v, &end);
    return (end != v) ? r : fallback;
}

int sein_get_int(const SeinConfig *cfg, const char *section,
                 const char *key,   int fallback)
{
    const char *v = sein_get(cfg, section, key, NULL);
    if (!v || *v == '\0') return fallback;
    char *end;
    long r = strtol(v, &end, 10);
    return (end != v) ? (int)r : fallback;
}

int sein_get_bool(const SeinConfig *cfg, const char *section,
                  const char *key,   int fallback)
{
    const char *v = sein_get(cfg, section, key, NULL);
    if (!v || *v == '\0') return fallback;

    char lower[32];
    int i = 0;
    while (v[i] && i < 31) { lower[i] = (char)tolower((unsigned char)v[i]); ++i; }
    lower[i] = '\0';

    if (strcmp(lower, "true")  == 0 || strcmp(lower, "yes") == 0 || strcmp(lower, "1") == 0) return 1;
    if (strcmp(lower, "false") == 0 || strcmp(lower, "no")  == 0 || strcmp(lower, "0") == 0) return 0;
    return fallback;
}

//////////// array helpers ///////////////
static int sein__split(const char *val, char delim,
                       char out[][SEIN_MAX_VAL_LEN], int max_out)
{
    if (!val) return 0;
    int count = 0;
    const char *p = val;

    while (*p && count < max_out) {
        const char *q = p;
        while (*q && *q != delim) ++q;

        size_t len = (size_t)(q - p);
        if (len > SEIN_MAX_VAL_LEN - 1) len = SEIN_MAX_VAL_LEN - 1;
        if (len > 0) {
            memcpy(out[count], p, len);
            out[count][len] = '\0';

            char *tok = sein__trim_inplace(out[count]);
            if (tok != out[count]) {
                size_t tok_len = strlen(tok);
                memmove(out[count], tok, tok_len + 1);
            }

            if (out[count][0] != '\0') ++count;
        }

        if (*q == '\0') break;
        p = q + 1;
    }
    return count;
}

int sein_get_array(const SeinConfig *cfg, const char *section,
                   const char *key, char delim,
                   char out[][SEIN_MAX_VAL_LEN], int max_out)
{
    const char *v = sein_get(cfg, section, key, NULL);
    if (!v) return 0;
    return sein__split(v, delim, out, max_out);
}

int sein_get_float_array(const SeinConfig *cfg, const char *section,
                         const char *key, char delim,
                         float *out, int max_out)
{
    const char *v = sein_get(cfg, section, key, NULL);
    if (!v) return 0;
    int count = 0;
    const char *p = v;
    while (*p && count < max_out) {
        while (*p == ' ' || *p == '\t') ++p;
        if (*p == '\0') break;
        char *end;
        float r = strtof(p, &end);
        if (end == p) { while (*p && *p != delim) ++p; }
        else { out[count++] = r; p = end; }
        while (*p == ' ' || *p == '\t') ++p;
        if (*p == delim) ++p;
    }
    return count;
}

int sein_get_int_array(const SeinConfig *cfg, const char *section,
                       const char *key, char delim,
                       int *out, int max_out)
{
    const char *v = sein_get(cfg, section, key, NULL);
    if (!v) return 0;
    int count = 0;
    const char *p = v;
    while (*p && count < max_out) {
        while (*p == ' ' || *p == '\t') ++p;
        if (*p == '\0') break;
        char *end;
        long r = strtol(p, &end, 10);
        if (end == p) { while (*p && *p != delim) ++p; }
        else { out[count++] = (int)r; p = end; }
        while (*p == ' ' || *p == '\t') ++p;
        if (*p == delim) ++p;
    }
    return count;
}

#endif
#endif