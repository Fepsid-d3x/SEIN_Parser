#pragma once

/*
= sein.hpp - Single-header SEIN config parser (C++17)
=
= Async:
=   auto handle = parse_sein(path, true);
=   handle.wait(); // block until parsing is complete //
=   auto& cfg = handle.data;  // then use the config //
*/

#include <string>
#include <string_view>
#include <vector>
#include <variant>
#include <algorithm>
#include <iostream>
#include <unordered_map>
#include <charconv>
#include <atomic>
#include <thread>
#include <future>
#include <functional>
#include <cstdlib>
#include <cstring>

// mmap support //
#if defined(__unix__) || defined(__linux__) || defined(__APPLE__)
#  define SEIN_HPP_MMAP 1
#  include <sys/mman.h>
#  include <sys/stat.h>
#  include <fcntl.h>
#  include <unistd.h>
#elif defined(_WIN32)
#  define SEIN_HPP_WIN_MMAP 1
#  ifndef WIN32_LEAN_AND_MEAN
#    define WIN32_LEAN_AND_MEAN
#  endif
#  include <windows.h>
#endif

namespace fd::sein {

    // Types //

    struct SeinValue {
        enum class Type { String, Int, Float, Array };

        std::variant<std::string, int, float, std::vector<SeinValue>> data;

        SeinValue() : data(std::string{}) {}
        SeinValue(std::string_view s) : data(std::string(s)) {}
        SeinValue(std::string s) : data(std::move(s)) {}
        SeinValue(int i) : data(i) {}
        SeinValue(float f) : data(f) {}
        SeinValue(std::vector<SeinValue> arr) : data(std::move(arr)) {}

        Type type() const { return static_cast<Type>(data.index()); }

        std::string_view string_view() const {
            if (auto* p = std::get_if<std::string>(&data)) return *p;
            return {};
        }

        const std::string& string_ref() const {
            static const std::string empty;
            if (auto* p = std::get_if<std::string>(&data)) return *p;
            return empty;
        }

        int as_int(int fallback = 0) const {
            if (auto* p = std::get_if<int>(&data))    return *p;
            if (auto* p = std::get_if<float>(&data))  return static_cast<int>(*p);
            if (auto* p = std::get_if<std::string>(&data)) {
                int r = fallback;
                std::from_chars(p->data(), p->data() + p->size(), r);
                return r;
            }
            return fallback;
        }

        float as_float(float fallback = 0.f) const {
            if (auto* p = std::get_if<float>(&data))  return *p;
            if (auto* p = std::get_if<int>(&data))    return static_cast<float>(*p);
            if (auto* p = std::get_if<std::string>(&data)) {
                float r = fallback;
#if defined(__cpp_lib_to_chars) && __cpp_lib_to_chars >= 201611L
                std::from_chars(p->data(), p->data() + p->size(), r);
#else
                try { r = std::stof(*p); } catch (...) { r = fallback; }
#endif
                return r;
            }
            return fallback;
        }

        bool as_bool(bool fallback = false) const {
            if (auto* p = std::get_if<int>(&data))   return *p != 0;
            if (auto* p = std::get_if<float>(&data)) return *p != 0.f;
            if (auto* p = std::get_if<std::string>(&data)) {
                std::string_view sv(*p);
                if (sv == "true"  || sv == "yes" || sv == "1") return true;
                if (sv == "false" || sv == "no"  || sv == "0") return false;
                std::string lower(*p);
                std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
                if (lower == "true"  || lower == "yes") return true;
                if (lower == "false" || lower == "no")  return false;
            }
            return fallback;
        }
    };

    using Config = std::unordered_map<std::string,
                   std::unordered_map<std::string, SeinValue>>;

    // SeinResult - returned by parse_sein() //
    // Holds data + optional async future //
    struct SeinResult {
        Config               data;
        bool                 ok    = false;
        std::shared_future<void> ready; // valid only when async //

        // Block until parsing completes (no-op for sync) //
        void wait() const {
            if (ready.valid()) ready.wait();
        }

        // True if parsing is complete //
        bool done() const {
            if (!ready.valid()) return true;
            return ready.wait_for(std::chrono::seconds(0)) == std::future_status::ready;
        }
    };

    // Detail helpers //
    namespace detail {

    inline std::string_view trim_view(std::string_view s) {
        constexpr std::string_view ws = " \t\r\n";
        size_t start = s.find_first_not_of(ws);
        if (start == std::string_view::npos) return {};
        size_t end = s.find_last_not_of(ws);
        return s.substr(start, end - start + 1);
    }

