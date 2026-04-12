#pragma once

#include "../Precompiled.h"

enum eConfigServerType
{
    CONFIG_SERVER_MASTER,
    CONFIG_SERVER_GAME,
    CONFIG_SERVER_RENDERER,
};

struct ServerConfigSchema
{
    uint16 id;
    string lanIP;
    string wanIP;
    uint16 tcpPort;
    uint16 udpPort;
    eConfigServerType serverType;
};

struct DatabaseConfigSchema
{
    string host;
    string user;
    string pass;
    string database;
    uint16 port;
};

class GameConfig {
public:
    GameConfig();

public:
    bool LoadConfig(const string& filePath);

    uint16 LoadServersMaster(const string& filePath);
    uint16 LoadServersClient(const string& filePath, uint16 serverID);

private:
    void AddServer(uint16 serverID, const string& lanIP, const string& wanIP, uint16 tcpStart, uint16 udpStart, eConfigServerType serverType);

public:
    std::vector<ServerConfigSchema> servers;
    DatabaseConfigSchema database;

    string cdnServer = "";
    string cdnPath = "";
    string worldSavePath = "";
    string rendererSavePath = "";
    string rendererStaticPath = "";
    uint16 maxLoginsAtOnce = 20;
    uint16 maxAccountsPerIP = 3;
};