#include "env_loader.h"
#include <fstream>
#include <iostream>
#include <algorithm>
#include <cctype>
#include <map>       // <-- обязательно добавить

std::map<std::string, std::string> EnvLoader::variables;   // <-- правильное определение

bool EnvLoader::load(const std::string& filename) {
    std::ifstream file(filename);
    if (!file.is_open()) return false;
    std::string line;
    while (std::getline(file, line)) {
        // обрезаем пробелы в начале
        line.erase(line.begin(), std::find_if(line.begin(), line.end(),
            [](int ch) { return !std::isspace(ch); }));
        if (line.empty() || line[0] == '#') continue;
        size_t pos = line.find('=');
        if (pos != std::string::npos) {
            std::string key = line.substr(0, pos);
            std::string value = line.substr(pos + 1);
            value.erase(value.find_last_not_of(" \t\r\n") + 1);
            variables[key] = value;
        }
    }
    return true;
}

std::string EnvLoader::get(const std::string& key, const std::string& defaultValue) {
    auto it = variables.find(key);
    return (it != variables.end()) ? it->second : defaultValue;
}