    inline std::string trim(std::string_view s) {
        auto v = trim_view(s);
        return std::string(v);
    }

    // Strip comment, respecting quoted strings //
    inline std::string_view strip_comment_view(std::string_view line) {
        bool in_quote = false;
        for (size_t i = 0; i < line.size(); ++i) {
            if (line[i] == '"') in_quote = !in_quote;
            if (!in_quote && line[i] == '#') return line.substr(0, i);
        }
        return line;
    }

    inline std::string_view strip_quotes_view(std::string_view val) {
        if (val.size() >= 2 && val.front() == '"' && val.back() == '"')
            return val.substr(1, val.size() - 2);
        return val;
    }

    inline std::string strip_quotes(std::string_view val) {
        return std::string(strip_quotes_view(val));
    }

    inline std::string normalize_key(std::string_view k) {
        size_t lb = k.find('[');
        if (lb == std::string_view::npos) return std::string(k);
        size_t rb = k.rfind(']');
        if (rb == std::string_view::npos || rb <= lb) return std::string(k);
        std::string_view inner = strip_quotes_view(k.substr(lb + 1, rb - lb - 1));
        std::string result;
        result.reserve(lb + 1 + inner.size() + 1);
        result.append(k.substr(0, lb + 1));
        result.append(inner);
        result += ']';
        return result;
    }

    inline std::string dedent(std::string_view s) {
        std::vector<std::string_view> lines;
        size_t start = 0;
        while (start <= s.size()) {
            size_t nl = s.find('\n', start);
            if (nl == std::string_view::npos) nl = s.size();
            lines.push_back(s.substr(start, nl - start));
            start = nl + 1;
        }
        size_t min_indent = std::string_view::npos;
        for (auto& l : lines) {
            size_t first = l.find_first_not_of(" \t");
            if (first == std::string_view::npos) continue;
            min_indent = (min_indent == std::string_view::npos) ? first : std::min(min_indent, first);
        }
        if (min_indent == std::string_view::npos) min_indent = 0;
        std::string result;
        result.reserve(s.size());
        for (size_t i = 0; i < lines.size(); ++i) {
            const auto& l = lines[i];
            result += (l.size() >= min_indent) ? std::string(l.substr(min_indent)) : std::string(l);
            if (i + 1 < lines.size()) result += '\n';
        }
        return result;
    }

    // Build a typed SeinValue from a final substituted string //
    inline SeinValue make_value(std::string_view sv) {
        if (sv.empty()) return SeinValue(std::string{});

        // try int //
        int ival = 0;
        {
            auto [p, ec] = std::from_chars(sv.data(), sv.data() + sv.size(), ival);
            if (ec == std::errc{} && p == sv.data() + sv.size())
                return SeinValue(ival);
        }

        // try float //
#if defined(__cpp_lib_to_chars) && __cpp_lib_to_chars >= 201611L
        float fval = 0.f;
        {
            auto [p, ec] = std::from_chars(sv.data(), sv.data() + sv.size(), fval);
            if (ec == std::errc{} && p == sv.data() + sv.size())
                return SeinValue(fval);
        }
#endif

        return SeinValue(std::string(sv));
    }

    // Expand ${VAR}, ${section.key}, ${SYSENV:VAR} //
    inline std::string substitute_value(std::string_view val,
        const Config& config,
        const std::unordered_map<std::string, std::string>& vars)
    {
        if (val.empty()) return {};

        std::string result;
        result.reserve(val.size() + 64);
        size_t pos = 0;

        while (pos < val.size()) {
            size_t dollar = val.find('$', pos);
            if (dollar == std::string_view::npos) {
                result.append(val.substr(pos));
                break;
            }
            result.append(val.substr(pos, dollar - pos));

            if (dollar + 1 < val.size() && val[dollar + 1] == '{') {
                size_t brace_end = val.find('}', dollar + 2);
                if (brace_end != std::string_view::npos) {
                    auto var_name = val.substr(dollar + 2, brace_end - dollar - 2);

                    // ${SYSENV:VAR} - only OS environment //
                    if (var_name.size() > 7 && var_name.substr(0, 7) == "SYSENV:") {
                        std::string env_key(var_name.substr(7));
                        const char *ev = std::getenv(env_key.c_str());
                        if (ev) result += ev;
                    } else {
                        size_t dot = var_name.find('.');
                        if (dot != std::string_view::npos) {
                            std::string sec_str(var_name.substr(0, dot));
                            std::string key_str(var_name.substr(dot + 1));
                            auto s_it = config.find(sec_str);
                            if (s_it != config.end()) {
                                auto k_it = s_it->second.find(key_str);
                                if (k_it != s_it->second.end())
                                    result += k_it->second.string_ref();
                            }
                        } else {
                            // @set takes priority over OS environment //
                            std::string var_str(var_name);
                            auto v_it = vars.find(var_str);
                            if (v_it != vars.end()) {
                                result += v_it->second;
                            } else {
                                const char *ev = std::getenv(var_str.c_str());
                                if (ev) result += ev;
                            }
                        }
                    }
                    pos = brace_end + 1;
                    continue;
                }
            }
            result += val[dollar];
            pos = dollar + 1;
        }
        return result;
    }

