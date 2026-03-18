#pragma once

#include "Precompiled.h"
#include "Server/ServerBroadwayBase.h"
#include "Packet/NetPacket.h"

struct NetClientInfo
{
    uint64 lastHeartbeatTime = 0;
    string authKey = "";
    bool authed = false;
};

struct ServerInfo
{
    uint16 serverID = 0;
    uint32 playerCount = 0;
    uint32 worldCount = 0;
    int32 socketConnID = -1;
    string wanIP;
    uint16 port;
};

struct RendererInfo
{
    uint16 serverID = 0;
    int32 socketConnID = -1;
    string wanIP;
    uint16 port;
};

class ServerManager : public ServerBroadwayBase {
public:
    ServerManager();
    ~ServerManager();

public:
    static ServerManager* GetInstance()
    {
        static ServerManager instance;
        return &instance;
    }

public:
    void OnClientConnect(NetClient* pClient) override;
    void OnClientDisconnect(NetClient* pClient) override;
    void UpdateTCPLogic(uint64 maxTimeMS) override;
    void Kill() override;
    void RegisterEvents() override;

public:
    void AddServer(uint16 serverID, NetClient* pClient, int8 serverType);
    void RemoveServer(uint16 serverID);
    ServerInfo* GetBestServer();
    RendererInfo* GetBestRenderer();

    ServerInfo* GetServerByID(uint16 serverID);
    RendererInfo* GetRendererByID(uint rendererID);
    bool SendPacketServerRaw(uint16 serverID, VariantVector& data);
    bool SendPacketRendererRaw(uint16 rendererID, VariantVector& data);

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
    std::unordered_map<uint16, ServerInfo*> m_servers;
    std::vector<RendererInfo*> m_renderers;
    EventDispatcher<int8, NetClient*, VariantVector&> m_events;
};

ServerManager* GetServerManager();