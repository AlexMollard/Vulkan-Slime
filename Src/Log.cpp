//
// Created by alexm on 23/01/2022.
//

#include "Log.h"
#include <iostream>
#include <chrono>
#include <ctime>
#include <string>
#include <filesystem>

std::shared_ptr<spdlog::logger> Log::mCoreLogger;
std::shared_ptr<spdlog::logger> Log::mClientLogger;
std::shared_ptr<spdlog::logger> Log::mFileLogger;
std::shared_ptr<spdlog::logger> Log::mFunctionLogger;

std::string getDateStr(){
    std::time_t now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());

    std::string s(30, '\0');
    std::strftime(&s[0], s.size(), "%Y-%m-%d", std::localtime(&now));
    return s;
}

void Log::init() {
    spdlog::set_pattern("[%T] %n: %v%$");

    mCoreLogger = spdlog::stdout_color_mt("VULAN_SLIME");
    mCoreLogger->set_level(spdlog::level::trace);

    mClientLogger = spdlog::stdout_color_mt("APP");
    mClientLogger->set_level(spdlog::level::trace);

    std::string currentFileDir = std::filesystem::current_path().string();
    std::string logFileDir = std::string();
    logFileDir.append(currentFileDir).append(R"(\..\logs\)").append(getDateStr()).append(std::string(".log"));

    mFileLogger = spdlog::basic_logger_mt("BUILD_LOG", logFileDir);
    spdlog::set_default_logger(mFileLogger);

    mFunctionLogger = spdlog::basic_logger_mt("FUNCTION_CALL", logFileDir);

    get_file_logger()->info("--------------------");
#ifdef DEBUG
    get_file_logger()->info("DEBUG_BUILD_STARTED");
#else
    get_file_logger()->info("RELEASE_BUILD_STARTED");
#endif
    get_file_logger()->info("--------------------");
}

void Log::trace(const std::string &message) {
    get_core_logger()->trace(message);

    get_file_logger()->trace(message);
    get_file_logger()->flush();
}

void Log::info(const std::string &message) {
    get_core_logger()->info(message);

    get_file_logger()->info(message);
    get_file_logger()->flush();
}

void Log::warn(const std::string &message) {
    get_core_logger()->warn(message);

    get_file_logger()->warn(message);
    get_file_logger()->flush();
}

void Log::error(const std::string &message) {
    get_core_logger()->error(message);

    get_file_logger()->error(message);
    get_file_logger()->flush();
}

void Log::func(const std::string &message) {
    get_function_logger()->info(message);
    get_function_logger()->flush();
}