    // mmap line source //

    struct LineSrc {
#if defined(SEIN_HPP_MMAP)
        int         fd   = -1;
        const char *buf  = nullptr;
        size_t      len  = 0;
        size_t      pos  = 0;
        bool        mapped = false;
#elif defined(SEIN_HPP_WIN_MMAP)
        HANDLE      hfile = INVALID_HANDLE_VALUE;
        HANDLE      hmap  = nullptr;
        const char *buf   = nullptr;
        size_t      len   = 0;
        size_t      pos   = 0;
        bool        mapped = false;
#endif
        // stdio fallback //
        std::string text; // whole file read into string if no mmap //
        size_t      text_pos = 0;
        bool        use_text = false;

        std::string line_buf;

        bool open(std::string_view path) {
            std::string spath(path);

#if defined(SEIN_HPP_MMAP)
            fd = ::open(spath.c_str(), O_RDONLY);
            if (fd >= 0) {
                struct stat st{};
                if (fstat(fd, &st) == 0 && st.st_size > 0) {
                    len = (size_t)st.st_size;
                    buf = (const char *)mmap(nullptr, len, PROT_READ, MAP_PRIVATE, fd, 0);
                    if (buf != MAP_FAILED) {
#ifdef MADV_SEQUENTIAL
                        madvise((void *)buf, len, MADV_SEQUENTIAL);
#endif
                        mapped = true;
                        return true;
                    }
                    buf = nullptr;
                }
                ::close(fd); fd = -1;
            }
#elif defined(SEIN_HPP_WIN_MMAP)
            hfile = CreateFileA(spath.c_str(), GENERIC_READ, FILE_SHARE_READ,
                                nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
            if (hfile != INVALID_HANDLE_VALUE) {
                LARGE_INTEGER fsz{};
                GetFileSizeEx(hfile, &fsz);
                len = (size_t)fsz.QuadPart;
                if (len > 0) {
                    hmap = CreateFileMappingA(hfile, nullptr, PAGE_READONLY, 0, 0, nullptr);
                    if (hmap) {
                        buf = (const char *)MapViewOfFile(hmap, FILE_MAP_READ, 0, 0, 0);
                        if (buf) { mapped = true; return true; }
                        CloseHandle(hmap); hmap = nullptr;
                    }
                }
                CloseHandle(hfile); hfile = INVALID_HANDLE_VALUE;
            }
#endif
            // stdio fallback — read entire file into string //
            FILE *f = fopen(spath.c_str(), "r");
            if (!f) return false;
            char tmp[4096];
            while (size_t n = fread(tmp, 1, sizeof(tmp), f)) text.append(tmp, n);
            fclose(f);
            use_text = true;
            return true;
        }

        // Returns pointer to next line or nullptr at EOF //
        const std::string *readline() {
            const char *src = nullptr;
            size_t      src_len = 0;
            size_t     *src_pos = nullptr;

#if defined(SEIN_HPP_MMAP) || defined(SEIN_HPP_WIN_MMAP)
            if (mapped) { src = buf; src_len = len; src_pos = &pos; }
#endif
            if (!src && use_text) { src = text.data(); src_len = text.size(); src_pos = &text_pos; }

            if (src) {
                if (*src_pos >= src_len) return nullptr;
                size_t s = *src_pos;
                while (*src_pos < src_len && src[*src_pos] != '\n') ++(*src_pos);
                size_t e = *src_pos;
                if (*src_pos < src_len) ++(*src_pos);
                if (e > s && src[e-1] == '\r') --e;
                line_buf.assign(src + s, e - s);
                return &line_buf;
            }
            return nullptr;
        }

        void close() {
#if defined(SEIN_HPP_MMAP)
            if (mapped && buf) { munmap((void *)buf, len); buf = nullptr; mapped = false; }
            if (fd >= 0) { ::close(fd); fd = -1; }
#elif defined(SEIN_HPP_WIN_MMAP)
            if (buf)   { UnmapViewOfFile(buf); buf = nullptr; }
            if (hmap)  { CloseHandle(hmap); hmap = nullptr; }
            if (hfile != INVALID_HANDLE_VALUE) { CloseHandle(hfile); hfile = INVALID_HANDLE_VALUE; }
            mapped = false;
#endif
        }
    };

