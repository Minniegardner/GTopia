#pragma once

#include "Network/NetClient.h"

struct TCPPlayerSessionEventData
{
    int32 playerNetID = -1;

    void FromVariant(const VariantVector& varVec);
};

class TCPEventPlayerSession {
public:
    static void Execute(NetClient* pClient, VariantVector& data);
};