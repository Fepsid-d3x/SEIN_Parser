#pragma once
#include <string>
#include <string_view>
#include <vector>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <iostream>
#include <unordered_map>

namespace fd::sein {

    namespace detail {

    inline std::string_view trim_view(std::string_view s) {
        static constexpr std::string_view ws = " \t\r\n";
        size_t start = s.find_first_not_of(ws);
        if (start == std::string_view::npos) return "";
        size_t end = s.find_last_not_of(ws);
        return s.substr(start, end - start + 1);
    }

    inline std::string trim(std::string_view s) {
        auto v = trim_view(s);
        return std::string(v.begin(), v.end());
    }

    inline std::string strip_comment(std::string_view line) {
        size_t pos = line.find('#');
        return std::string(line.substr(0, pos != std::string_view::npos ? pos : line.size()));
    }

    inline std::string_view strip_quotes_view(std::string_view val) {
        if (val.size() >= 2 && val.front() == '"' && val.back() == '"')
            return val.substr(1, val.size() - 2);
        return val;
    }

    inline std::string strip_quotes(std::string_view val) {
        auto v = strip_quotes_view(val);
        return std::string(v.begin(), v.end());
    }

    inline std::string dedent(std::string_view s) {
        std::istringstream ss{std::string(s)};
        std::vector<std::string> lines;
        std::string line_buffer;
        while (std::getline(ss, line_buffer)) {
            lines.push_back(line_buffer);
        }

        size_t min_indent = std::string::npos;
        for (const auto& l : lines) {
            size_t first = l.find_first_not_of(" \t");
            if (first == std::string::npos) continue;
            min_indent = (min_indent == std::string::npos) ? first : std::min(min_indent, first);
        }
        if (min_indent == std::string::npos) min_indent = 0;

        std::string result;
        for (size_t i = 0; i < lines.size(); ++i) {
            const auto& l = lines[i];
            result += l.size() >= min_indent ? l.substr(min_indent) : l;
            if (i + 1 < lines.size()) result += '\n';
        }
        return result;
    }

    inline std::string substitute_value(std::string_view val,
        const std::unordered_map<std::string, std::unordered_map<std::string, std::string>>& config,
        int depth = 0)
    {
        if (depth > 8 || val.empty()) return std::string(val);
        
        std::string result;
        result.reserve(val.size() + 64);
        size_t pos = 0;
        
        while (pos < val.size()) {
            size_t start = val.find('$', pos);
            if (start == std::string_view::npos) {
                result.append(val.begin() + pos, val.end());
                break;
            }
            
            size_t brace_start = start + 1;
            if (brace_start < val.size() && val[brace_start] == '{') {
                size_t brace_end = val.find('}', brace_start + 1);
                if (brace_end != std::string_view::npos) {
                    result.append(val.begin() + pos, val.begin() + start);
                    auto var_name = val.substr(brace_start + 1, brace_end - brace_start - 1);
                    
                    size_t dot_pos = var_name.find('.');
                    if (dot_pos != std::string_view::npos) {
                        auto section = var_name.substr(0, dot_pos);
                        auto key = var_name.substr(dot_pos + 1);
                        auto s_it = config.find(std::string(section));
                        if (s_it != config.end()) {
                            auto k_it = s_it->second.find(std::string(key));
                            if (k_it != s_it->second.end()) {
                                result += k_it->second;
                            }
                        }
                    } else {
                        std::string var_name_str(var_name);
                        const char *env_val = std::getenv(var_name_str.c_str());
                        if (env_val) result += env_val;
                    }
                    
                    pos = brace_end + 1;
                    continue;
                }
            }
            
            result.append(val.begin() + pos, val.begin() + start + 1);
            pos = start + 1;
        }
        
        return result;
    }

