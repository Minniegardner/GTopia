#pragma once

#include "Network/NetClient.h"

class TCPEventHeartBeat {
public:
    static void Execute(NetClient* pClient, VariantVector& data);
};
