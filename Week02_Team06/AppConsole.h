#pragma once

struct AppConsole;

extern AppConsole* appConsole;

void CreateAppConsole();
void DestroyAppConsole();

void DrawAppConsole(const char* title, bool* open);

void AddLogToConsole(const char* fmt, ...);
#define UE_LOG(format, ...) AddLogToConsole(format, ##__VA_ARGS__)