    inline void parse_file(std::string_view path,
        std::unordered_map<std::string, std::unordered_map<std::string, std::string>>& result,
        int depth = 0, bool substitute = true)
    {
        if (depth > 8) {
            std::cerr << "[SEIN Parser]: @include depth limit reached at: " << path << "\n";
            return;
        }

        std::ifstream file{std::string(path)};
        if (!file.is_open()) {
            std::cerr << "[SEIN Parser]: Failed to open: " << path << "\n";
            return;
        }

        std::string base_dir;
        size_t slash = path.find_last_of("/\\");
        if (slash != std::string::npos)
            base_dir.assign(path.begin(), path.begin() + slash + 1);

        std::string current_section = "Default";
        std::string line;

        std::string multiline_key;
        std::string multiline_val;
        bool in_multiline = false;

        std::string raw_key;
        std::string raw_val;
        bool in_raw = false;

        while (std::getline(file, line)) {
            if (in_raw) {
                size_t end_pos = line.find(")R\"");
                if (end_pos != std::string::npos) {
                    raw_val += line.substr(0, end_pos);
                    auto final_val = dedent(raw_val);
                    result[current_section][raw_key] = substitute ? substitute_value(final_val, result) : final_val;
                    raw_key.clear();
                    raw_val.clear();
                    in_raw = false;
                } else {
                    raw_val.reserve(raw_val.size() + line.size() + 1);
                    raw_val += line;
                    raw_val += '\n';
                }
                continue;
            }

            if (in_multiline) {
                std::string clean_stripped = strip_comment(line); 
                auto clean_line = trim_view(clean_stripped);
                bool continues = (!clean_line.empty() && clean_line.back() == '\\');
                if (continues) clean_line.remove_suffix(1);
                auto stripped = trim_view(clean_line);
                multiline_val += ' ';
                multiline_val.append(stripped.begin(), stripped.end());
                if (!continues) {
                    auto final_val = strip_quotes(trim_view(multiline_val));
                    result[current_section][multiline_key] = substitute ? substitute_value(final_val, result) : final_val;
                    multiline_key.clear();
                    multiline_val.clear();
                    in_multiline = false;
                }
                continue;
            }

            std::string clean_stripped = strip_comment(line);
            auto clean_sv = trim_view(clean_stripped);
            if (clean_sv.empty()) continue;

            if (clean_sv.substr(0, std::min(size_t(8), clean_sv.size())) == "@include") {
                auto inc = trim_view(clean_sv.substr(8));
                inc = strip_quotes_view(inc);
                std::string inc_path(base_dir);
                inc_path.append(inc.begin(), inc.end());
                parse_file(inc_path, result, depth + 1);
                continue;
            }

            if (clean_sv.front() == '[' && clean_sv.back() == ']') {
                current_section = trim(clean_sv.substr(1, clean_sv.size() - 2));
                continue;
            }

            size_t sep = clean_sv.find('=');
            if (sep == std::string_view::npos) continue;

            auto key_view = trim_view(clean_sv.substr(0, sep));
            auto val_view = trim_view(clean_sv.substr(sep + 1));
            std::string key(key_view.begin(), key_view.end());

            if (val_view.size() >= 3 && val_view.substr(0, 3) == "\"R(") {
                size_t end_pos = val_view.find(")R\"", 3);
                if (end_pos != std::string::npos) {
                    auto raw_str = std::string(val_view.substr(3, end_pos - 3));
                    result[current_section][key] = substitute ? substitute_value(raw_str, result) : raw_str;
                } else {
                    raw_key = key;
                    raw_val.assign(val_view.begin() + 3, val_view.end());
                    raw_val += '\n';
                    in_raw = true;
                }
                continue;
            }

            if (!val_view.empty() && val_view.back() == '\\') {
                auto val_trimmed = trim_view(val_view.substr(0, val_view.size() - 1));
                multiline_key = key;
                multiline_val.assign(val_trimmed.begin(), val_trimmed.end());
                in_multiline = true;
                continue;
            }

            auto final_val = strip_quotes_view(val_view);
            auto val_str = std::string(final_val.begin(), final_val.end());
            result[current_section][key] = substitute ? substitute_value(val_str, result) : val_str;
        }
    }

    }

    //////////////////////// parse_sein ///////////////////////////////////////
    inline std::unordered_map<std::string, std::unordered_map<std::string, std::string>>
    parse_sein(std::string_view path)
    {
        std::unordered_map<std::string, std::unordered_map<std::string, std::string>> result;
        result.reserve(8);
        detail::parse_file(path, result);
        return result;
    }

    ///////////////////////////// Getters ////////////////////////////////////
    using Config = std::unordered_map<std::string, std::unordered_map<std::string, std::string>>;

    inline std::string_view get_value_view(const Config& cfg,
        std::string_view section, std::string_view key)
    {
        auto s = cfg.find(std::string(section));
        if (s == cfg.end()) return "";
        auto k = s->second.find(std::string(key));
        if (k == s->second.end()) return "";
        return k->second;
    }

    inline std::string get_value(const Config& cfg,
        std::string_view section, std::string_view key,
        std::string_view fallback = "")
    {
        auto v = get_value_view(cfg, section, key);
        if (v.empty()) return std::string(fallback);
        return std::string(v);
    }

    inline float get_float(const Config& cfg, std::string_view section,
                           std::string_view key, float fallback = 0.f)
    {
        auto val = get_value(cfg, section, key);
        if (val.empty()) return fallback;
        try { return std::stof(val); } catch (...) { return fallback; }
    }

    inline int get_int(const Config& cfg, std::string_view section,
                       std::string_view key, int fallback = 0)
    {
        auto val = get_value(cfg, section, key);
        if (val.empty()) return fallback;
        try { return std::stoi(val); } catch (...) { return fallback; }
    }

    inline bool get_bool(const Config& cfg, std::string_view section,
                         std::string_view key, bool fallback = false)
    {
        auto val = get_value(cfg, section, key);
        if (val.empty()) return fallback;
        if (val == "true"  || val == "yes" || val == "1") return true;
        if (val == "false" || val == "no"  || val == "0") return false;
        std::string lower(val);
        std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
        if (lower == "true"  || lower == "yes" || lower == "1") return true;
        if (lower == "false" || lower == "no"  || lower == "0") return false;
        return fallback;
    }

    /////////////////////// Array helpers //////////////////////////////
    inline std::vector<std::string> split_value(std::string_view val, char delim = ';') {
        std::vector<std::string> result;
        size_t start = 0;
        while (start < val.size()) {
            size_t end = val.find(delim, start);
            if (end == std::string_view::npos) end = val.size();
            auto token = detail::trim_view(val.substr(start, end - start));
            if (!token.empty()) result.emplace_back(token.begin(), token.end());
            start = end + 1;
        }
        return result;
    }

    inline std::vector<std::string> get_array(const Config& cfg,
        std::string_view section, std::string_view key, char delim = ';')
    {
        auto val = get_value(cfg, section, key);
        return split_value(val, delim);
    }

    inline std::vector<float> get_float_array(const Config& cfg,
        std::string_view section, std::string_view key, char delim = ';')
    {
        auto tokens = get_array(cfg, section, key, delim);
        std::vector<float> result;
        result.reserve(tokens.size());
        for (const auto& t : tokens) {
            try { result.push_back(std::stof(t)); } catch (...) { result.push_back(0.f); }
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
            try { result.push_back(std::stoi(t)); } catch (...) { result.push_back(0); }
        }
        return result;
    }

}