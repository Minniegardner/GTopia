#pragma once

#include "Network/NetClient.h"

struct TCPPlayerSessionEventData
{
    int32 netID = -1;
    uint32 userID = 0;
    uint32 token = 0;
    uint32 serverID = 0;

    void FromVariant(VariantVector& varVec);
};

class TCPEventPlayerSession {
public:
    static void Execute(NetClient* pClient, VariantVector& data);
};