    // core file parser //

    // Inheritance map: child -> parent section name //
    using InheritMap = std::unordered_map<std::string, std::string>;

    // Returns true if the file was successfully opened //
    inline bool parse_file(std::string_view path,
        Config& result,
        std::unordered_map<std::string, std::string>& vars,
        InheritMap& inherit,
        int depth = 0)
    {
        if (depth > 8) {
            std::cerr << "[SEIN Parser]: @include depth limit reached at: " << path << "\n";
            return false;
        }

        LineSrc src;
        if (!src.open(path)) {
            std::cerr << "[SEIN Parser]: Failed to open: " << path << "\n";
            return false;
        }

        std::string base_dir;
        {
            size_t sl = path.find_last_of("/\\");
            if (sl != std::string_view::npos)
                base_dir.assign(path.substr(0, sl + 1));
        }

        std::string current_section = "Default";

        std::string multiline_key;
        std::string multiline_val;
        bool in_multiline = false;

        std::string raw_key;
        std::string raw_val;
        bool in_raw = false;

        const std::string *lineptr;
        while ((lineptr = src.readline()) != nullptr) {
            std::string_view line = *lineptr;

            // raw-string block //
            if (in_raw) {
                size_t ep = line.find(")R\"");
                if (ep != std::string_view::npos) {
                    raw_val.append(line.substr(0, ep));
                    auto fv = dedent(raw_val);
                    result[current_section][raw_key] = make_value(substitute_value(fv, result, vars));
                    raw_key.clear(); raw_val.clear(); in_raw = false;
                } else {
                    raw_val.append(line);
                    raw_val += '\n';
                }
                continue;
            }

            // multiline continuation //
            if (in_multiline) {
                auto cs = strip_comment_view(line);
                auto cl = trim_view(cs);
                bool cont = (!cl.empty() && cl.back() == '\\');
                if (cont) cl.remove_suffix(1);
                auto stripped = trim_view(cl);
                multiline_val += ' ';
                multiline_val.append(stripped);
                if (!cont) {
                    auto fv = strip_quotes(trim_view(multiline_val));
                    result[current_section][multiline_key] = make_value(substitute_value(fv, result, vars));
                    multiline_key.clear(); multiline_val.clear(); in_multiline = false;
                }
                continue;
            }

            auto stripped = strip_comment_view(line);
            auto sv = trim_view(stripped);
            if (sv.empty()) continue;

            // @set VAR = value //
            if (sv.size() >= 4 && sv.substr(0, 4) == "@set" &&
                (sv.size() == 4 || sv[4] == ' ' || sv[4] == '\t'))
            {
                auto rest = trim_view(sv.substr(4));
                size_t eq = rest.find('=');
                if (eq != std::string_view::npos) {
                    auto vn  = trim(rest.substr(0, eq));
                    auto vv  = strip_quotes(trim_view(rest.substr(eq + 1)));
                    vars[vn] = substitute_value(vv, result, vars);
                }
                continue;
            }

            // @include "path" //
            if (sv.size() >= 8 && sv.substr(0, 8) == "@include") {
                auto inc = trim_view(sv.substr(8));
                inc = strip_quotes_view(inc);
                std::string inc_path(base_dir);
                inc_path.append(inc);
                parse_file(inc_path, result, vars, inherit, depth + 1);
                continue;
            }

            // [Section] or [Child : [Parent]] //
            if (sv.front() == '[' && sv.back() == ']') {
                auto inner = trim_view(sv.substr(1, sv.size() - 2));
                size_t colon = inner.find(':');
                if (colon != std::string_view::npos) {
                    auto child_name = trim(inner.substr(0, colon));
                    auto parent_raw = trim_view(inner.substr(colon + 1));
                    // parent_raw should be "[Parent]" //
                    if (!parent_raw.empty() && parent_raw.front() == '[') {
                        parent_raw.remove_prefix(1);
                        if (!parent_raw.empty() && parent_raw.back() == ']')
                            parent_raw.remove_suffix(1);
                    }
                    auto parent_name = trim(parent_raw);
                    current_section = child_name;
                    if (!parent_name.empty())
                        inherit[current_section] = parent_name;
                } else {
                    current_section = trim(inner);
                }
                // ensure section exists //
                result.emplace(current_section, std::unordered_map<std::string, SeinValue>{});
                continue;
            }

            // key = value //
            size_t sep = sv.find('=');
            if (sep == std::string_view::npos) continue;

            auto key_sv  = trim_view(sv.substr(0, sep));
            auto val_sv  = trim_view(sv.substr(sep + 1));
            std::string key = normalize_key(key_sv);

            // raw string "R( ... )R" //
            if (val_sv.size() >= 3 && val_sv.substr(0, 3) == "\"R(") {
                size_t ep = val_sv.find(")R\"", 3);
                if (ep != std::string_view::npos) {
                    auto rs = std::string(val_sv.substr(3, ep - 3));
                    result[current_section][key] = make_value(substitute_value(rs, result, vars));
                } else {
                    raw_key = key;
                    raw_val.assign(val_sv.substr(3));
                    raw_val += '\n';
                    in_raw = true;
                }
                continue;
            }

            // multiline continuation //
            if (!val_sv.empty() && val_sv.back() == '\\') {
                multiline_key = key;
                multiline_val = std::string(trim_view(val_sv.substr(0, val_sv.size() - 1)));
                in_multiline = true;
                continue;
            }

            auto fv = std::string(strip_quotes_view(val_sv));
            result[current_section][key] = make_value(substitute_value(fv, result, vars));
        }

        src.close();
        return true;
    }

