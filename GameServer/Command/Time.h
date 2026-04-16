#pragma once

#include "CommandBase.h"

class CmdTime : public CommandBase<CmdTime> {
public:
    static const CommandInfo& GetInfo();

public:
    static void Execute(GamePlayer* pPlayer, std::vector<string>& args);
};
