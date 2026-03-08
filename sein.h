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
#define SEIN_MAX_VARS          64
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
    SeinEntry   vars[SEIN_MAX_VARS];   /// @set variables ///
    int         var_count;
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

///////////////////////////////////////////////////////////////////////////
//                       Writer API — types                              //
///////////////////////////////////////////////////////////////////////////

#define SEIN_MAX_DIRECTIVES   32
#define SEIN_MAX_DOC_SECTIONS 32
#define SEIN_MAX_DOC_KEYS     64
#define SEIN_MAX_COMMENT_LEN  128
#define SEIN_MAX_PATH_LEN     512

typedef enum {
    SEIN_DIR_BLANK   = 0,
    SEIN_DIR_COMMENT = 1,
    SEIN_DIR_INCLUDE = 2,
    SEIN_DIR_SET     = 3
} SeinDirectiveKind;

typedef struct {
    SeinDirectiveKind kind;
    char operand[SEIN_MAX_KEY_LEN];  /// include path or @set name or comment text ///
    char value  [SEIN_MAX_VAL_LEN];  /// @set value ///
    char comment[SEIN_MAX_COMMENT_LEN]; /// inline comment ///
} SeinDirective;

typedef struct {
    char      key    [SEIN_MAX_KEY_LEN];
    char      value  [SEIN_MAX_VAL_LEN];
    char      comment[SEIN_MAX_COMMENT_LEN];
} SeinDocEntry;

typedef struct {
    char         name   [SEIN_MAX_SEC_LEN];
    char         comment[SEIN_MAX_COMMENT_LEN];
    SeinDocEntry entries[SEIN_MAX_DOC_KEYS];
    int          entry_count;
} SeinDocSection;

typedef struct {
    char            path      [SEIN_MAX_PATH_LEN];
    SeinDirective   directives[SEIN_MAX_DIRECTIVES];
    int             directive_count;
    SeinDocSection  sections  [SEIN_MAX_DOC_SECTIONS];
    int             section_count;
} SeinDocument;

///////////////////////////////////////////////////////////////////////////
//                       Writer API — declarations                       //
///////////////////////////////////////////////////////////////////////////

void sein_doc_init        (SeinDocument *doc, const char *path);
SeinDocument *sein_doc_alloc (const char *path);  /// heap-allocate; free with sein_doc_free ///
void          sein_doc_free  (SeinDocument *doc); /// free heap-allocated document ///
void sein_doc_add_include (SeinDocument *doc, const char *inc_path, const char *comment);
void sein_doc_add_global_var     (SeinDocument *doc, const char *name, const char *value, const char *comment);
void sein_doc_add_comment (SeinDocument *doc, const char *text);
void sein_doc_add_blank   (SeinDocument *doc);
void sein_doc_add_section (SeinDocument *doc, const char *name, const char *comment);
void sein_doc_add_value   (SeinDocument *doc, const char *section, const char *key,
                            const char *value, const char *comment);
void sein_doc_remove_value  (SeinDocument *doc, const char *section, const char *key);
void sein_doc_remove_section(SeinDocument *doc, const char *name);
int  sein_doc_save        (const SeinDocument *doc);
int  sein_doc_save_as     (const SeinDocument *doc, const char *path);
int  sein_doc_load        (SeinDocument *doc, const char *path);

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

