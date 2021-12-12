#pragma once
#include <stdexcept>
#include <iostream>

class VulkanCreateError : public std::runtime_error
{
public:
	explicit VulkanCreateError(const std::string& msg = "") : runtime_error(msg) {}
};

class VulkanError : public std::runtime_error
{
public:
	explicit VulkanError(const std::string& msg = "") : runtime_error(msg) {}
};

class CppError : public std::runtime_error
{
public:
	explicit CppError(const std::string& msg = "") : runtime_error(msg) {}
};

struct Logger
{
	Logger() = delete;
	static void Log(const std::string& msg) { std::cout << msg << '\n'; };
};