    // apply section inheritance: copy parent keys not defined in child //
    inline void resolve_inheritance(Config& cfg, const InheritMap& inherit)
    {
        for (const auto& [child, parent] : inherit) {
            auto p_it = cfg.find(parent);
            if (p_it == cfg.end()) continue;
            auto& child_sec = cfg[child];
            for (const auto& [k, v] : p_it->second) {
                if (child_sec.find(k) == child_sec.end())
                    child_sec[k] = v;
            }
        }
    }

    } // detail //

    // Public parse API  //

    //  parse_sein(path)              — synchronous, returns SeinResult with data ready //
    //    parse_sein(path, true)        — async; call result.wait() before using result.data //
    inline SeinResult parse_sein(std::string_view path, bool usage_async = false)
    {
        if (!usage_async) {
            SeinResult sr;
            sr.data.reserve(8);
            std::unordered_map<std::string, std::string> vars;
            detail::InheritMap inherit;
            sr.ok = detail::parse_file(path, sr.data, vars, inherit);
            detail::resolve_inheritance(sr.data, inherit);
            return sr;
        }

        // async: parse in background thread via std::async //
        std::promise<void> prom;
        SeinResult sr;
        sr.data.reserve(8);
        sr.ready = prom.get_future().share();

        Config* cfg_ptr = &sr.data;
        bool*   ok_ptr  = &sr.ok;
        std::string spath(path);

        std::thread([cfg_ptr, ok_ptr, spath, p = std::move(prom)]() mutable {
            cfg_ptr->reserve(8);
            std::unordered_map<std::string, std::string> vars;
            detail::InheritMap inherit;
            *ok_ptr = detail::parse_file(spath, *cfg_ptr, vars, inherit);
            detail::resolve_inheritance(*cfg_ptr, inherit);
            p.set_value();
        }).detach();

        return sr;
    }

    // Getters //
    inline const SeinValue* get_value_ptr(const Config& cfg,
        std::string_view section, std::string_view key)
    {
        auto s = cfg.find(std::string(section));
        if (s == cfg.end()) return nullptr;
        auto k = s->second.find(std::string(key));
        if (k == s->second.end()) return nullptr;
        return &k->second;
    }

    inline std::string_view get_value_view(const Config& cfg,
        std::string_view section, std::string_view key)
    {
        const SeinValue* v = get_value_ptr(cfg, section, key);
        if (!v) return {};
        return v->string_view();
    }

    inline std::string get_value(const Config& cfg,
        std::string_view section, std::string_view key,
        std::string_view fallback = {})
    {
        const SeinValue* v = get_value_ptr(cfg, section, key);
        if (!v) return std::string(fallback);
        auto sv = v->string_view();
        if (!sv.empty()) return std::string(sv);
        if (v->type() == SeinValue::Type::Int)   return std::to_string(v->as_int());
        if (v->type() == SeinValue::Type::Float) return std::to_string(v->as_float());
        return std::string(fallback);
    }

