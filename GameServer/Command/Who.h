#pragma once

#include "CommandBase.h"

class Who : public CommandBase<Who> {
public:
    static const CommandInfo& GetInfo();

public:
    static void Execute(GamePlayer* pPlayer, std::vector<string>& args);
};
