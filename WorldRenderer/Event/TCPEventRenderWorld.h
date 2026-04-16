#pragma once

#include "../MasterBroadway.h"

struct TCPRenderWorldEventData
{
    int32 subType = TCP_RENDER_RESULT;
    uint32 playerUserID = 0;
    uint32 worldID = 0;

    void FromVariant(VariantVector& varVec);
};

class TCPEventRenderWorld {
public:
    static void Execute(NetClient* pClient, VariantVector& data);
};