#ifndef UTIL_H
#define UTIL_H

#include <cctype>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <set>
#include <sstream>
#include <string>
#include <vector>

inline std::string utilTrim(const std::string& s) {
    size_t b = 0;
    size_t e = s.size();
    while (b < e && std::isspace(static_cast<unsigned char>(s[b]))) ++b;
    while (e > b && std::isspace(static_cast<unsigned char>(s[e - 1]))) --e;
    return s.substr(b, e - b);
}

inline std::vector<std::string> utilSplitWhitespace(const std::string& s) {
    std::vector<std::string> out;
    std::istringstream is(s);
    std::string tok;
    while (is >> tok) out.push_back(tok);
    return out;
}

inline std::string utilJoin(const std::vector<std::string>& v, const std::string& sep) {
    std::string out;
    for (size_t i = 0; i < v.size(); ++i) {
        if (i) out += sep;
        out += v[i];
    }
    return out;
}

inline std::string utilFormatSet(const std::set<std::string>& s, const std::string& sep = ", ") {
    if (s.empty()) return "{ }";
    std::string out = "{ ";
    bool first = true;
    for (const std::string& x : s) {
        if (!first) out += sep;
        first = false;
        out += x;
    }
    out += " }";
    return out;
}

inline bool utilReadFile(const std::string& path, std::string& out, std::string& error) {
    std::ifstream in(path.c_str(), std::ios::binary);
    if (!in) {
        error = "cannot open file: " + path;
        return false;
    }
    std::ostringstream ss;
    ss << in.rdbuf();
    out = ss.str();
    return true;
}

inline std::string utilRepeat(char c, int n) {
    return std::string(n > 0 ? (size_t)n : 0, c);
}

inline void utilBanner(std::ostream& os, const std::string& title, char ch = '=') {
    std::string line = utilRepeat(ch, 60);
    os << line << "\n" << title << "\n" << line << "\n";
}

inline void utilSection(std::ostream& os, const std::string& title) {
    os << "\n" << utilRepeat('-', 60) << "\n" << title << "\n" << utilRepeat('-', 60) << "\n";
}

inline std::string utilDirName(const std::string& path) {
    size_t p = path.find_last_of("/\\");
    if (p == std::string::npos) return ".";
    if (p == 0) return path.substr(0, 1);
    return path.substr(0, p);
}

#endif
