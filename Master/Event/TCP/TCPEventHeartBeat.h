#pragma once

#include "Network/NetClient.h"

class TCPEventHearthBeat {
public:
    static void Execute(NetClient* pClient, VariantVector& data);
};