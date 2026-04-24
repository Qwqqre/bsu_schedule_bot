#include <iostream>
#include <csignal>
#include "env_loader.h"
#include "bot_handler.h"

volatile sig_atomic_t running = 1;
void signalHandler(int) { running = 0; }

int main() {
    signal(SIGINT, signalHandler);
    signal(SIGTERM, signalHandler);

    if (!EnvLoader::load(".env") && !EnvLoader::load("../.env")) {
        std::cerr << "Ошибка: не найден .env файл" << std::endl;
        return 1;
    }
    std::string token = EnvLoader::get("BOT_TOKEN");
    if (token.empty()) {
        std::cerr << "BOT_TOKEN не задан" << std::endl;
        return 1;
    }

    try {
        BotHandler bot(token);
        bot.run();
    } catch (std::exception& e) {
        std::cerr << "Исключение: " << e.what() << std::endl;
        return 1;
    }
    return 0;
}
