/*
= sein.h - Single-header SEIN config parser (C11)
=
= Async:
=   Pass usage_async=true to sein_parse(); call sein_wait(cfg) before using data
*/

#ifndef SEIN_H
#define SEIN_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdbool.h>

// C11 atomics for lock-free hot paths //
#if __STDC_VERSION__ >= 201112L && !defined(__STDC_NO_ATOMICS__)
#  include <stdatomic.h>
#  define SEIN_ATOMIC(T)          _Atomic T
#  define SEIN_ALOAD(x)           atomic_load_explicit(&(x), memory_order_acquire)
#  define SEIN_ASTORE(x,v)        atomic_store_explicit(&(x),(v),memory_order_release)
#  define SEIN_AINC(x)            atomic_fetch_add_explicit(&(x),1,memory_order_acq_rel)
#else
#  define SEIN_ATOMIC(T)          T
#  define SEIN_ALOAD(x)           (x)
#  define SEIN_ASTORE(x,v)        ((x)=(v))
#  define SEIN_AINC(x)            (++(x))
#endif

// mmap support //
#if defined(__unix__) || defined(__linux__) || defined(__APPLE__)
#  define SEIN_HAS_MMAP 1
#  include <sys/mman.h>
#  include <sys/stat.h>
#  include <fcntl.h>
#  include <unistd.h>
#elif defined(_WIN32)
#  define SEIN_HAS_WIN_MMAP 1
#  ifndef WIN32_LEAN_AND_MEAN
#    define WIN32_LEAN_AND_MEAN
#  endif
#  include <windows.h>
#endif

// async (threading) support //
#if defined(SEIN_HAS_MMAP)
#  define SEIN_HAS_PTHREAD 1
#  include <pthread.h>
#elif defined(SEIN_HAS_WIN_MMAP)
#  define SEIN_HAS_WINTHREADS 1
#endif

// Public constants //
#define SEIN_MAX_SECTIONS      32
#define SEIN_MAX_KEYS          64
#define SEIN_MAX_VARS          64
#define SEIN_MAX_KEY_LEN      128
#define SEIN_MAX_VAL_LEN      512
#define SEIN_MAX_SEC_LEN      128
#define SEIN_MAX_INCLUDE_DEPTH  8
#define SEIN_MAX_DIRECTIVES   32
#define SEIN_MAX_DOC_SECTIONS 32
#define SEIN_MAX_DOC_KEYS     64
#define SEIN_MAX_COMMENT_LEN  128
#define SEIN_MAX_PATH_LEN     512

// Public types //
typedef struct {
    char key[SEIN_MAX_KEY_LEN];
    char val[SEIN_MAX_VAL_LEN];
} SeinEntry;

typedef struct {
    char             name  [SEIN_MAX_SEC_LEN];
    char             parent[SEIN_MAX_SEC_LEN]; // "" = no inheritance //
    SeinEntry        entries[SEIN_MAX_KEYS];
    SEIN_ATOMIC(int) entry_count;
} SeinSection;

typedef struct {
    SeinSection      sections[SEIN_MAX_SECTIONS];
    SEIN_ATOMIC(int) section_count;
    SeinEntry        vars[SEIN_MAX_VARS];   // @set variables /
    SEIN_ATOMIC(int) var_count;

    // async state //
    bool             _usage_async;
    char             _async_path[SEIN_MAX_PATH_LEN];
    SEIN_ATOMIC(int) _async_done; // 1 once parsing completes //
#if defined(SEIN_HAS_PTHREAD)
    pthread_t        _async_thread;
#elif defined(SEIN_HAS_WINTHREADS)
    HANDLE           _async_thread;
#endif
} SeinConfig;

// Public API //

// Parse a .sein file. If usage_async=true the call returns immediately
//   use sein_wait() before reading any values //
int  sein_parse(SeinConfig *cfg, const char *path, bool usage_async);

// Block until an async parse finishes. Safe to call on sync parses too //
void sein_wait(SeinConfig *cfg);

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

// Writer API - types //
typedef enum {
    SEIN_DIR_BLANK   = 0,
    SEIN_DIR_COMMENT = 1,
    SEIN_DIR_INCLUDE = 2,
    SEIN_DIR_SET     = 3
} SeinDirectiveKind;

