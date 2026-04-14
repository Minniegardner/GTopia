#pragma once

#include "Packet/PacketUtils.h"
#include "../../../Player/GamePlayer.h"

class Buy {
public:
    static void Execute(GamePlayer* pPlayer, ParsedTextPacket<8>& packet);
    static void SendHome(GamePlayer* pPlayer);
    static void SendTab(GamePlayer* pPlayer, uint8 tab, const string& selection);

private:
    static void HandlePurchase(GamePlayer* pPlayer, const string& button);
};
