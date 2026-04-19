#pragma once

#include "CommandBase.h"

class AddTitle : public CommandBase<AddTitle> {
public:
    static const CommandInfo& GetInfo();
    static void Execute(GamePlayer* pPlayer, std::vector<string>& args);
};