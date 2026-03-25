#pragma once

#include "Network/NetClient.h"

class TCPEventWorldSendPlayer {
public:
    static void Execute(NetClient* pClient, VariantVector& data);
};