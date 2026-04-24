cmake_minimum_required(VERSION 3.14)
project(bsu_schedule_bot LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(EXISTS /opt/homebrew)
    set(HOMEBREW_PREFIX /opt/homebrew)
else()
    set(HOMEBREW_PREFIX /usr/local)
endif()

set(OPENSSL_ROOT_DIR ${HOMEBREW_PREFIX}/opt/openssl)
find_package(OpenSSL REQUIRED)

set(BOOST_ROOT ${HOMEBREW_PREFIX}/opt/boost)
find_package(Boost REQUIRED)

find_package(CURL REQUIRED)

find_library(TGBOT_LIB TgBot PATHS /usr/local/lib ${HOMEBREW_PREFIX}/lib)
find_path(TGBOT_INCLUDE tgbot/tgbot.h PATHS /usr/local/include ${HOMEBREW_PREFIX}/include)

find_library(GUMBO_LIB gumbo PATHS ${HOMEBREW_PREFIX}/lib /usr/local/lib)
find_path(GUMBO_INCLUDE gumbo.h PATHS ${HOMEBREW_PREFIX}/include /usr/local/include)

add_executable(${PROJECT_NAME}
    src/main.cpp
    src/env_loader.cpp
    src/schedule_parser.cpp
    src/bot_handler.cpp
)

target_include_directories(${PROJECT_NAME} PRIVATE
    ${TGBOT_INCLUDE}
    ${GUMBO_INCLUDE}
    ${Boost_INCLUDE_DIRS}
    ${OPENSSL_INCLUDE_DIR}
    ${CURL_INCLUDE_DIRS}
    src
)

target_link_libraries(${PROJECT_NAME} PRIVATE
    ${TGBOT_LIB}
    ${GUMBO_LIB}
    OpenSSL::SSL
    OpenSSL::Crypto
    ${Boost_LIBRARIES}
    CURL::libcurl
    pthread
)
