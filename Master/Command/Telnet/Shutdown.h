#pragma once

#include "TelnetCommandBase.h"

class Shutdown {
public:
    static const TelnetCommandInfo& GetInfo();
    static void Execute(TelnetClient* pNetClient, std::vector<string>& args);
};
