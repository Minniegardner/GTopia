#pragma once

#include "CommandBase.h"

class News : public CommandBase<News> {
public:
    static const CommandInfo& GetInfo();

public:
    static void Execute(GamePlayer* pPlayer, std::vector<string>& args);
};
