#include "bot_handler.h"
#include "schedule_parser.h"
#include <iostream>
#include <thread>
#include <chrono>

BotHandler::BotHandler(const std::string& token) : bot_(token) {
    // ... (оставьте как есть)
}

void BotHandler::sendDepartmentMenu(int64_t chatId) {
    auto keyboard = createInlineKeyboard({
        {{"📚 Очное отделение", "department_1"}},
        {{"🌙 Заочное отделение", "department_2"}}
    });
    bot_.getApi().sendMessage(chatId, "🎓 Выберите отделение:", nullptr, 0, keyboard, "Markdown");
}

void BotHandler::sendCourseMenu(int64_t chatId, int department) {
    std::vector<std::pair<std::string, std::string>> buttons;
    int maxCourse = (department == 1) ? 4 : 5;
    for (int c = 1; c <= maxCourse; ++c) {
        buttons.push_back({std::to_string(c) + " курс", "course_" + std::to_string(c)});
    }
    std::vector<std::vector<std::pair<std::string, std::string>>> rows;
    for (size_t i = 0; i < buttons.size(); i += 2) {
        std::vector<std::pair<std::string, std::string>> row;
        row.push_back(buttons[i]);
        if (i + 1 < buttons.size()) row.push_back(buttons[i+1]);
        rows.push_back(row);
    }
    rows.push_back({{"⬅️ Назад к отделениям", "back_to_departments"}});
    auto keyboard = createInlineKeyboard(rows);
    std::string text = (department == 1) ? "📘 *Очное отделение*" : "📙 *Заочное отделение*";
    text += "\nВыберите курс:";
    bot_.getApi().sendMessage(chatId, text, nullptr, 0, keyboard, "Markdown");
}

void BotHandler::sendExternalLink(int64_t chatId, int course, int department) {
    std::string url = "https://mmf.bsu.by/ru/raspisanie-zanyatij/zaochnoe-otdelenie/" +
                      std::to_string(course) + "-kurs/";
    std::string msg = "📎 Расписание заочного отделения доступно на сайте:\n" + url +
                      "\n\nБот не парсит заочное отделение автоматически. Перейдите по ссылке.";
    bot_.getApi().sendMessage(chatId, msg, nullptr, 0, nullptr, "Markdown");
}

// В handleCallbackQuery используйте правильные названия методов
void BotHandler::handleCallbackQuery(TgBot::CallbackQuery::Ptr query) {
    // ... (ваш код, где вызываются sendDepartmentMenu, sendCourseMenu, sendExternalLink)
    // убедитесь, что передаёте правильное количество аргументов
}

// Остальные методы (sendGroupMenu, sendSchedule, createInlineKeyboard, getUserState, setUserState)
// остаются без изменений.
