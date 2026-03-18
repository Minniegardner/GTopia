#include "MasterBroadway.h"
#include "IO/Log.h"
#include "Packet/NetPacket.h"
#include "Utils/Timer.h"

#include "../Event/TCP/TCPEventHello.h"
#include "../Event/TCP/TCPEventAuth.h"
#include "../Event/TCP/TCPEventPlayerSession.h"
#include "../Event/TCP/TCPEventWorldInit.h"
#include "../Event/TCP/TCPEventRenderWorldRes.h"

MasterBroadway::MasterBroadway()
{
}

MasterBroadway::~MasterBroadway()
{
}

void MasterBroadway::RegisterEvents()
{
    ServerBroadwayBase::RegisterEvents();

    RegisterEvent<TCPEventHello>(TCP_PACKET_HELLO);
    RegisterEvent<TCPEventAuth>(TCP_PACKET_AUTH);
    RegisterEvent<TCPEventPlayerSession>(TCP_PACKET_PLAYER_CHECK_SESSION);
    RegisterEvent<TCPEventWorldInit>(TCP_PACKET_WORLD_INIT);
    RegisterEvent<TCPEventRenderWorldRes>(TCP_PACKET_RENDER_WORLD_RES);
}

void MasterBroadway::UpdateTCPLogic(uint64 maxTimeMS)
{
    uint64 startTime = Time::GetSystemTime();
    TCPPacketEvent event;

    uint32 processed = 0;

    while(m_packetQueue.try_dequeue(event)) {
        if(!event.pClient) {
            continue;
        }

        int8 type = event.data[0].GetINT();
        m_events.Dispatch(type, event.pClient, event.data);

        processed++;
        if(Time::GetSystemTime() - startTime >= maxTimeMS) {
            break;
        }
    }

    if(processed > 0) {
        LOGGER_LOG_DEBUG("Processed %d TCP packets maxMS %d, took %d MS", processed, maxTimeMS, Time::GetSystemTime() - startTime);
    }
}

void MasterBroadway::SendHelloPacket()
{
    if(!m_connected || !m_pNetClient) {
        return;
    }

    VariantVector data(1);
    data[0] = TCP_PACKET_HELLO;

    m_pNetClient->Send(data);
}

void MasterBroadway::SendCheckSessionPacket(int32 netID, uint32 userID, uint32 token, uint16 serverID)
{
    if(!m_connected || !m_pNetClient) {
        return;
    }

    VariantVector data(5);
    data[0] = TCP_PACKET_PLAYER_CHECK_SESSION;
    data[1] = netID;
    data[2] = userID;
    data[3] = token;
    data[4] = serverID;

    m_pNetClient->Send(data);
}

void MasterBroadway::SendRenderWorldRequest(uint32 worldID, uint32 userID)
{
    if(!m_connected || !m_pNetClient) {
        return;
    }

    VariantVector data(3);
    data[0] = TCP_PACKET_RENDER_WORLD;
    data[1] = worldID;
    data[2] = userID;

    m_pNetClient->Send(data);
}

MasterBroadway* GetMasterBroadway() { return MasterBroadway::GetInstance(); }