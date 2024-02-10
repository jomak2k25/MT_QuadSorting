#pragma once
#include <iostream>
#include <sstream>
#include <fstream>
#include <string>
#include "assert.h"

#define DBG_LOG(file,...) Log(file,__VA_ARGS__)

#define DBG_LOG_WARNING(file,...) LogWarning(file,__VA_ARGS__)

#define DBG_LOG_ERROR(file,...) LogError(file,__VA_ARGS__)

#define DBG_LOG_ASSERT(condition,file,...) LogAssert(condition,file,__VA_ARGS__)

static const std::string LogFileName = "Log.txt";
static bool LogStarted = false;
static std::ofstream LogFile;

// Terminator
void LogRecursive(const char* file, std::ostringstream& msg)
{
    // Print to console
    std::cout << file << ':' << msg.str() << std::endl;
    
    // Open Log File to write out the log
    LogFile.open(LogFileName, std::ios::out | std::ios::ate);
    if (LogFile.is_open())
    {
        // Clear the file if this is the first log
        if (!LogStarted)
        {
            LogFile.clear();
            LogStarted = true;
        }
        LogFile << msg.str();
        LogFile << "\n";
        LogFile.close();
    }
    else
    {
        std::cout << "FAILED TO OPEN LOG FILE \n";
    }
};

// "Recursive" variadic function
template<typename T, typename... Args>
void LogRecursive(const char* file, std::ostringstream& msg,
    T value, const Args&... args)
{
    msg << value;
    LogRecursive(file, msg, args...);
};

template<typename... Args>
void Log(const char* file, const Args&... args)
{
    std::ostringstream msg;
    msg << "Log: ";
    LogRecursive(file, msg, args...);
};

template<typename... Args>
void LogWarning(const char* file, const Args&... args)
{
    std::ostringstream msg;
    msg << "WARNING: ";
    LogRecursive(file, msg, args...);
};

template<typename... Args>
void LogError(const char* file, const Args&... args)
{
    std::ostringstream msg;
    msg << "ERROR: ";
    LogRecursive(file, msg, args...);
};

template<typename... Args>
void LogAssert(bool condition, const char* file, const Args&... args)
{
    if (condition) { return; };
    std::ostringstream msg;
    msg << "ASSERT: ";
    LogRecursive(file, msg, args...);
    assert(false);
};