#pragma once

#include "Server/ServerBroadwayBase.h"
#include "Event/EventDispatcher.h"
#include "Packet/GamePacket.h"

class MasterBroadway : public ServerBroadwayBase {
public:
    MasterBroadway();
    ~MasterBroadway();

public:
    static MasterBroadway* GetInstance()
    {
        static MasterBroadway instance;
        return &instance;
    }

public:
    void RegisterEvents() override;
    void UpdateTCPLogic(uint64 maxTimeMS) override;

public:
    void SendHelloPacket();
    void SendWorldRenderResult(eTCPPacketType result, uint32 userID, uint32 worldID);

private:
    template<class T>
    void RegisterEvent(eTCPPacketType packet)
    {
        m_events.Register(
            packet,
            Delegate<NetClient*, VariantVector&>::Create<&T::Execute>()
        );
    }

private:
    EventDispatcher<int8, NetClient*, VariantVector&> m_events;
};

MasterBroadway* GetMasterBroadway();