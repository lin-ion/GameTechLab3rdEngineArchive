#pragma once

struct AppConsole;

AppConsole* CreateAppConsole();
void DestoryAppConsole(AppConsole* console);

#define UE_LOG(format, ...) if(appCocnsole) {appCocnsole->AddLog(format,##__VA_ARGS__);}