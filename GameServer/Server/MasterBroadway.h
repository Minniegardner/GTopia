#pragma once

#include "Server/ServerBroadwayBase.h"
#include "Event/EventDispatcher.h"
#include "Packet/GamePacket.h"
#include "Utils/Timer.h"

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
    void SendAuthPacket(const string& authKey);
    void SendCheckSessionPacket(int32 netID, uint32 userID, uint32 token, uint16 serverID);
    void SendRenderWorldRequest(uint32 userID, uint32 worldID);
    void SendWorldInitResult(bool succeed, uint32 worldID);
    void SendPlayerWorldJoin(int32 playerNetID, const string& worldName);
    void SendHeartBeat();
    void SendEndPlayerSession(uint32 userID);
    void SendServerKillPacket();

    uint32 GetGlobalOnlineCount() const { return m_globalOnlineCount; }
    void SetGlobalOnlineCount(uint32 onlineCount)
    {
        m_globalOnlineCount = onlineCount;
        m_lastHearthBeatRecvTime.Reset();
    }

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
    Timer m_lastHearthBeatSentTime;
    Timer m_lastHearthBeatRecvTime;
    uint32 m_globalOnlineCount = 0;
};

MasterBroadway* GetMasterBroadway();