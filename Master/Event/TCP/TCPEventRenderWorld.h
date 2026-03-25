#pragma once

#include "Network/NetClient.h"

struct TCPEventRenderWorldData
{
    uint32 userID = 0;
    uint32 worldID = 0;
    int32 result = 0;

    void FromVariant(VariantVector& varVec, bool forResult);
};

class TCPEventRenderWorld {
public:
    static void Execute(NetClient* pClient, VariantVector& data);
};