#pragma once

#include "../Precompiled.h"
#include "../Packet/PacketUtils.h"

class Player;

struct PlayerLoginDetail
{
    string requestedName = "";
    string tankIDName = "";
    string tankIDPass = "";

    float gameVersion;
    uint8 platformType;
    string country = "00";
    uint32 protocol = 0;
    
    string meta;
    int32 fhash;
    int32 hash;
    string mac;
    string rid;

    string wk;
    int32 zf = -1;

    uint32 token = 0;
    uint32 user = 0;

    bool Serialize(ParsedTextPacket<25>& packet, Player* pPlayer, bool asGameServer);
};