typedef struct {
    SeinDirectiveKind kind;
    char operand[SEIN_MAX_KEY_LEN];
    char value  [SEIN_MAX_VAL_LEN];
    char comment[SEIN_MAX_COMMENT_LEN];
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
    bool         usage_async; // stored per-document for future async save //
} SeinDocSection;

typedef struct {
    char            path      [SEIN_MAX_PATH_LEN];
    SeinDirective   directives[SEIN_MAX_DIRECTIVES];
    int             directive_count;
    SeinDocSection  sections  [SEIN_MAX_DOC_SECTIONS];
    int             section_count;
    bool            usage_async;
} SeinDocument;

// Writer API - declarations //
void          sein_doc_init      (SeinDocument *doc, const char *path);

// create_new_config: heap-allocate a new document (usage_async stored for future saves)//
SeinDocument *create_new_config  (const char *path, bool usage_async);

void          sein_doc_free      (SeinDocument *doc);
void          sein_doc_add_include     (SeinDocument *doc, const char *inc_path, const char *comment);
void          sein_doc_add_global_var  (SeinDocument *doc, const char *name, const char *value, const char *comment);
void          sein_doc_add_comment     (SeinDocument *doc, const char *text);
void          sein_doc_add_blank       (SeinDocument *doc);
void          sein_doc_add_section     (SeinDocument *doc, const char *name, const char *comment);
void          sein_doc_add_value       (SeinDocument *doc, const char *section, const char *key,
                                        const char *value, const char *comment);
void          sein_doc_remove_value    (SeinDocument *doc, const char *section, const char *key);
void          sein_doc_remove_section  (SeinDocument *doc, const char *name);
int           sein_doc_save            (const SeinDocument *doc);
int           sein_doc_save_as         (const SeinDocument *doc, const char *path);
int           sein_doc_load            (SeinDocument *doc, const char *path);

// IMPLEMENTATION //
#ifdef SEIN_IMPLEMENTATION

// internal helpers //
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
    // don't strip inside quoted strings //
    bool in_quote = false;
    for (char *p = line; *p; ++p) {
        if (*p == '"') in_quote = !in_quote;
        if (!in_quote && *p == '#') { *p = '\0'; return; }
    }
}

static char *sein__strip_quotes(char *val)
{
    if (!val) return val;
    size_t len = strlen(val);
    if (len >= 2 && val[0] == '"' && val[len - 1] == '"') {
        val[len - 1] = '\0';
        return val + 1;
    }
    return val;
}

// Lock-free section lookup - acquires section_count once, then reads
//   sequentially. Safe for concurrent readers after parse is complete //
static SeinSection *sein__find_section(const SeinConfig *cfg, const char *name)
{
    int n = SEIN_ALOAD(cfg->section_count);
    for (int i = 0; i < n; ++i)
        if (strcmp(cfg->sections[i].name, name) == 0)
            return (SeinSection *)&cfg->sections[i];
    return NULL;
}

static SeinSection *sein__get_or_create_section(SeinConfig *cfg, const char *name)
{
    SeinSection *s = sein__find_section(cfg, name);
    if (s) return s;

    int idx = SEIN_ALOAD(cfg->section_count);
    if (idx >= SEIN_MAX_SECTIONS) {
        fprintf(stderr, "[SEIN Parser]: section limit reached\n");
        return NULL;
    }
    s = &cfg->sections[idx];
    strncpy(s->name, name, SEIN_MAX_SEC_LEN - 1);
    s->name[SEIN_MAX_SEC_LEN - 1] = '\0';
    s->parent[0]  = '\0';
    SEIN_ASTORE(s->entry_count, 0);
    // publish the new section atomically ///
    SEIN_AINC(cfg->section_count);
    return s;
}

