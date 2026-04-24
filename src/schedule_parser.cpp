#include "schedule_parser.h"
#include "env_loader.h"
#include <curl/curl.h>
#include <gumbo.h>
#include <iostream>
#include <sstream>
#include <algorithm>
#include <cctype>
#include <functional>
#include <map>
#include <ctime>

// ==================== ВСПОМОГАТЕЛЬНЫЕ ФУНКЦИИ ДЛЯ UTF-8 ====================
static bool isValidUtf8(const std::string& s) {
    const unsigned char* bytes = reinterpret_cast<const unsigned char*>(s.data());
    size_t len = s.size();
    size_t i = 0;
    while (i < len) {
        if (bytes[i] <= 0x7F) {
            i++;
        } else if ((bytes[i] & 0xE0) == 0xC0) {
            if (i+1 >= len || (bytes[i+1] & 0xC0) != 0x80) return false;
            i += 2;
        } else if ((bytes[i] & 0xF0) == 0xE0) {
            if (i+2 >= len || (bytes[i+1] & 0xC0) != 0x80 || (bytes[i+2] & 0xC0) != 0x80) return false;
            i += 3;
        } else if ((bytes[i] & 0xF8) == 0xF0) {
            if (i+3 >= len || (bytes[i+1] & 0xC0) != 0x80 || (bytes[i+2] & 0xC0) != 0x80 || (bytes[i+3] & 0xC0) != 0x80) return false;
            i += 4;
        } else {
            return false;
        }
    }
    return true;
}

static std::string fixUtf8(const std::string& s) {
    if (isValidUtf8(s)) return s;
    std::string result;
    const unsigned char* bytes = reinterpret_cast<const unsigned char*>(s.data());
    size_t len = s.size();
    size_t i = 0;
    while (i < len) {
        if (bytes[i] <= 0x7F) {
            result += static_cast<char>(bytes[i]);
            i++;
        } else if ((bytes[i] & 0xE0) == 0xC0 && i+1 < len && (bytes[i+1] & 0xC0) == 0x80) {
            result += static_cast<char>(bytes[i]);
            result += static_cast<char>(bytes[i+1]);
            i += 2;
        } else if ((bytes[i] & 0xF0) == 0xE0 && i+2 < len && (bytes[i+1] & 0xC0) == 0x80 && (bytes[i+2] & 0xC0) == 0x80) {
            result += static_cast<char>(bytes[i]);
            result += static_cast<char>(bytes[i+1]);
            result += static_cast<char>(bytes[i+2]);
            i += 3;
        } else if ((bytes[i] & 0xF8) == 0xF0 && i+3 < len && (bytes[i+1] & 0xC0) == 0x80 && (bytes[i+2] & 0xC0) == 0x80 && (bytes[i+3] & 0xC0) == 0x80) {
            result += static_cast<char>(bytes[i]);
            result += static_cast<char>(bytes[i+1]);
            result += static_cast<char>(bytes[i+2]);
            result += static_cast<char>(bytes[i+3]);
            i += 4;
        } else {
            result += '?';
            i++;
        }
    }
    return result;
}
// ========================================================================

static size_t WriteCallback(void* contents, size_t size, size_t nmemb, void* userp) {
    ((std::string*)userp)->append((char*)contents, size * nmemb);
    return size * nmemb;
}

std::string ScheduleParser::downloadPage(const std::string& url) {
    CURL* curl = curl_easy_init();
    std::string response;
    if (curl) {
        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
        curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, 30L);
        curl_easy_setopt(curl, CURLOPT_USERAGENT, "Mozilla/5.0 (Macintosh; Intel Mac OS X 10_15_7) AppleWebKit/537.36");
        CURLcode res = curl_easy_perform(curl);
        if (res != CURLE_OK) {
            std::cerr << "curl error: " << curl_easy_strerror(res) << std::endl;
            response.clear();
        }
        curl_easy_cleanup(curl);
    }
    return response;
}

