#pragma once
#include <map>
class ExitHandler
{
public:
    void exit() { exitHandler(0); }
    static void exitHandler(int) { s_shouldExit = true; }
    bool shouldExit() const { return s_shouldExit; }

private:
    static bool s_shouldExit;
};
bool ExitHandler::s_shouldExit = false;
