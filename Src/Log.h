//
// Created by alexm on 23/01/2022.
//

#pragma once

#include <memory.h>
#include "spdlog/spdlog.h"
#include "spdlog/logger.h"
#include "spdlog/sinks/stdout_color_sinks.h"
#include "spdlog/sinks/basic_file_sink.h"

class Log {
public:
    static void init();

    static void trace(const std::string &message);

    static void info(const std::string &message);

    static void warn(const std::string &message);

    static void error(const std::string &message);

    static void func(const std::string &message);

private:
    inline static std::shared_ptr<spdlog::logger> &get_core_logger() { return mCoreLogger; }

    inline static std::shared_ptr<spdlog::logger> &get_client_logger() { return mClientLogger; }

    inline static std::shared_ptr<spdlog::logger> &get_file_logger() { return mFileLogger; }

    inline static std::shared_ptr<spdlog::logger> &get_function_logger() { return mFunctionLogger; }

    static std::shared_ptr<spdlog::logger> mCoreLogger;
    static std::shared_ptr<spdlog::logger> mClientLogger;
    static std::shared_ptr<spdlog::logger> mFileLogger;
    static std::shared_ptr<spdlog::logger> mFunctionLogger;
};