std::string ScheduleParser::extractText(GumboNode* node) {
    if (node->type == GUMBO_NODE_TEXT) {
        return fixUtf8(std::string(node->v.text.text));
    }
    if (node->type != GUMBO_NODE_ELEMENT) return "";
    std::string result;
    GumboVector* children = &node->v.element.children;
    for (unsigned int i = 0; i < children->length; ++i) {
        GumboNode* child = (GumboNode*)children->data[i];
        std::string text = extractText(child);
        if (!text.empty()) {
            if (!result.empty() && result.back() != ' ' && text.front() != ' ') result += ' ';
            result += text;
        }
    }
    if (node->v.element.tag == GUMBO_TAG_BR) result += '\n';
    return result;
}

std::string ScheduleParser::cleanText(const std::string& text) {
    std::string result;
    bool lastSpace = false;
    for (char c : text) {
        if (c == '\n') {
            result += '\n';
            lastSpace = false;
        } else if (std::isspace(static_cast<unsigned char>(c))) {
            if (!lastSpace && !result.empty() && result.back() != '\n') {
                result += ' ';
                lastSpace = true;
            }
        } else {
            result += c;
            lastSpace = false;
        }
    }
    size_t start = result.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) return "";
    size_t end = result.find_last_not_of(" \t\r\n");
    return result.substr(start, end - start + 1);
}

void ScheduleParser::searchForScheduleTable(GumboNode* node, std::vector<GumboNode*>& tables) {
    if (node->type != GUMBO_NODE_ELEMENT) return;
    if (node->v.element.tag == GUMBO_TAG_TABLE) {
        tables.push_back(node);
    }
    GumboVector* children = &node->v.element.children;
    for (unsigned int i = 0; i < children->length; ++i) {
        searchForScheduleTable((GumboNode*)children->data[i], tables);
    }
}

