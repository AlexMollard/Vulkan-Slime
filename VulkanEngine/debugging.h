#pragma once

#include <stdexcept>
#include <iostream>

class vulkanCreateError : public std::runtime_error {
public:
    explicit vulkanCreateError(const std::string &msg = "") : runtime_error(msg) {}
};

class vulkanError : public std::runtime_error {
public:
    explicit vulkanError(const std::string &msg = "") : runtime_error(msg) {}
};

class cppError : public std::runtime_error {
public:
    explicit cppError(const std::string &msg = "") : runtime_error(msg) {}
};

struct logger {
    logger() = delete;

    static void log(const std::string &msg) { std::cout << msg << '\n'; };
};