    inline float get_float(const Config& cfg, std::string_view section,
                           std::string_view key, float fallback = 0.f)
    {
        const SeinValue* v = get_value_ptr(cfg, section, key);
        return v ? v->as_float(fallback) : fallback;
    }

    inline int get_int(const Config& cfg, std::string_view section,
                       std::string_view key, int fallback = 0)
    {
        const SeinValue* v = get_value_ptr(cfg, section, key);
        return v ? v->as_int(fallback) : fallback;
    }

    inline bool get_bool(const Config& cfg, std::string_view section,
                         std::string_view key, bool fallback = false)
    {
        const SeinValue* v = get_value_ptr(cfg, section, key);
        return v ? v->as_bool(fallback) : fallback;
    }

    // Array helpers //

    inline std::vector<std::string> split_value(std::string_view val, char delim = ';') {
        std::vector<std::string> result;
        size_t start = 0;
        while (start <= val.size()) {
            size_t end = val.find(delim, start);
            if (end == std::string_view::npos) end = val.size();
            auto token = detail::trim_view(val.substr(start, end - start));
            if (!token.empty()) result.emplace_back(token);
            if (end == val.size()) break;
            start = end + 1;
        }
        return result;
    }

    inline std::vector<std::string> get_array(const Config& cfg,
        std::string_view section, std::string_view key, char delim = ';')
    {
        return split_value(get_value_view(cfg, section, key), delim);
    }

    inline std::vector<float> get_float_array(const Config& cfg,
        std::string_view section, std::string_view key, char delim = ';')
    {
        auto tokens = get_array(cfg, section, key, delim);
        std::vector<float> result;
        result.reserve(tokens.size());
        for (const auto& t : tokens) {
            float r = 0.f;
#if defined(__cpp_lib_to_chars) && __cpp_lib_to_chars >= 201611L
            std::from_chars(t.data(), t.data() + t.size(), r);
#else
            try { r = std::stof(t); } catch (...) {}
#endif
            result.push_back(r);
        }
        return result;
    }

    inline std::vector<int> get_int_array(const Config& cfg,
        std::string_view section, std::string_view key, char delim = ';')
    {
        auto tokens = get_array(cfg, section, key, delim);
        std::vector<int> result;
        result.reserve(tokens.size());
        for (const auto& t : tokens) {
            int r = 0;
            std::from_chars(t.data(), t.data() + t.size(), r);
            result.push_back(r);
        }
        return result;
    }

    // Subkey helpers — key[subkey] = value //

    inline std::string_view get_subkey_view(const Config& cfg,
        std::string_view section, std::string_view key, std::string_view subkey)
    {
        std::string composite;
        composite.reserve(key.size() + subkey.size() + 2);
        composite.append(key);
        composite += '[';
        composite.append(subkey);
        composite += ']';
        return get_value_view(cfg, section, composite);
    }

    inline std::string get_subkey(const Config& cfg,
        std::string_view section, std::string_view key, std::string_view subkey,
        std::string_view fallback = {})
    {
        auto v = get_subkey_view(cfg, section, key, subkey);
        return v.empty() ? std::string(fallback) : std::string(v);
    }

    inline std::vector<std::string> get_subkeys(const Config& cfg,
        std::string_view section, std::string_view key)
    {
        std::vector<std::string> result;
        auto s = cfg.find(std::string(section));
        if (s == cfg.end()) return result;
        std::string prefix(key);
        prefix += '[';
        for (const auto& [k, v] : s->second) {
            if (k.size() > prefix.size() &&
                k.compare(0, prefix.size(), prefix) == 0 &&
                k.back() == ']')
            {
                std::string_view raw(k.data() + prefix.size(),
                                     k.size() - prefix.size() - 1);
                raw = detail::strip_quotes_view(raw);
                result.emplace_back(raw);
            }
        }
        return result;
    }

    // Writer API - types //
    struct SeinKeyValue {
        std::string key;
        std::string value;
        std::string comment;
    };

    struct SeinSection {
        std::string               name;
        std::string               comment;
        std::vector<SeinKeyValue> entries;
    };

    struct SeinDirective {
        enum class Kind { Include, Set, Comment, Blank };
        Kind        kind    = Kind::Blank;
        std::string operand;
        std::string value;
        std::string comment;
    };

    struct SeinDocument {
        std::string                path;
        std::vector<SeinDirective> directives;
        std::vector<SeinSection>   sections;
        bool                       usage_async = false;
    };

    // Writer API — doc_ functions //
    // create a new empty document
    //      usage_async=true stores the flag for future async saves//
    inline SeinDocument doc_create_new_config(std::string_view path,
                                              bool usage_async = false)
    {
        SeinDocument doc;
        doc.path         = std::string(path);
        doc.usage_async  = usage_async;
        return doc;
    }

