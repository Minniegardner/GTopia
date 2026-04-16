#pragma once

#include "../../Server/MasterBroadway.h"

struct TCPHelloEventData
{
    string authKey;
    
    void FromVariant(const VariantVector& varVec);
};

class TCPEventHello {
public:
    static void Execute(NetClient* pClient, VariantVector& data);
};