static void sein__set(SeinSection *sec, const char *key, const char *val)
{
    size_t key_len = key ? strlen(key) : 0;
    size_t val_len = val ? strlen(val) : 0;
    int n = SEIN_ALOAD(sec->entry_count);

    for (int i = 0; i < n; ++i) {
        if (strlen(sec->entries[i].key) == key_len &&
            memcmp(sec->entries[i].key, key, key_len) == 0)
        {
            size_t cv = val_len < SEIN_MAX_VAL_LEN ? val_len + 1 : SEIN_MAX_VAL_LEN;
            memcpy(sec->entries[i].val, val, cv - 1);
            sec->entries[i].val[cv - 1] = '\0';
            return;
        }
    }
    if (n >= SEIN_MAX_KEYS) {
        fprintf(stderr, "[SEIN Parser]: key limit reached in section '%s'\n", sec->name);
        return;
    }
    SeinEntry *e = &sec->entries[n];
    size_t ck = key_len < SEIN_MAX_KEY_LEN ? key_len + 1 : SEIN_MAX_KEY_LEN;
    memcpy(e->key, key, ck - 1); e->key[ck - 1] = '\0';
    size_t cv = val_len < SEIN_MAX_VAL_LEN ? val_len + 1 : SEIN_MAX_VAL_LEN;
    memcpy(e->val, val, cv - 1); e->val[cv - 1] = '\0';
    /// publish the new entry atomically //
    SEIN_AINC(sec->entry_count);
}

static void sein__set_var(SeinConfig *cfg, const char *key, const char *val)
{
    size_t key_len = key ? strlen(key) : 0;
    size_t val_len = val ? strlen(val) : 0;
    int n = SEIN_ALOAD(cfg->var_count);

    for (int i = 0; i < n; ++i) {
        if (strcmp(cfg->vars[i].key, key) == 0) {
            size_t cv = val_len < SEIN_MAX_VAL_LEN ? val_len + 1 : SEIN_MAX_VAL_LEN;
            memcpy(cfg->vars[i].val, val, cv - 1);
            cfg->vars[i].val[cv - 1] = '\0';
            return;
        }
    }
    if (n >= SEIN_MAX_VARS) {
        fprintf(stderr, "[SEIN Parser]: @set variable limit reached\n");
        return;
    }
    SeinEntry *e = &cfg->vars[n];
    size_t ck = key_len < SEIN_MAX_KEY_LEN ? key_len + 1 : SEIN_MAX_KEY_LEN;
    memcpy(e->key, key, ck - 1); e->key[ck - 1] = '\0';
    size_t cv = val_len < SEIN_MAX_VAL_LEN ? val_len + 1 : SEIN_MAX_VAL_LEN;
    memcpy(e->val, val, cv - 1); e->val[cv - 1] = '\0';
    SEIN_AINC(cfg->var_count);
}

