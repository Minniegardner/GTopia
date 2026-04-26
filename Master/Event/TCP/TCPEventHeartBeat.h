#pragma once

#include "Network/NetClient.h"

struct TCPHeartBeatEventData
{
    uint32 playerCount = 0;
    uint32 worldCount = 0;

    void FromVariant(VariantVector& varVec);
};

class TCPEventHeartBeat {
public:
    static void Execute(NetClient* pClient, VariantVector& data);
};