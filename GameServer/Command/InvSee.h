#pragma once

#include "CommandBase.h"

class InvSee : public CommandBase<InvSee> {
public:
    static const CommandInfo& GetInfo();

public:
    static void Execute(GamePlayer* pPlayer, std::vector<string>& args);
};
