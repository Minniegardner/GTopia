#pragma once

#include "Network/NetClient.h"

class TCPEventRenderWorldRes {
public:
    static void Execute(NetClient* pClient, VariantVector& data);
};