WeekSchedule ScheduleParser::parse(const std::string& html) {
    WeekSchedule schedule;
    if (html.empty()) return schedule;

    GumboOutput* output = gumbo_parse(html.c_str());
    if (!output) return schedule;

    std::vector<GumboNode*> tables;
    searchForScheduleTable(output->root, tables);

    if (tables.empty()) {
        gumbo_destroy_output(&kGumboDefaultOptions, output);
        return schedule;
    }

    // Ищем таблицу с расписанием (по наличию слов "День" и "Время" в первой строке)
    GumboNode* targetTable = nullptr;
    for (GumboNode* table : tables) {
        std::vector<GumboNode*> rows;
        std::function<void(GumboNode*)> findRows = [&](GumboNode* n) {
            if (n->type == GUMBO_NODE_ELEMENT && n->v.element.tag == GUMBO_TAG_TR)
                rows.push_back(n);
            if (n->type == GUMBO_NODE_ELEMENT) {
                GumboVector* ch = &n->v.element.children;
                for (unsigned i = 0; i < ch->length; ++i)
                    findRows(static_cast<GumboNode*>(ch->data[i]));
            }
        };
        findRows(table);
        if (rows.empty()) continue;
        GumboNode* headerRow = rows[0];
        std::string headerText = cleanText(extractText(headerRow));
        if (headerText.find("День") != std::string::npos && 
            headerText.find("Время") != std::string::npos) {
            targetTable = table;
            break;
        }
    }
    if (!targetTable) {
        for (GumboNode* table : tables) {
            std::vector<GumboNode*> rows;
            std::function<void(GumboNode*)> findRows = [&](GumboNode* n) {
                if (n->type == GUMBO_NODE_ELEMENT && n->v.element.tag == GUMBO_TAG_TR)
                    rows.push_back(n);
                if (n->type == GUMBO_NODE_ELEMENT) {
                    GumboVector* ch = &n->v.element.children;
                    for (unsigned i = 0; i < ch->length; ++i)
                        findRows(static_cast<GumboNode*>(ch->data[i]));
                }
            };
            findRows(table);
            if (rows.size() > 10) {
                targetTable = table;
                break;
            }
        }
    }
    if (!targetTable) {
        gumbo_destroy_output(&kGumboDefaultOptions, output);
        return schedule;
    }

    // Собираем все строки целевой таблицы
    std::vector<GumboNode*> allRows;
    std::function<void(GumboNode*)> findAllRows = [&](GumboNode* n) {
        if (n->type == GUMBO_NODE_ELEMENT && n->v.element.tag == GUMBO_TAG_TR)
            allRows.push_back(n);
        if (n->type == GUMBO_NODE_ELEMENT) {
            GumboVector* ch = &n->v.element.children;
            for (unsigned i = 0; i < ch->length; ++i)
                findAllRows(static_cast<GumboNode*>(ch->data[i]));
        }
    };
    findAllRows(targetTable);
    if (allRows.size() < 2) {
        gumbo_destroy_output(&kGumboDefaultOptions, output);
        return schedule;
    }

    std::vector<std::string> dayNames = {"Понедельник", "Вторник", "Среда", "Четверг", "Пятница", "Суббота"};
    DaySchedule currentDay;
    std::string currentDayName;

    // Обрабатываем строки таблицы, начиная с первой (индекс 0 — заголовок, индекс 1 — первая строка с данными)
    for (size_t i = 1; i < allRows.size(); ++i) {
        GumboNode* row = allRows[i];
        std::vector<GumboNode*> cells;
        std::function<void(GumboNode*)> findCells = [&](GumboNode* n) {
            if (n->type == GUMBO_NODE_ELEMENT && (n->v.element.tag == GUMBO_TAG_TD || n->v.element.tag == GUMBO_TAG_TH))
                cells.push_back(n);
            if (n->type == GUMBO_NODE_ELEMENT) {
                GumboVector* ch = &n->v.element.children;
                for (unsigned j = 0; j < ch->length; ++j)
                    findCells(static_cast<GumboNode*>(ch->data[j]));
            }
        };
        findCells(row);
        if (cells.empty()) continue;

        std::string firstCell = cleanText(extractText(cells[0]));
        bool isDayRow = false;
        std::string newDayName;
        for (const auto& dn : dayNames) {
            if (firstCell.find(dn) != std::string::npos) {
                isDayRow = true;
                newDayName = dn;
                break;
            }
        }

        if (isDayRow && newDayName != currentDayName) {
            // Завершаем предыдущий день
            if (!currentDay.dayName.empty()) {
                schedule.push_back(currentDay);
            }
            currentDay = DaySchedule();
            currentDay.dayName = newDayName;
            currentDayName = newDayName;

            // --------------------------------------------------------------
            // ИСПРАВЛЕНИЕ: если в строке с днём есть дополнительные ячейки
            // (начиная со второй), то извлекаем из них первое занятие
            // --------------------------------------------------------------
            if (cells.size() > 1) {
                Lesson lesson;
                // Время обычно во второй ячейке (индекс 1)
                if (cells.size() > 1) {
                    lesson.time = cleanText(extractText(cells[1]));
                    size_t dash = lesson.time.find("–");
                    if (dash != std::string::npos)
                        lesson.time = lesson.time.substr(0, dash);
                }
                // Предполагаемая структура: 1-я ячейка - день, 2-я - время, 3-я - предмет, 4-я - тип, 5-я - аудитория
                if (cells.size() > 2) {
                    std::string subj = cleanText(extractText(cells[2]));
                    std::replace(subj.begin(), subj.end(), '\n', ' ');
                    lesson.subject = subj;
                }
                if (cells.size() > 3) {
                    lesson.type = cleanText(extractText(cells[3]));
                }
                if (cells.size() > 4) {
                    lesson.room = cleanText(extractText(cells[4]));
                }
                if (!lesson.subject.empty() || !lesson.time.empty()) {
                    currentDay.lessons.push_back(lesson);
                }
            }
        }
        else if (!currentDayName.empty() && cells.size() >= 2) {
            // Обычная строка с занятием
            Lesson lesson;
            lesson.time = cleanText(extractText(cells[1]));
            size_t dash = lesson.time.find("–");
            if (dash != std::string::npos)
                lesson.time = lesson.time.substr(0, dash);

            int subjectCol = 2;
            if (cells.size() > 3) {
                std::string thirdCell = cleanText(extractText(cells[2]));
                if (thirdCell.find("гр") != std::string::npos || thirdCell.find("группа") != std::string::npos) {
                    subjectCol = 3;
                } else {
                    subjectCol = 2;
                }
            }
            if (cells.size() > subjectCol) {
                std::string subj = cleanText(extractText(cells[subjectCol]));
                std::replace(subj.begin(), subj.end(), '\n', ' ');
                lesson.subject = subj;
            }
            if (cells.size() > subjectCol + 1) {
                lesson.type = cleanText(extractText(cells[subjectCol + 1]));
            }
            if (cells.size() > subjectCol + 2) {
                lesson.room = cleanText(extractText(cells[subjectCol + 2]));
            }
            if (!lesson.subject.empty() || !lesson.time.empty()) {
                currentDay.lessons.push_back(lesson);
            }
        }
    }

    if (!currentDay.dayName.empty()) {
        schedule.push_back(currentDay);
    }

    gumbo_destroy_output(&kGumboDefaultOptions, output);
    return schedule;
}

