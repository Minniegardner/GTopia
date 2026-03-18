#include "TCPEventHello.h"
#include "Packet/GamePacket.h"
#include "Network/NetClient.h"
#include "../Context.h"

void TCPEventHello::Execute(NetClient* pClient, VariantVector& data)
{
    VariantVector packet(4);
    packet[0] = TCP_PACKET_AUTH;

    packet[1] = data[1].GetString();
    packet[2] = (uint32)GetContext()->GetID();
    packet[3] = CONFIG_SERVER_RENDERER;

    pClient->Send(packet);
}