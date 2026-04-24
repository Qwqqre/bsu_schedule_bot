#pragma once

#include <string>
#include <map>

class EnvLoader {
public:
    static bool load(const std::string& filename);
    static std::string get(const std::string& key, const std::string& defaultValue = "");

private:
    static std::map<std::string, std::string> variables;
};
