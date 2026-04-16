#pragma once

#include "CommandBase.h"

class Reply : public CommandBase<Reply> {
public:
    static const CommandInfo& GetInfo();

public:
    static void Execute(GamePlayer* pPlayer, std::vector<string>& args);
};