static void sein__set_var(SeinConfig *cfg, const char *key, const char *val)
{
    size_t key_len = key ? strlen(key) : 0;
    size_t val_len = val ? strlen(val) : 0;
    for (int i = 0; i < cfg->var_count; ++i) {
        if (strcmp(cfg->vars[i].key, key) == 0) {
            size_t copy = val_len < SEIN_MAX_VAL_LEN ? val_len + 1 : SEIN_MAX_VAL_LEN;
            memcpy(cfg->vars[i].val, val, copy - 1);
            cfg->vars[i].val[copy - 1] = '\0';
            return;
        }
    }
    if (cfg->var_count >= SEIN_MAX_VARS) {
        fprintf(stderr, "[SEIN Parser]: @set variable limit reached\n");
        return;
    }
    SeinEntry *e = &cfg->vars[cfg->var_count++];
    size_t ck = key_len < SEIN_MAX_KEY_LEN ? key_len + 1 : SEIN_MAX_KEY_LEN;
    memcpy(e->key, key, ck - 1); e->key[ck - 1] = '\0';
    size_t cv = val_len < SEIN_MAX_VAL_LEN ? val_len + 1 : SEIN_MAX_VAL_LEN;
    memcpy(e->val, val, cv - 1); e->val[cv - 1] = '\0';
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
                        /// @set variables take priority over environment ///
                        const char *resolved = NULL;
                        for (int vi = 0; vi < cfg->var_count; ++vi) {
                            if (strcmp(cfg->vars[vi].key, var_name) == 0) {
                                resolved = cfg->vars[vi].val;
                                break;
                            }
                        }
                        if (!resolved) resolved = getenv(var_name);
                        if (resolved) {
                            size_t env_len = strlen(resolved);
                            if (dst + env_len < SEIN_MAX_VAL_LEN) {
                                memcpy(temp + dst, resolved, env_len);
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

        /// @set VAR_NAME = value ///
        if (strncmp(clean, "@set", 4) == 0 && (clean[4] == ' ' || clean[4] == '\t')) {
            char *rest = sein__trim_inplace(clean + 4);
            char *eq = strchr(rest, '=');
            if (eq) {
                *eq = '\0';
                char *var_name = sein__trim_inplace(rest);
                char *var_val  = sein__trim_inplace(eq + 1);
                var_val = sein__strip_quotes(var_val);
                sein__substitute_value(var_val, cfg);
                sein__set_var(cfg, var_name, var_val);
            }
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

        /////////// raw string: value starts with "R( ////////
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

///////////////////////////////////////////////////////////////////////////
//                     Writer API — implementation                       //
///////////////////////////////////////////////////////////////////////////

static int sein__doc_needs_quotes(const char *v)
{
    if (!v || *v == '\0') return 1;
    size_t len = strlen(v);
    if (v[0] == ' ' || v[0] == '\t') return 1;
    if (v[len-1] == ' ' || v[len-1] == '\t') return 1;
    for (const char *p = v; *p; ++p)
        if (*p == '#' || *p == '=' || *p == ' ' || *p == '\t') return 1;
    return 0;
}

static SeinDocSection *sein__doc_find_section(SeinDocument *doc, const char *name)
{
    for (int i = 0; i < doc->section_count; ++i)
        if (strcmp(doc->sections[i].name, name) == 0)
            return &doc->sections[i];
    return NULL;
}

static SeinDocSection *sein__doc_get_or_create_section(SeinDocument *doc,
                                                        const char *name,
                                                        const char *comment)
{
    SeinDocSection *s = sein__doc_find_section(doc, name);
    if (s) return s;
    if (doc->section_count >= SEIN_MAX_DOC_SECTIONS) {
        fprintf(stderr, "[SEIN Writer]: section limit reached\n");
        return NULL;
    }
    s = &doc->sections[doc->section_count++];
    memset(s, 0, sizeof(*s));
    strncpy(s->name, name, SEIN_MAX_SEC_LEN - 1);
    if (comment) strncpy(s->comment, comment, SEIN_MAX_COMMENT_LEN - 1);
    return s;
}

void sein_doc_init(SeinDocument *doc, const char *path)
{
    memset(doc, 0, sizeof(*doc));
    if (path) strncpy(doc->path, path, SEIN_MAX_PATH_LEN - 1);
}

SeinDocument *sein_doc_alloc(const char *path)
{
    SeinDocument *doc = (SeinDocument *)calloc(1, sizeof(SeinDocument));
    if (!doc) { fprintf(stderr, "[SEIN Writer]: out of memory\n"); return NULL; }
    if (path) strncpy(doc->path, path, SEIN_MAX_PATH_LEN - 1);
    return doc;
}

void sein_doc_free(SeinDocument *doc)
{
    free(doc);
}

void sein_doc_add_include(SeinDocument *doc, const char *inc_path, const char *comment)
{
    if (doc->directive_count >= SEIN_MAX_DIRECTIVES) return;
    SeinDirective *d = &doc->directives[doc->directive_count++];
    memset(d, 0, sizeof(*d));
    d->kind = SEIN_DIR_INCLUDE;
    if (inc_path) strncpy(d->operand, inc_path, SEIN_MAX_KEY_LEN - 1);
    if (comment)  strncpy(d->comment, comment,  SEIN_MAX_COMMENT_LEN - 1);
}

void sein_doc_add_global_var(SeinDocument *doc, const char *name, const char *value,
                      const char *comment)
{
    /// update existing @set with same name ///
    for (int i = 0; i < doc->directive_count; ++i) {
        if (doc->directives[i].kind == SEIN_DIR_SET &&
            strcmp(doc->directives[i].operand, name) == 0)
        {
            if (value)   strncpy(doc->directives[i].value,   value,   SEIN_MAX_VAL_LEN - 1);
            if (comment) strncpy(doc->directives[i].comment, comment, SEIN_MAX_COMMENT_LEN - 1);
            return;
        }
    }
    if (doc->directive_count >= SEIN_MAX_DIRECTIVES) return;
    SeinDirective *d = &doc->directives[doc->directive_count++];
    memset(d, 0, sizeof(*d));
    d->kind = SEIN_DIR_SET;
    if (name)    strncpy(d->operand, name,    SEIN_MAX_KEY_LEN - 1);
    if (value)   strncpy(d->value,   value,   SEIN_MAX_VAL_LEN - 1);
    if (comment) strncpy(d->comment, comment, SEIN_MAX_COMMENT_LEN - 1);
}

void sein_doc_add_comment(SeinDocument *doc, const char *text)
{
    if (doc->directive_count >= SEIN_MAX_DIRECTIVES) return;
    SeinDirective *d = &doc->directives[doc->directive_count++];
    memset(d, 0, sizeof(*d));
    d->kind = SEIN_DIR_COMMENT;
    if (text) strncpy(d->operand, text, SEIN_MAX_KEY_LEN - 1);
}

void sein_doc_add_blank(SeinDocument *doc)
{
    if (doc->directive_count >= SEIN_MAX_DIRECTIVES) return;
    SeinDirective *d = &doc->directives[doc->directive_count++];
    memset(d, 0, sizeof(*d));
    d->kind = SEIN_DIR_BLANK;
}

void sein_doc_add_section(SeinDocument *doc, const char *name, const char *comment)
{
    sein__doc_get_or_create_section(doc, name, comment);
}

void sein_doc_add_value(SeinDocument *doc, const char *section,
                        const char *key, const char *value, const char *comment)
{
    SeinDocSection *sec = sein__doc_get_or_create_section(doc, section, NULL);
    if (!sec) return;
    /// update if exists ///
    for (int i = 0; i < sec->entry_count; ++i) {
        if (strcmp(sec->entries[i].key, key) == 0) {
            if (value)   strncpy(sec->entries[i].value,   value,   SEIN_MAX_VAL_LEN - 1);
            if (comment) strncpy(sec->entries[i].comment, comment, SEIN_MAX_COMMENT_LEN - 1);
            return;
        }
    }
    if (sec->entry_count >= SEIN_MAX_DOC_KEYS) {
        fprintf(stderr, "[SEIN Writer]: key limit reached in section '%s'\n", section);
        return;
    }
    SeinDocEntry *e = &sec->entries[sec->entry_count++];
    memset(e, 0, sizeof(*e));
    if (key)     strncpy(e->key,     key,     SEIN_MAX_KEY_LEN - 1);
    if (value)   strncpy(e->value,   value,   SEIN_MAX_VAL_LEN - 1);
    if (comment) strncpy(e->comment, comment, SEIN_MAX_COMMENT_LEN - 1);
}

void sein_doc_remove_value(SeinDocument *doc, const char *section, const char *key)
{
    SeinDocSection *sec = sein__doc_find_section(doc, section);
    if (!sec) return;
    for (int i = 0; i < sec->entry_count; ++i) {
        if (strcmp(sec->entries[i].key, key) == 0) {
            memmove(&sec->entries[i], &sec->entries[i + 1],
                    (size_t)(sec->entry_count - i - 1) * sizeof(SeinDocEntry));
            sec->entry_count--;
            return;
        }
    }
}

void sein_doc_remove_section(SeinDocument *doc, const char *name)
{
    for (int i = 0; i < doc->section_count; ++i) {
        if (strcmp(doc->sections[i].name, name) == 0) {
            memmove(&doc->sections[i], &doc->sections[i + 1],
                    (size_t)(doc->section_count - i - 1) * sizeof(SeinDocSection));
            doc->section_count--;
            return;
        }
    }
}

int sein_doc_save_as(const SeinDocument *doc, const char *path)
{
    FILE *f = fopen(path, "w");
    if (!f) {
        fprintf(stderr, "[SEIN Writer]: Failed to open for writing: %s\n", path);
        return 0;
    }

    for (int i = 0; i < doc->directive_count; ++i) {
        const SeinDirective *d = &doc->directives[i];
        switch (d->kind) {
            case SEIN_DIR_BLANK:
                fprintf(f, "\n");
                break;
            case SEIN_DIR_COMMENT:
                fprintf(f, "# %s\n", d->operand);
                break;
            case SEIN_DIR_INCLUDE:
                if (d->comment[0])
                    fprintf(f, "@include \"%s\"  # %s\n", d->operand, d->comment);
                else
                    fprintf(f, "@include \"%s\"\n", d->operand);
                break;
            case SEIN_DIR_SET:
                if (sein__doc_needs_quotes(d->value)) {
                    if (d->comment[0])
                        fprintf(f, "@set %s = \"%s\"  # %s\n", d->operand, d->value, d->comment);
                    else
                        fprintf(f, "@set %s = \"%s\"\n", d->operand, d->value);
                } else {
                    if (d->comment[0])
                        fprintf(f, "@set %s = %s  # %s\n", d->operand, d->value, d->comment);
                    else
                        fprintf(f, "@set %s = %s\n", d->operand, d->value);
                }
                break;
        }
    }

    if (doc->directive_count > 0 && doc->section_count > 0)
        fprintf(f, "\n");

    for (int si = 0; si < doc->section_count; ++si) {
        const SeinDocSection *sec = &doc->sections[si];
        if (sec->comment[0]) fprintf(f, "# %s\n", sec->comment);
        fprintf(f, "[%s]\n", sec->name);
        for (int ki = 0; ki < sec->entry_count; ++ki) {
            const SeinDocEntry *e = &sec->entries[ki];
            if (sein__doc_needs_quotes(e->value)) {
                if (e->comment[0])
                    fprintf(f, "%s = \"%s\"  # %s\n", e->key, e->value, e->comment);
                else
                    fprintf(f, "%s = \"%s\"\n", e->key, e->value);
            } else {
                if (e->comment[0])
                    fprintf(f, "%s = %s  # %s\n", e->key, e->value, e->comment);
                else
                    fprintf(f, "%s = %s\n", e->key, e->value);
            }
        }
        if (si + 1 < doc->section_count) fprintf(f, "\n");
    }

    fclose(f);
    return 1;
}

int sein_doc_save(const SeinDocument *doc)
{
    return sein_doc_save_as(doc, doc->path);
}

int sein_doc_load(SeinDocument *doc, const char *path)
{
    sein_doc_init(doc, path);
    FILE *f = fopen(path, "r");
    if (!f) {
        fprintf(stderr, "[SEIN Writer]: Failed to open: %s\n", path);
        return 0;
    }

    char line[SEIN_MAX_VAL_LEN * 2];
    char current_section[SEIN_MAX_SEC_LEN] = "";
    int  in_header = 1;

    while (fgets(line, sizeof(line), f)) {
        /// strip newline ///
        size_t len = strlen(line);
        while (len > 0 && (line[len-1] == '\n' || line[len-1] == '\r'))
            line[--len] = '\0';

        char *clean = sein__trim_inplace(line);
        if (!clean || *clean == '\0') {
            if (in_header) sein_doc_add_blank(doc);
            continue;
        }
        if (clean[0] == '#') {
            if (in_header) sein_doc_add_comment(doc, sein__trim_inplace(clean + 1));
            continue;
        }
        if (strncmp(clean, "@include", 8) == 0) {
            char *inc = sein__strip_quotes(sein__trim_inplace(clean + 8));
            sein_doc_add_include(doc, inc, NULL);
            continue;
        }
        if (strncmp(clean, "@set", 4) == 0 && (clean[4] == ' ' || clean[4] == '\t')) {
            char *rest = sein__trim_inplace(clean + 4);
            char *eq   = strchr(rest, '=');
            if (eq) {
                *eq = '\0';
                char *name = sein__trim_inplace(rest);
                char *val  = sein__strip_quotes(sein__trim_inplace(eq + 1));
                sein_doc_add_global_var(doc, name, val, NULL);
            }
            continue;
        }
        size_t clen = strlen(clean);
        if (clean[0] == '[' && clean[clen-1] == ']') {
            in_header = 0;
            clean[clen-1] = '\0';
            strncpy(current_section, sein__trim_inplace(clean + 1), SEIN_MAX_SEC_LEN - 1);
            sein_doc_add_section(doc, current_section, NULL);
            continue;
        }
        char *eq = strchr(clean, '=');
        if (eq && current_section[0]) {
            *eq = '\0';
            char *key = sein__trim_inplace(clean);
            char *val = sein__trim_inplace(eq + 1);
            /// strip inline comment ///
            char *hash = strchr(val, '#');
            if (hash) { *hash = '\0'; val = sein__trim_inplace(val); }
            val = sein__strip_quotes(val);
            sein_doc_add_value(doc, current_section, key, val, NULL);
        }
    }

    fclose(f);
    return 1;
}

#endif
#endif