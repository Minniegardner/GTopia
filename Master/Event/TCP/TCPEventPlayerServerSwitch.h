#pragma once

#include "Network/NetClient.h"

struct TCPPlayerServerSwitchEventData
{
    uint32 userID = 0;
    uint32 targetServerID = 0;

    void FromVariant(VariantVector& varVec);
};

class TCPEventPlayerServerSwitch {
public:
    static void Execute(NetClient* pClient, VariantVector& data);
};