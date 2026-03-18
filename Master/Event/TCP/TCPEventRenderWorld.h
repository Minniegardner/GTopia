#pragma once

#include "Network/NetClient.h"

class TCPEventRenderWorld {
public:
    static void Execute(NetClient* pClient, VariantVector& data);
};