    // directives //

    inline void doc_add_include(SeinDocument& doc, std::string_view path,
                                std::string_view comment = {})
    {
        SeinDirective d;
        d.kind    = SeinDirective::Kind::Include;
        d.operand = std::string(path);
        d.comment = std::string(comment);
        doc.directives.push_back(std::move(d));
    }

    inline void doc_add_global_var(SeinDocument& doc, std::string_view name,
                                   std::string_view value, std::string_view comment = {})
    {
        for (auto& d : doc.directives) {
            if (d.kind == SeinDirective::Kind::Set && d.operand == name) {
                d.value   = std::string(value);
                d.comment = std::string(comment);
                return;
            }
        }
        SeinDirective d;
        d.kind    = SeinDirective::Kind::Set;
        d.operand = std::string(name);
        d.value   = std::string(value);
        d.comment = std::string(comment);
        doc.directives.push_back(std::move(d));
    }

    inline void doc_add_header_comment(SeinDocument& doc, std::string_view text)
    {
        SeinDirective d;
        d.kind    = SeinDirective::Kind::Comment;
        d.operand = std::string(text);
        doc.directives.push_back(std::move(d));
    }

    inline void doc_add_blank_line(SeinDocument& doc)
    {
        doc.directives.push_back({ SeinDirective::Kind::Blank, {}, {}, {} });
    }

    // sections //

    inline SeinSection* doc_find_section(SeinDocument& doc, std::string_view name)
    {
        for (auto& s : doc.sections)
            if (s.name == name) return &s;
        return nullptr;
    }

    inline SeinSection& doc_add_section(SeinDocument& doc, std::string_view name,
                                        std::string_view comment = {})
    {
        if (auto* ex = doc_find_section(doc, name)) return *ex;
        SeinSection sec;
        sec.name    = std::string(name);
        sec.comment = std::string(comment);
        doc.sections.push_back(std::move(sec));
        return doc.sections.back();
    }

    inline void doc_remove_section(SeinDocument& doc, std::string_view name)
    {
        auto& s = doc.sections;
        s.erase(std::remove_if(s.begin(), s.end(),
            [&](const SeinSection& sec){ return sec.name == name; }), s.end());
    }

    // key / value //

    inline void doc_add_value(SeinDocument& doc, std::string_view section,
                              std::string_view key, std::string_view value,
                              std::string_view comment = {})
    {
        auto& sec = doc_add_section(doc, section);
        for (auto& kv : sec.entries) {
            if (kv.key == key) {
                kv.value   = std::string(value);
                kv.comment = std::string(comment);
                return;
            }
        }
        sec.entries.push_back({ std::string(key), std::string(value), std::string(comment) });
    }

    inline void doc_remove_value(SeinDocument& doc, std::string_view section,
                                 std::string_view key)
    {
        auto* sec = doc_find_section(doc, section);
        if (!sec) return;
        auto& e = sec->entries;
        e.erase(std::remove_if(e.begin(), e.end(),
            [&](const SeinKeyValue& kv){ return kv.key == key; }), e.end());
    }

    inline void doc_add_subkey_value(SeinDocument& doc, std::string_view section,
                                     std::string_view key, std::string_view subkey,
                                     std::string_view value, std::string_view comment = {})
    {
        std::string composite;
        composite.reserve(key.size() + subkey.size() + 2);
        composite.append(key);
        composite += '[';
        composite.append(subkey);
        composite += ']';
        doc_add_value(doc, section, composite, value, comment);
    }

    inline void doc_remove_subkey_value(SeinDocument& doc, std::string_view section,
                                        std::string_view key, std::string_view subkey)
    {
        std::string composite;
        composite.reserve(key.size() + subkey.size() + 2);
        composite.append(key);
        composite += '[';
        composite.append(subkey);
        composite += ']';
        doc_remove_value(doc, section, composite);
    }

    // serialization helpers //

    namespace detail {

    inline bool needs_quotes(std::string_view v)
    {
        if (v.empty()) return true;
        if (v.front() == ' ' || v.front() == '\t') return true;
        if (v.back()  == ' ' || v.back()  == '\t') return true;
        for (char c : v)
            if (c == '#' || c == '=' || c == ' ' || c == '\t') return true;
        return false;
    }

    inline std::string quote_if_needed(std::string_view v)
    {
        if (needs_quotes(v)) return '"' + std::string(v) + '"';
        return std::string(v);
    }

