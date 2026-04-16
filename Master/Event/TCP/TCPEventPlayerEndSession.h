#pragma once

#include "Network/NetClient.h"

struct TCPPlayerEndSessionEventData
{
    uint32 userID = 0;

    void FromVariant(VariantVector& varVec);
};

class TCPEventPlayerEndSession {
public:
    static void Execute(NetClient* pClient, VariantVector& data);
};