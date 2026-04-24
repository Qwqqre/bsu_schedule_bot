#include "bot_handler.h"
#include "schedule_parser.h"
#include <iostream>
#include <thread>
#include <chrono>
#include <future>
#include <cctype>
#include <tgbot/types/GenericReply.h>
#include <tgbot/types/InlineKeyboardMarkup.h>

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

static std::string cleanUtf8String(const std::string& s) {
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

// Безопасная отправка сообщения с автоматической очисткой UTF-8
static void safeSendMessage(TgBot::Bot& bot, int64_t chatId, const std::string& text,
                            TgBot::GenericReply::Ptr replyMarkup = nullptr,
                            const std::string& parseMode = "") {
    if (text.empty()) return;
    try {
        bot.getApi().sendMessage(chatId, text, nullptr, 0, replyMarkup, parseMode);
    } catch (const TgBot::TgException& e) {
        std::string err = e.what();
        if (err.find("strings must be encoded in UTF-8") != std::string::npos) {
            // Очищаем строку и повторяем попытку
            std::string cleaned = cleanUtf8String(text);
            if (!cleaned.empty()) {
                try {
                    bot.getApi().sendMessage(chatId, cleaned, nullptr, 0, replyMarkup, parseMode);
                } catch (const std::exception& e2) {
                    std::cerr << "[safeSendMessage] Ошибка после очистки: " << e2.what() << std::endl;
                }
            }
        } else {
            std::cerr << "[safeSendMessage] Ошибка: " << err << std::endl;
        }
    } catch (const std::exception& e) {
        std::cerr << "[safeSendMessage] Исключение: " << e.what() << std::endl;
    }
}
// ========================================================================

BotHandler::BotHandler(const std::string& token) : bot_(token) {
    std::cout << "[Bot] Инициализация..." << std::endl;

    bot_.getEvents().onCommand("start", [this](TgBot::Message::Ptr msg) {
        try {
            handleStart(msg);
        } catch (const std::exception& e) {
            std::cerr << "[start] Ошибка: " << e.what() << std::endl;
        }
    });

    bot_.getEvents().onCommand("help", [this](TgBot::Message::Ptr msg) {
        try {
            std::string help =
                "ℹ️ *Помощь*\n\n"
                "Бот показывает расписание мехмата БГУ.\n"
                "Используйте /start для выбора отделения, курса и группы.\n"
                "Данные с сайта mmf.bsu.by";
            safeSendMessage(bot_, msg->chat->id, help, nullptr, "Markdown");
        } catch (const std::exception& e) {
            std::cerr << "[help] Ошибка: " << e.what() << std::endl;
        }
    });

    bot_.getEvents().onCallbackQuery([this](TgBot::CallbackQuery::Ptr query) {
        try {
            handleCallbackQuery(query);
        } catch (const std::exception& e) {
            std::cerr << "[callback] Ошибка: " << e.what() << std::endl;
        }
    });

    bot_.getEvents().onAnyMessage([this](TgBot::Message::Ptr msg) {
        try {
            if (msg->text.empty() || msg->text[0] == '/') return;
            safeSendMessage(bot_, msg->chat->id,
                "👋 Используйте /start для выбора отделения и группы.");
        } catch (const std::exception& e) {
            std::cerr << "[anyMessage] Ошибка: " << e.what() << std::endl;
        }
    });
}

void BotHandler::run() {
    std::cout << "[Bot] Запущен, ожидание сообщений..." << std::endl;
    TgBot::TgLongPoll longPoll(bot_);
    while (true) {
        try {
            longPoll.start();
        } catch (TgBot::TgException& e) {
            std::cerr << "[Bot] Ошибка longPoll: " << e.what() << std::endl;
            std::this_thread::sleep_for(std::chrono::seconds(5));
        } catch (const std::exception& e) {
            std::cerr << "[Bot] Неизвестная ошибка: " << e.what() << std::endl;
            std::this_thread::sleep_for(std::chrono::seconds(5));
        }
    }
}

void BotHandler::handleStart(TgBot::Message::Ptr message) {
    int64_t chatId = message->chat->id;
    int64_t userId = message->from->id;
    setUserState(userId, UserState());
    sendDepartmentMenu(chatId);
}

void BotHandler::handleCallbackQuery(TgBot::CallbackQuery::Ptr query) {
    // Безопасный answerCallbackQuery
    try {
        bot_.getApi().answerCallbackQuery(query->id);
    } catch (const TgBot::TgException& e) {
        std::string err = e.what();
        if (err.find("query is too old") == std::string::npos &&
            err.find("query ID is invalid") == std::string::npos) {
            std::cerr << "[Callback] answerCallbackQuery: " << err << std::endl;
        }
        // Если не смогли ответить – выходим
        return;
    }

    int64_t chatId = query->message->chat->id;
    int64_t userId = query->from->id;
    std::string data = query->data;

    if (data == "department_1") {
        UserState state = getUserState(userId);
        state.department = 1;
        setUserState(userId, state);
        sendCourseMenu(chatId, 1);
    }
    else if (data == "department_2") {
        UserState state = getUserState(userId);
        state.department = 2;
        setUserState(userId, state);
        sendCourseMenu(chatId, 2);
    }
    else if (data.find("course_") == 0) {
        int course = std::stoi(data.substr(7));
        UserState state = getUserState(userId);
        state.selectedCourse = course;
        setUserState(userId, state);
        if (state.department == 1) {
            sendGroupMenu(chatId, course);
        } else {
            sendExternalLink(chatId, course, 2);
        }
    }
    else if (data.find("group_") == 0) {
        int group = std::stoi(data.substr(6));
        UserState state = getUserState(userId);
        state.selectedGroup = group;
        setUserState(userId, state);

        // Асинхронная отправка расписания
        std::thread([this, chatId, course = state.selectedCourse, group]() {
            try {
                // Сообщение "Загружаю..."
                auto loadingMsg = bot_.getApi().sendMessage(chatId,
                    "⏳ Загружаю расписание для " + std::to_string(course) + " курса, " + std::to_string(group) + " группы...");
                std::string scheduleText = ScheduleParser::getScheduleText(course, group);
                bot_.getApi().deleteMessage(chatId, loadingMsg->messageId);

                auto keyboard = createInlineKeyboard({
                    {{"🔄 Обновить", "refresh"}, {"⬅️ Другая группа", "back_to_groups"}},
                    {{"🏠 В начало", "back_to_departments"}}
                });

                if (scheduleText.size() > 4096) {
                    for (size_t i = 0; i < scheduleText.size(); i += 4096) {
                        safeSendMessage(bot_, chatId, scheduleText.substr(i, 4096));
                    }
                    safeSendMessage(bot_, chatId, "Продолжение следует...", keyboard);
                } else {
                    safeSendMessage(bot_, chatId, scheduleText, keyboard, "Markdown");
                }
            } catch (const std::exception& e) {
                std::cerr << "[async schedule] Ошибка: " << e.what() << std::endl;
            }
        }).detach();
    }
    else if (data == "back_to_departments") {
        setUserState(userId, UserState());
        sendDepartmentMenu(chatId);
    }
    else if (data == "back_to_courses") {
        UserState state = getUserState(userId);
        if (state.department != 0) {
            sendCourseMenu(chatId, state.department);
        } else {
            sendDepartmentMenu(chatId);
        }
    }
    else if (data == "back_to_groups") {
        UserState state = getUserState(userId);
        sendGroupMenu(chatId, state.selectedCourse);
    }
    else if (data == "refresh") {
        UserState state = getUserState(userId);
        if (state.selectedCourse > 0 && state.selectedGroup > 0) {
            std::thread([this, chatId, course = state.selectedCourse, group = state.selectedGroup]() {
                try {
                    auto loadingMsg = bot_.getApi().sendMessage(chatId, "⏳ Обновляю расписание...");
                    std::string scheduleText = ScheduleParser::getScheduleText(course, group);
                    bot_.getApi().deleteMessage(chatId, loadingMsg->messageId);
                    auto keyboard = createInlineKeyboard({
                        {{"🔄 Обновить", "refresh"}, {"⬅️ Другая группа", "back_to_groups"}},
                        {{"🏠 В начало", "back_to_departments"}}
                    });
                    safeSendMessage(bot_, chatId, scheduleText, keyboard, "Markdown");
                } catch (const std::exception& e) {
                    std::cerr << "[refresh] Ошибка: " << e.what() << std::endl;
                }
            }).detach();
        } else {
            sendCourseMenu(chatId, state.department);
        }
    }
}

void BotHandler::sendDepartmentMenu(int64_t chatId) {
    auto keyboard = createInlineKeyboard({
        {{"📚 Очное отделение", "department_1"}},
        {{"🌙 Заочное отделение", "department_2"}}
    });
    safeSendMessage(bot_, chatId, "🎓 Выберите отделение:", keyboard, "Markdown");
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
    safeSendMessage(bot_, chatId, text, keyboard, "Markdown");
}

void BotHandler::sendGroupMenu(int64_t chatId, int course) {
    int groupsCount = 0;
    if (course == 1) groupsCount = 12;
    else if (course == 2) groupsCount = 11;
    else if (course == 3) groupsCount = 10;
    else if (course == 4) groupsCount = 9;
    else groupsCount = 0;

    if (groupsCount == 0) {
        safeSendMessage(bot_, chatId, "❌ Для этого курса нет данных о группах.");
        return;
    }

    std::vector<std::vector<std::pair<std::string, std::string>>> rows;
    std::vector<std::pair<std::string, std::string>> row;
    for (int g = 1; g <= groupsCount; ++g) {
        row.push_back({std::to_string(g) + " гр.", "group_" + std::to_string(g)});
        if (row.size() == 3 || g == groupsCount) {
            rows.push_back(row);
            row.clear();
        }
    }
    rows.push_back({{"⬅️ Назад к курсам", "back_to_courses"}});
    auto keyboard = createInlineKeyboard(rows);
    safeSendMessage(bot_, chatId, "📋 *" + std::to_string(course) + " курс*\nВыберите группу:",
                    keyboard, "Markdown");
}

void BotHandler::sendSchedule(int64_t chatId, int course, int group) {
    // Этот метод не используется (заменён асинхронным в handleCallbackQuery)
    auto keyboard = createInlineKeyboard({
        {{"🔄 Обновить", "refresh"}, {"⬅️ Другая группа", "back_to_groups"}},
        {{"🏠 В начало", "back_to_departments"}}
    });
    std::string scheduleText = ScheduleParser::getScheduleText(course, group);
    safeSendMessage(bot_, chatId, scheduleText, keyboard, "Markdown");
}

void BotHandler::sendExternalLink(int64_t chatId, int course, int department) {
    std::string url = "https://mmf.bsu.by/ru/raspisanie-zanyatij/zaochnoe-otdelenie/" +
                      std::to_string(course) + "-kurs/";
    std::string msg = "📎 Расписание заочного отделения доступно на сайте:\n" + url +
                      "\n\nБот не парсит заочное отделение автоматически. Перейдите по ссылке.";
    safeSendMessage(bot_, chatId, msg, nullptr, "Markdown");
}

TgBot::InlineKeyboardMarkup::Ptr BotHandler::createInlineKeyboard(
    const std::vector<std::vector<std::pair<std::string, std::string>>>& buttons)
{
    auto keyboard = std::make_shared<TgBot::InlineKeyboardMarkup>();
    for (const auto& row : buttons) {
        std::vector<TgBot::InlineKeyboardButton::Ptr> kbRow;
        for (const auto& btn : row) {
            auto b = std::make_shared<TgBot::InlineKeyboardButton>();
            b->text = btn.first;
            b->callbackData = btn.second;
            kbRow.push_back(b);
        }
        keyboard->inlineKeyboard.push_back(kbRow);
    }
    return keyboard;
}

UserState BotHandler::getUserState(int64_t userId) {
    std::lock_guard<std::mutex> lock(statesMutex_);
    auto it = userStates_.find(userId);
    if (it != userStates_.end()) return it->second;
    return UserState();
}

void BotHandler::setUserState(int64_t userId, const UserState& state) {
    std::lock_guard<std::mutex> lock(statesMutex_);
    userStates_[userId] = state;
}
