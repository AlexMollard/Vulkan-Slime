//
// Created by alexm on 23/01/2022.
//

#include "Log.h"
#include "date.h"
#include <iostream>
#include <string>
#include <filesystem>

std::shared_ptr<spdlog::logger> Log::mCoreLogger;
std::shared_ptr<spdlog::logger> Log::mClientLogger;
std::shared_ptr<spdlog::logger> Log::mFileLogger;
std::shared_ptr<spdlog::logger> Log::mFunctionLogger;

std::string get_dmy_txt_string()
{
    auto tp = std::chrono::system_clock::now(); // tp is a C::system_clock::time_point
    {
        // Need to reach into namespace date for this streaming operator
        using namespace date;
        std::cout << tp << '\n';
    }
    auto dp = date::floor<date::days>(tp);
    auto dmy = date::year_month_day{dp};

    std::stringstream ss;
    ss << dmy.day() << '-' << dmy.month() << '-' << dmy.year() << ".log";
    return ss.str();
}

void Log::init() {
    spdlog::set_pattern("[%T] %n: %v%$");

    mCoreLogger = spdlog::stdout_color_mt("VULAN_SLIME");
    mCoreLogger->set_level(spdlog::level::trace);

    mClientLogger = spdlog::stdout_color_mt("APP");
    mClientLogger->set_level(spdlog::level::trace);

    std::string currentFileDir = std::filesystem::current_path().string();
    std::string logFileDir = std::string();
    logFileDir.append(currentFileDir).append(R"(\..\logs\)").append(get_dmy_txt_string());

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
