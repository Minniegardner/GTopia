#pragma once

#include "Network/NetClient.h"

struct TCPKillServerEventData
{
    uint32 serverID = 0;

    void FromVariant(VariantVector& varVec);
};

class TCPEventKillServer {
public:
    static void Execute(NetClient* pClient, VariantVector& data);
};