#pragma once

#include "CommandBase.h"

class ServerCmd : public CommandBase<ServerCmd> {
public:
    static const CommandInfo& GetInfo();
    static void Execute(GamePlayer* pPlayer, std::vector<string>& args);
};