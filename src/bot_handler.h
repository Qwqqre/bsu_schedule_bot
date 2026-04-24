#pragma once

#include <tgbot/tgbot.h>
#include <map>
#include <mutex>

struct UserState {
    int department = 0;        // 1 - очное, 2 - заочное
    int selectedCourse = 0;
    int selectedGroup = 0;
};

class BotHandler {
public:
    explicit BotHandler(const std::string& token);
    void run();

private:
    void handleStart(TgBot::Message::Ptr message);
    void handleCallbackQuery(TgBot::CallbackQuery::Ptr query);

    void sendDepartmentMenu(int64_t chatId);
    void sendCourseMenu(int64_t chatId, int department);
    void sendGroupMenu(int64_t chatId, int course);
    void sendSchedule(int64_t chatId, int course, int group);
    void sendExternalLink(int64_t chatId, int course, int department);

    TgBot::InlineKeyboardMarkup::Ptr createInlineKeyboard(
        const std::vector<std::vector<std::pair<std::string, std::string>>>& buttons);

    UserState getUserState(int64_t userId);
    void setUserState(int64_t userId, const UserState& state);

    TgBot::Bot bot_;
    std::map<int64_t, UserState> userStates_;
    std::mutex statesMutex_;
};
