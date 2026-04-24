#pragma once

#include <string>
#include <vector>
#include <gumbo.h>   // ← обязательно

struct Lesson {
    std::string time;
    std::string subject;
    std::string type;
    std::string teacher;
    std::string room;
};

struct DaySchedule {
    std::string dayName;
    std::vector<Lesson> lessons;
};

using WeekSchedule = std::vector<DaySchedule>;

class ScheduleParser {
public:
    static std::string getScheduleText(int course, int group);

private:
    static std::string downloadPage(const std::string& url);
    static std::string extractText(GumboNode* node);           // убрали class
    static std::string cleanText(const std::string& text);
    static void searchForScheduleTable(GumboNode* node, std::vector<GumboNode*>& tables);
    static WeekSchedule parse(const std::string& html);
    static std::string formatSchedule(const WeekSchedule& schedule, int course, int group);
    static std::string buildUrl(int course, int group);
};
