#include "MasterBroadway.h"
#include "IO/Log.h"
#include "Utils/Timer.h"

#include "Event/TCPEventHello.h"
#include "Event/TCPEventAuth.h"
#include "Event/TCPEventRenderWorld.h"

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
    RegisterEvent<TCPEventRenderWorld>(TCP_PACKET_RENDER_WORLD);
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

/**
 * set flag to world if player trying to change world name with address
 * and block it
 */
void MasterBroadway::SendWorldRenderResult(eTCPPacketType result, uint32 userID, uint32 worldID)
{
    VariantVector data(4);
    data[0] = TCP_PACKET_RENDER_WORLD_RES;
    data[1] = result;
    data[2] = userID;
    data[3] = worldID;

    m_pNetClient->Send(data);
}

MasterBroadway* GetMasterBroadway() { return MasterBroadway::GetInstance(); }