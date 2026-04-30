#pragma once

#include "Network/NetClient.h"

class TCPEventKillServer {
public:
    static void Execute(NetClient* pClient, VariantVector& data);
};