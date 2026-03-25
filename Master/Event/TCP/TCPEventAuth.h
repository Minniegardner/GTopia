#pragma once

#include "Network/NetClient.h"

struct TCPAuthEventData
{
    string authKey;
    uint32 serverID = 0;
    int32 serverType = 0;
    
    void FromVariant(const VariantVector& varVec);
};

class TCPEventAuth {
public:
    static void Execute(NetClient* pClient, VariantVector& data);
};