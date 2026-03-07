#pragma once
#include <string>
#include <vector>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <iostream>
#include <unordered_map>

namespace fd::sein {

    namespace detail {

    inline std::string trim(const std::string& s) {
        const char* ws = " \t\r\n";
        size_t start = s.find_first_not_of(ws);
        if (start == std::string::npos) return "";
        size_t end = s.find_last_not_of(ws);
        return s.substr(start, end - start + 1);
    }

    inline std::string strip_comment(const std::string& line) {
        size_t pos = line.find('#');
        return (pos != std::string::npos) ? line.substr(0, pos) : line;
    }

    inline std::string strip_quotes(const std::string& val) {
        if (val.size() >= 2 && val.front() == '"' && val.back() == '"')
            return val.substr(1, val.size() - 2);
        return val;
    }

    inline std::string dedent(const std::string& s) {
        std::istringstream ss(s);
        std::vector<std::string> lines;
        std::string ln;
        while (std::getline(ss, ln)) lines.push_back(ln);

        size_t min_indent = std::string::npos;
        for (const auto& l : lines) {
            size_t first = l.find_first_not_of(" \t");
            if (first == std::string::npos) continue;
            if (min_indent == std::string::npos || first < min_indent)
                min_indent = first;
        }
        if (min_indent == std::string::npos) min_indent = 0;

        std::string result;
        for (size_t i = 0; i < lines.size(); ++i) {
            const auto& l = lines[i];
            if (l.size() >= min_indent)
                result += l.substr(min_indent);
            else
                result += l;
            if (i + 1 < lines.size()) result += '\n';
        }
        return result;
    }

    inline void parse_file(const std::string& path,
        std::unordered_map<std::string, std::unordered_map<std::string, std::string>>& result,
        int depth = 0)
    {
        if (depth > 8) {
            std::cerr << "[SEIN Parser]: @include depth limit reached at: " << path << "\n";
            return;
        }

        std::ifstream file(path);
        if (!file.is_open()) {
            std::cerr << "[SEIN Parser]: Failed to open: " << path << "\n";
            return;
        }

        std::string base_dir;
        size_t slash = path.find_last_of("/\\");
        if (slash != std::string::npos)
            base_dir = path.substr(0, slash + 1);

        std::string current_section = "Default";
        std::string line;

        std::string multiline_key;
        std::string multiline_val;
        bool in_multiline = false;

        // raw-string R(...)R state
        std::string raw_key;
        std::string raw_val;
        bool in_raw = false;

        while (std::getline(file, line)) {

            // inside a raw-string block — collect lines until ")R" is found
            if (in_raw) {
                size_t end_pos = line.find(")R\"");
                if (end_pos != std::string::npos) {
                    // append the part before the closing marker
                    raw_val += line.substr(0, end_pos);
                    result[current_section][raw_key] = dedent(raw_val);
                    raw_key.clear();
                    raw_val.clear();
                    in_raw = false;
                } else {
                    raw_val += line + "\n";
                }
                continue;
            }

            if (in_multiline) {
                std::string stripped = trim(strip_comment(line));
                bool continues = (!stripped.empty() && stripped.back() == '\\');
                if (continues) stripped.pop_back();
                stripped = trim(stripped);
                multiline_val += " " + stripped;
                if (!continues) {
                    result[current_section][multiline_key] = strip_quotes(trim(multiline_val));
                    multiline_key.clear();
                    multiline_val.clear();
                    in_multiline = false;
                }
                continue;
            }

            std::string clean = trim(strip_comment(line));
            if (clean.empty()) continue;

            // @include "other.sein"
            if (clean.rfind("@include", 0) == 0) {
                std::string inc = trim(clean.substr(8));
                if (inc.size() >= 2 && inc.front() == '"' && inc.back() == '"')
                    inc = inc.substr(1, inc.size() - 2);
                parse_file(base_dir + inc, result, depth + 1);
                continue;
            }

            // [Section]
            if (clean.front() == '[' && clean.back() == ']') {
                current_section = trim(clean.substr(1, clean.size() - 2));
                continue;
            }

            // key = value
            size_t sep = clean.find('=');
            if (sep == std::string::npos) continue;

            std::string key = trim(clean.substr(0, sep));
            std::string val = trim(clean.substr(sep + 1));

            if (val.size() >= 3 && val.substr(0, 3) == "\"R(") {
                size_t end_pos = val.find(")R\"", 3);
                if (end_pos != std::string::npos) {
                    // single-line raw string
                    result[current_section][key] = val.substr(3, end_pos - 3);
                } else {
                    // multi-line raw string — collect until )R"
                    raw_key = key;
                    raw_val  = val.substr(3) + "\n"; // content after "R(
                    in_raw   = true;
                }
                continue;
            }

            if (!val.empty() && val.back() == '\\') {
                val.pop_back();
                multiline_key = key;
                multiline_val = trim(val);
                in_multiline  = true;
                continue;
            }

            result[current_section][key] = strip_quotes(val);
        }
    }

    }

    //////////////////////// parse_sein ///////////////////////////////////////
    inline std::unordered_map<std::string, std::unordered_map<std::string, std::string>>
    parse_sein(const std::string& path)
    {
        std::unordered_map<std::string, std::unordered_map<std::string, std::string>> result;
        detail::parse_file(path, result);
        return result;
    }

    ///////////////////////////// Getters ////////////////////////////////////
    using Config = std::unordered_map<std::string, std::unordered_map<std::string, std::string>>;

    inline std::string get_value(const Config& cfg,
        const std::string& section, const std::string& key,
        const std::string& fallback = "")
    {
        auto s = cfg.find(section);
        if (s == cfg.end()) return fallback;
        auto k = s->second.find(key);
        if (k == s->second.end()) return fallback;
        return k->second;
    }

    inline float get_float(const Config& cfg, const std::string& section,
                           const std::string& key, float fallback = 0.f)
    {
        auto val = get_value(cfg, section, key);
        if (val.empty()) return fallback;
        try { return std::stof(val); } catch (...) { return fallback; }
    }

    inline int get_int(const Config& cfg, const std::string& section,
                       const std::string& key, int fallback = 0)
    {
        auto val = get_value(cfg, section, key);
        if (val.empty()) return fallback;
        try { return std::stoi(val); } catch (...) { return fallback; }
    }

    // true/false; yes/no; 1/0
    inline bool get_bool(const Config& cfg, const std::string& section,
                         const std::string& key, bool fallback = false)
    {
        auto val = get_value(cfg, section, key);
        if (val.empty()) return fallback;
        std::string lower = val;
        std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
        if (lower == "true"  || lower == "yes" || lower == "1") return true;
        if (lower == "false" || lower == "no"  || lower == "0") return false;
        return fallback;
    }

    /////////////////////// Array helpers //////////////////////////////
    inline std::vector<std::string> split_value(const std::string& val, char delim = ';') {
        std::vector<std::string> result;
        std::stringstream ss(val);
        std::string token;
        while (std::getline(ss, token, delim)) {
            token = detail::trim(token);
            if (!token.empty()) result.push_back(token);
        }
        return result;
    }

    inline std::vector<std::string> get_array(const Config& cfg,
        const std::string& section, const std::string& key, char delim = ';')
    {
        return split_value(get_value(cfg, section, key), delim);
    }

    inline std::vector<float> get_float_array(const Config& cfg,
        const std::string& section, const std::string& key, char delim = ';')
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
        const std::string& section, const std::string& key, char delim = ';')
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