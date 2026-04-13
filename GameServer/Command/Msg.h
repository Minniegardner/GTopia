#pragma once

#include "CommandBase.h"

class Msg : public CommandBase<Msg> {
public:
    static const CommandInfo& GetInfo();

public:
    static void Execute(GamePlayer* pPlayer, std::vector<string>& args);
};
