#pragma once

#include "CommandBase.h"

class PInfo : public CommandBase<PInfo> {
public:
    static const CommandInfo& GetInfo();

public:
    static void Execute(GamePlayer* pPlayer, std::vector<string>& args);
};