    inline void write_inline_comment(std::string& out, std::string_view c)
    {
        if (!c.empty()) { out += "  # "; out.append(c); }
    }

    } // detail //

    // save / load //

    inline bool doc_save_config(const SeinDocument& doc, std::string_view path)
    {
        FILE *f = fopen(std::string(path).c_str(), "w");
        if (!f) {
            std::cerr << "[SEIN Writer]: Failed to open for writing: " << path << "\n";
            return false;
        }

        std::string buf;
        buf.reserve(4096);

        for (const auto& d : doc.directives) {
            switch (d.kind) {
                case SeinDirective::Kind::Blank:
                    buf += '\n';
                    break;
                case SeinDirective::Kind::Comment:
                    buf += "# "; buf += d.operand; buf += '\n';
                    break;
                case SeinDirective::Kind::Include:
                    buf += "@include \""; buf += d.operand; buf += '"';
                    detail::write_inline_comment(buf, d.comment);
                    buf += '\n';
                    break;
                case SeinDirective::Kind::Set:
                    buf += "@set "; buf += d.operand; buf += " = ";
                    buf += detail::quote_if_needed(d.value);
                    detail::write_inline_comment(buf, d.comment);
                    buf += '\n';
                    break;
            }
        }

        if (!doc.directives.empty() && !doc.sections.empty())
            buf += '\n';

        for (size_t i = 0; i < doc.sections.size(); ++i) {
            const auto& sec = doc.sections[i];
            if (!sec.comment.empty()) { buf += "# "; buf += sec.comment; buf += '\n'; }
            buf += '['; buf += sec.name; buf += "]\n";
            for (const auto& kv : sec.entries) {
                buf += kv.key; buf += " = ";
                buf += detail::quote_if_needed(kv.value);
                detail::write_inline_comment(buf, kv.comment);
                buf += '\n';
            }
            if (i + 1 < doc.sections.size()) buf += '\n';
        }

        fwrite(buf.data(), 1, buf.size(), f);
        fclose(f);
        return true;
    }

    inline bool doc_save_config(const SeinDocument& doc)
    {
        return doc_save_config(doc, doc.path);
    }

    inline SeinDocument doc_load_as_document(std::string_view path)
    {
        SeinDocument doc;
        doc.path = std::string(path);

        FILE *f = fopen(doc.path.c_str(), "r");
        if (!f) {
            std::cerr << "[SEIN Writer]: Failed to open: " << path << "\n";
            return doc;
        }

        std::string current_section;
        bool in_header = true;
        char line_c[2048];

        while (fgets(line_c, sizeof(line_c), f)) {
            std::string_view line = line_c;
            while (!line.empty() && (line.back() == '\n' || line.back() == '\r'))
                line.remove_suffix(1);
            auto sv = detail::trim_view(line);

            if (sv.empty()) {
                if (in_header) doc_add_blank_line(doc);
                continue;
            }
            if (sv.front() == '#') {
                if (in_header)
                    doc_add_header_comment(doc, detail::trim_view(sv.substr(1)));
                continue;
            }
            if (sv.size() >= 8 && sv.substr(0, 8) == "@include") {
                auto inc = detail::strip_quotes(detail::trim_view(sv.substr(8)));
                doc_add_include(doc, inc);
                continue;
            }
            if (sv.size() >= 4 && sv.substr(0, 4) == "@set") {
                auto rest = detail::trim_view(sv.substr(4));
                auto eq   = rest.find('=');
                if (eq != std::string_view::npos) {
                    auto name = detail::trim(rest.substr(0, eq));
                    auto val  = detail::strip_quotes(detail::trim(rest.substr(eq + 1)));
                    doc_add_global_var(doc, name, val);
                }
                continue;
            }
            if (sv.front() == '[' && sv.back() == ']') {
                in_header = false;
                current_section = detail::trim(sv.substr(1, sv.size() - 2));
                doc_add_section(doc, current_section);
                continue;
            }
            auto eq = sv.find('=');
            if (eq != std::string_view::npos && !current_section.empty()) {
                auto key = detail::trim(sv.substr(0, eq));
                auto val = detail::trim(sv.substr(eq + 1));
                auto hash = val.find('#');
                if (hash != std::string::npos)
                    val = detail::trim(val.substr(0, hash));
                if (val.size() >= 2 && val.front() == '"' && val.back() == '"')
                    val = val.substr(1, val.size() - 2);
                doc_add_value(doc, current_section, key, val);
            }
        }

        fclose(f);
        return doc;
    }

} // namespace fd::sein //