// Expand ${VAR}, ${section.key}, ${SYSENV:VAR} inside *val (in-place) //
static void sein__substitute_value(char *val, const SeinConfig *cfg)
{
    if (!val || *val == '\0') return;
    char temp[SEIN_MAX_VAL_LEN];
    size_t vlen = strlen(val);
    size_t src = 0, dst = 0;

    while (src < vlen && dst < SEIN_MAX_VAL_LEN - 1) {
        if (val[src] == '$' && src + 1 < vlen && val[src + 1] == '{') {
            char *brace_end = strchr(val + src + 2, '}');
            if (brace_end) {
                size_t var_len = (size_t)(brace_end - (val + src + 2));
                char var_name[256];
                if (var_len < sizeof(var_name)) {
                    memcpy(var_name, val + src + 2, var_len);
                    var_name[var_len] = '\0';

                    const char *resolved = NULL;

                    // ${SYSENV:VAR} - always OS environment, never @set //
                    if (var_len > 7 && memcmp(var_name, "SYSENV:", 7) == 0) {
                        resolved = getenv(var_name + 7);
                    } else {
                        char *dot = strchr(var_name, '.');
                        if (dot) {
                            *dot = '\0';
                            resolved = sein_get(cfg, var_name, dot + 1, NULL);
                        } else {
                            // @set takes priority over OS environment //
                            int nv = SEIN_ALOAD(cfg->var_count);
                            for (int vi = 0; vi < nv; ++vi) {
                                if (strcmp(cfg->vars[vi].key, var_name) == 0) {
                                    resolved = cfg->vars[vi].val;
                                    break;
                                }
                            }
                            if (!resolved) resolved = getenv(var_name);
                        }
                    }

                    if (resolved) {
                        size_t rlen = strlen(resolved);
                        if (dst + rlen < SEIN_MAX_VAL_LEN) {
                            memcpy(temp + dst, resolved, rlen);
                            dst += rlen;
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
    memcpy(val, temp, dst + 1);
}

// Resolve [Child : [Parent]] inheritance after all files are parsed //
static void sein__resolve_inheritance(SeinConfig *cfg)
{
    int nsec = SEIN_ALOAD(cfg->section_count);
    for (int i = 0; i < nsec; ++i) {
        SeinSection *child = &cfg->sections[i];
        if (child->parent[0] == '\0') continue;

        SeinSection *parent = sein__find_section(cfg, child->parent);
        if (!parent) continue;

        int np = SEIN_ALOAD(parent->entry_count);
        for (int j = 0; j < np; ++j) {
            // Only copy keys the child does not already define //
            int nc = SEIN_ALOAD(child->entry_count);
            bool exists = false;
            for (int k = 0; k < nc; ++k) {
                if (strcmp(child->entries[k].key, parent->entries[j].key) == 0) {
                    exists = true; break;
                }
            }
            if (!exists)
                sein__set(child, parent->entries[j].key, parent->entries[j].val);
        }
    }
}

// mmap-based line-source abstraction //

typedef struct {
    const char *buf;   // mmap'd or NULL (stdio mode) //
    size_t      len;
    size_t      pos;
    FILE       *fp;    // used if buf == NULL //
#if defined(SEIN_HAS_WIN_MMAP)
    HANDLE      hfile;
    HANDLE      hmap;
#elif defined(SEIN_HAS_MMAP)
    int         fd;
#endif
    char        line_buf[SEIN_MAX_VAL_LEN * 2];
} SeinSrc;

static bool sein__src_open(SeinSrc *s, const char *path)
{
    memset(s, 0, sizeof(*s));
#if defined(SEIN_HAS_MMAP)
    s->fd = open(path, O_RDONLY);
    if (s->fd < 0) goto stdio_fallback;
    struct stat st;
    if (fstat(s->fd, &st) != 0 || st.st_size == 0) {
        close(s->fd); s->fd = -1; goto stdio_fallback;
    }
    s->len = (size_t)st.st_size;
    s->buf = (const char *)mmap(NULL, s->len, PROT_READ, MAP_PRIVATE, s->fd, 0);
    if (s->buf == MAP_FAILED) {
        s->buf = NULL; close(s->fd); s->fd = -1; goto stdio_fallback;
    }
#ifdef MADV_SEQUENTIAL
    madvise((void *)s->buf, s->len, MADV_SEQUENTIAL);
#endif
    return true;

stdio_fallback:
#elif defined(SEIN_HAS_WIN_MMAP)
    s->hfile = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ, NULL,
                           OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (s->hfile == INVALID_HANDLE_VALUE) goto stdio_fallback;
    LARGE_INTEGER fsz = {0};
    GetFileSizeEx(s->hfile, &fsz);
    s->len = (size_t)fsz.QuadPart;
    if (s->len == 0) { CloseHandle(s->hfile); goto stdio_fallback; }
    s->hmap = CreateFileMappingA(s->hfile, NULL, PAGE_READONLY, 0, 0, NULL);
    if (!s->hmap) { CloseHandle(s->hfile); goto stdio_fallback; }
    s->buf = (const char *)MapViewOfFile(s->hmap, FILE_MAP_READ, 0, 0, 0);
    if (!s->buf) {
        CloseHandle(s->hmap); CloseHandle(s->hfile); goto stdio_fallback;
    }
    return true;

stdio_fallback:
#endif
    s->fp = fopen(path, "r");
    return s->fp != NULL;
}

// Returns the next line into s->line_buf, returns NULL at EOF //
static char *sein__src_readline(SeinSrc *s)
{
    if (s->buf) {
        if (s->pos >= s->len) return NULL;
        size_t start = s->pos;
        while (s->pos < s->len && s->buf[s->pos] != '\n') ++s->pos;
        size_t end = s->pos;
        if (s->pos < s->len) ++s->pos; // consume '\n' //
        if (end > start && s->buf[end - 1] == '\r') --end;
        size_t ll = end - start;
        if (ll >= sizeof(s->line_buf)) ll = sizeof(s->line_buf) - 1;
        memcpy(s->line_buf, s->buf + start, ll);
        s->line_buf[ll] = '\0';
        return s->line_buf;
    } else {
        if (!s->fp) return NULL;
        if (!fgets(s->line_buf, (int)sizeof(s->line_buf), s->fp)) return NULL;
        size_t ll = strlen(s->line_buf);
        while (ll > 0 && (s->line_buf[ll-1] == '\n' || s->line_buf[ll-1] == '\r'))
            s->line_buf[--ll] = '\0';
        return s->line_buf;
    }
}

static void sein__src_close(SeinSrc *s)
{
#if defined(SEIN_HAS_MMAP)
    if (s->buf)  { munmap((void *)s->buf, s->len); s->buf = NULL; }
    if (s->fd >= 0) { close(s->fd); s->fd = -1; }
#elif defined(SEIN_HAS_WIN_MMAP)
    if (s->buf)   { UnmapViewOfFile(s->buf); s->buf = NULL; }
    if (s->hmap)  { CloseHandle(s->hmap); s->hmap = NULL; }
    if (s->hfile != INVALID_HANDLE_VALUE) { CloseHandle(s->hfile); s->hfile = INVALID_HANDLE_VALUE; }
#endif
    if (s->fp) { fclose(s->fp); s->fp = NULL; }
}

// core parse logic //

static void sein__parse_file(SeinConfig *cfg, const char *path, int depth)
{
    if (depth > SEIN_MAX_INCLUDE_DEPTH) {
        fprintf(stderr, "[SEIN Parser]: @include depth limit reached at: %s\n", path);
        return;
    }

    SeinSrc src;
    if (!sein__src_open(&src, path)) {
        fprintf(stderr, "[SEIN Parser]: Failed to open: %s\n", path);
        return;
    }

    // derive base directory for relative @includes //
    char base_dir[512] = "";
    {
        const char *slash = strrchr(path, '/');
#ifdef _WIN32
        const char *bslash = strrchr(path, '\\');
        if (!slash || (bslash && bslash > slash)) slash = bslash;
#endif
        if (slash) {
            size_t blen = (size_t)(slash - path) + 1;
            if (blen < sizeof(base_dir)) {
                memcpy(base_dir, path, blen);
                base_dir[blen] = '\0';
            }
        }
    }

    char current_section[SEIN_MAX_SEC_LEN] = "Default";

    // backslash-multiline state //
    char ml_key[SEIN_MAX_KEY_LEN] = "";
    char ml_val[SEIN_MAX_VAL_LEN] = "";
    int  in_multiline = 0;

    // raw-string state //
    char raw_key[SEIN_MAX_KEY_LEN] = "";
    char raw_val[SEIN_MAX_VAL_LEN] = "";
    int  in_raw = 0;

    char *line;
    while ((line = sein__src_readline(&src)) != NULL) {

        // raw-string block //
        if (in_raw) {
            char *end_pos = strstr(line, ")R\"");
            if (end_pos) {
                *end_pos = '\0';
                size_t cur = strlen(raw_val);
                { size_t _ll = strlen(line), _rem = SEIN_MAX_VAL_LEN - cur - 1, _cp = _ll < _rem ? _ll : _rem;
                  memcpy(raw_val + cur, line, _cp); raw_val[cur + _cp] = '\0'; }
                SeinSection *sec = sein__get_or_create_section(cfg, current_section);
                if (sec) {
                    sein__substitute_value(raw_val, cfg);
                    sein__set(sec, raw_key, raw_val);
                }
                raw_key[0] = raw_val[0] = '\0';
                in_raw = 0;
            } else {
                size_t cur = strlen(raw_val);
                size_t rem = SEIN_MAX_VAL_LEN - cur - 1;
                if (rem > 0) {
                    size_t ll = strlen(line);
                    size_t copy = ll < rem - 1 ? ll : rem - 1;
                    memcpy(raw_val + cur, line, copy);
                    cur += copy;
                    raw_val[cur] = '\0';
                    if (cur < SEIN_MAX_VAL_LEN - 1) { raw_val[cur] = '\n'; raw_val[cur+1] = '\0'; }
                }
            }
            continue;
        }

        // backslash multiline continuation //
        if (in_multiline) {
            sein__strip_comment(line);
            char *cl = sein__trim_inplace(line);
            size_t cl_len = strlen(cl);
            int continues = (cl_len > 0 && cl[cl_len - 1] == '\\');
            if (continues) { cl[cl_len - 1] = '\0'; cl_len--; }
            cl = sein__trim_inplace(cl);

            size_t cur = strlen(ml_val);
            size_t rem = SEIN_MAX_VAL_LEN - cur - 1;
            if (cur < SEIN_MAX_VAL_LEN - 2) {
                ml_val[cur] = ' '; ml_val[cur + 1] = '\0';
                size_t _cl = strlen(cl), _cp = _cl < rem - 1 ? _cl : rem - 1;
                memcpy(ml_val + cur + 1, cl, _cp);
                ml_val[cur + 1 + _cp] = '\0';
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

        // @include "other.sein" //
        if (strncmp(clean, "@include", 8) == 0) {
            char *inc = sein__trim_inplace(clean + 8);
            inc = sein__strip_quotes(inc);
            char inc_path[512];
            snprintf(inc_path, sizeof(inc_path), "%s%s", base_dir, inc);
            sein__parse_file(cfg, inc_path, depth + 1);
            continue;
        }

        // @set VAR = value //
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

        /// [Section] or [Child : [Parent]] //
        size_t clen = strlen(clean);
        if (clean[0] == '[' && clean[clen - 1] == ']') {
            clean[clen - 1] = '\0';
            char *inner = sein__trim_inplace(clean + 1);

            // check for inheritance: "Child : [Parent]" //
            char *colon = strstr(inner, ":");
            if (colon) {
                // split name and parent //
                *colon = '\0';
                char *sec_name_part = sein__trim_inplace(inner);
                char *parent_part   = sein__trim_inplace(colon + 1);
                // parent_part should look like "[Parent]" //
                if (parent_part[0] == '[') {
                    parent_part++;
                    char *pb = strchr(parent_part, ']');
                    if (pb) *pb = '\0';
                    parent_part = sein__trim_inplace(parent_part);
                }
                strncpy(current_section, sec_name_part, SEIN_MAX_SEC_LEN - 1);
                current_section[SEIN_MAX_SEC_LEN - 1] = '\0';
                SeinSection *sec = sein__get_or_create_section(cfg, current_section);
                if (sec && parent_part[0] != '\0') {
                    strncpy(sec->parent, parent_part, SEIN_MAX_SEC_LEN - 1);
                    sec->parent[SEIN_MAX_SEC_LEN - 1] = '\0';
                }
            } else {
                strncpy(current_section, inner, SEIN_MAX_SEC_LEN - 1);
                current_section[SEIN_MAX_SEC_LEN - 1] = '\0';
                sein__get_or_create_section(cfg, current_section);
            }
            continue;
        }
        //
        char *sep = strchr(clean, '=');
        if (!sep) continue;

        *sep = '\0';
        char *key = sein__trim_inplace(clean);
        char *val = sein__trim_inplace(sep + 1);

        // raw string: "R(  //
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

        // multiline continuation //
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

    sein__src_close(&src);
}

// async support //
typedef struct {
    SeinConfig *cfg;
    char        path[512];
} SeinAsyncCtx;

#if defined(SEIN_HAS_PTHREAD)
static void *sein__async_worker(void *arg)
{
    SeinAsyncCtx *ctx = (SeinAsyncCtx *)arg;
    sein__parse_file(ctx->cfg, ctx->path, 0);
    sein__resolve_inheritance(ctx->cfg);
    SEIN_ASTORE(ctx->cfg->_async_done, 1);
    free(ctx);
    return NULL;
}
#elif defined(SEIN_HAS_WINTHREADS)
static DWORD WINAPI sein__async_worker(LPVOID arg)
{
    SeinAsyncCtx *ctx = (SeinAsyncCtx *)arg;
    sein__parse_file(ctx->cfg, ctx->path, 0);
    sein__resolve_inheritance(ctx->cfg);
    SEIN_ASTORE(ctx->cfg->_async_done, 1);
    free(ctx);
    return 0;
}
#endif

// public API implementation //

int sein_parse(SeinConfig *cfg, const char *path, bool usage_async)
{
    memset(cfg, 0, sizeof(*cfg));
    cfg->_usage_async = usage_async;
    SEIN_ASTORE(cfg->_async_done, 0);

    if (!usage_async) {
        sein__parse_file(cfg, path, 0);
        sein__resolve_inheritance(cfg);
        SEIN_ASTORE(cfg->_async_done, 1);
        return 1;
    }

#if defined(SEIN_HAS_PTHREAD)
    SeinAsyncCtx *ctx = (SeinAsyncCtx *)malloc(sizeof(SeinAsyncCtx));
    if (!ctx) { sein__parse_file(cfg, path, 0); sein__resolve_inheritance(cfg); SEIN_ASTORE(cfg->_async_done, 1); return 1; }
    ctx->cfg = cfg;
    strncpy(ctx->path, path, sizeof(ctx->path) - 1);
    ctx->path[sizeof(ctx->path) - 1] = '\0';
    if (pthread_create(&cfg->_async_thread, NULL, sein__async_worker, ctx) != 0) {
        free(ctx);
        sein__parse_file(cfg, path, 0);
        sein__resolve_inheritance(cfg);
        SEIN_ASTORE(cfg->_async_done, 1);
    }
    return 1;
#elif defined(SEIN_HAS_WINTHREADS)
    SeinAsyncCtx *ctx = (SeinAsyncCtx *)malloc(sizeof(SeinAsyncCtx));
    if (!ctx) { sein__parse_file(cfg, path, 0); sein__resolve_inheritance(cfg); SEIN_ASTORE(cfg->_async_done, 1); return 1; }
    ctx->cfg = cfg;
    strncpy(ctx->path, path, sizeof(ctx->path) - 1);
    ctx->path[sizeof(ctx->path) - 1] = '\0';
    cfg->_async_thread = CreateThread(NULL, 0, sein__async_worker, ctx, 0, NULL);
    if (!cfg->_async_thread) {
        free(ctx);
        sein__parse_file(cfg, path, 0);
        sein__resolve_inheritance(cfg);
        SEIN_ASTORE(cfg->_async_done, 1);
    }
    return 1;
#else
    // no threading available - fall back to synchronous //
    sein__parse_file(cfg, path, 0);
    sein__resolve_inheritance(cfg);
    SEIN_ASTORE(cfg->_async_done, 1);
    return 1;
#endif
}

void sein_wait(SeinConfig *cfg)
{
    if (!cfg || !cfg->_usage_async) return;
    if (SEIN_ALOAD(cfg->_async_done)) return;
#if defined(SEIN_HAS_PTHREAD)
    pthread_join(cfg->_async_thread, NULL);
#elif defined(SEIN_HAS_WINTHREADS)
    if (cfg->_async_thread) {
        WaitForSingleObject(cfg->_async_thread, INFINITE);
        CloseHandle(cfg->_async_thread);
        cfg->_async_thread = NULL;
    }
#else
    // spin-wait fallback (should not be reached) //
    while (!SEIN_ALOAD(cfg->_async_done)) { /* busy wait */}
#endif
}

void sein_free(SeinConfig *cfg) { (void)cfg; }

SeinConfig *sein_alloc(void)
{
    SeinConfig *cfg = (SeinConfig *)calloc(1, sizeof(SeinConfig));
    if (!cfg) fprintf(stderr, "[SEIN Parser]: out of memory\n");
    return cfg;
}

void sein_destroy(SeinConfig *cfg) { free(cfg); }

/// Lock-free read - safe after sein_wait() //
const char *sein_get(const SeinConfig *cfg, const char *section,
                     const char *key,   const char *fallback)
{
    int nsec = SEIN_ALOAD(cfg->section_count);
    for (int i = 0; i < nsec; ++i) {
        if (strcmp(cfg->sections[i].name, section) == 0) {
            const SeinSection *s = &cfg->sections[i];
            int nk = SEIN_ALOAD(s->entry_count);
            for (int j = 0; j < nk; ++j)
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

// array helpers //
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


// Writer API - implementation //
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

SeinDocument *sein_doc_create_new_config(const char *path, bool usage_async)
{
    SeinDocument *doc = (SeinDocument *)calloc(1, sizeof(SeinDocument));
    if (!doc) { fprintf(stderr, "[SEIN Writer]: out of memory\n"); return NULL; }
    if (path) strncpy(doc->path, path, SEIN_MAX_PATH_LEN - 1);
    doc->usage_async = usage_async;
    return doc;
}

void sein_doc_free(SeinDocument *doc) { free(doc); }

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
            char *hash = strchr(val, '#');
            if (hash) { *hash = '\0'; val = sein__trim_inplace(val); }
            val = sein__strip_quotes(val);
            sein_doc_add_value(doc, current_section, key, val, NULL);
        }
    }

    fclose(f);
    return 1;
}

#endif // SEIN_IMPLEMENTATION //
#endif // SEIN_H //