std::string ScheduleParser::formatSchedule(const WeekSchedule& schedule, int course, int group) {
    if (schedule.empty()) {
        return "😔 Расписание для " + std::to_string(course) + " курса, группы " + std::to_string(group) +
               " не найдено.\nПроверьте правильность данных или посмотрите на сайте: https://mmf.bsu.by";
    }
    std::ostringstream oss;
    oss << "📚 *Расписание занятий*\n🎓 Мехмат БГУ | " << course << " курс, " << group << " группа\n━━━━━━━━━━━━━━━━━━━━━━━━━\n\n";
    for (const auto& day : schedule) {
        std::string emoji = "📅";
        if (day.dayName == "Понедельник") emoji = "🔴";
        else if (day.dayName == "Вторник") emoji = "🟠";
        else if (day.dayName == "Среда") emoji = "🟡";
        else if (day.dayName == "Четверг") emoji = "🟢";
        else if (day.dayName == "Пятница") emoji = "🔵";
        else if (day.dayName == "Суббота") emoji = "🟣";
        oss << emoji << " *" << day.dayName << "*\n─────────────────────\n";
        if (day.lessons.empty()) {
            oss << "   🎉 Нет занятий\n";
        } else {
            int n = 1;
            for (const auto& l : day.lessons) {
                oss << "   " << n << "⃣ ";
                if (!l.time.empty()) oss << "🕐 " << l.time << "\n";
                oss << "      📖 " << l.subject;
                if (!l.type.empty()) oss << " _(" << l.type << ")_";
                oss << "\n";
                if (!l.room.empty()) oss << "      🏫 " << l.room << "\n";
                oss << "\n";
                ++n;
            }
        }
        oss << "\n";
    }
    oss << "━━━━━━━━━━━━━━━━━━━━━━━━━\n🔄 Данные с mmf.bsu.by\nℹ️ /start — вернуться в меню";
    std::string result = oss.str();
    return fixUtf8(result);
}

std::string ScheduleParser::buildUrl(int course, int group) {
    std::string base = EnvLoader::get("BASE_URL", "https://mmf.bsu.by");
    return base + "/ru/raspisanie-zanyatij/dnevnoe-otdelenie/" +
           std::to_string(course) + "-kurs/" + std::to_string(group) + "-gruppa/";
}

// Кэширование расписания (1 час)
static std::map<std::string, std::pair<std::string, time_t>> scheduleCache;
static const int CACHE_TTL = 3600;

std::string ScheduleParser::getScheduleText(int course, int group) {
    std::string key = std::to_string(course) + "_" + std::to_string(group);
    time_t now = time(nullptr);
    auto it = scheduleCache.find(key);
    if (it != scheduleCache.end() && difftime(now, it->second.second) < CACHE_TTL) {
        return it->second.first;
    }

    std::string url = buildUrl(course, group);
    std::string html = downloadPage(url);
    WeekSchedule schedule;
    if (!html.empty()) {
        schedule = parse(html);
    }
    std::string result = formatSchedule(schedule, course, group);
    scheduleCache[key] = {result, now};
    return result;
}
