#pragma once

#include "TelnetCommandBase.h"

class Players {
public:
    static const TelnetCommandInfo& GetInfo();
    static void Execute(TelnetClient* pNetClient, std::vector<string>& args);
};
