#pragma once

#include "Network/NetClient.h"

class TCPEventCrossServerAction {
public:
    static void Execute(NetClient* pClient, VariantVector& data);
};
