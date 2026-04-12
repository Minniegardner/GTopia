#include "TCPEventHeartBeat.h"
#include "../../Server/ServerManager.h"

struct TCPHeartBeatEventData
{
	uint16 serverID = 0;
	uint32 playerCount = 0;
	uint32 worldCount = 0;

	void FromVariant(VariantVector& varVec)
	{
		if(varVec.size() < 4) {
			return;
		}

		serverID = varVec[1].GetUINT();
		playerCount = varVec[2].GetUINT();
		worldCount = varVec[3].GetUINT();
	}
};

void TCPEventHeartBeat::Execute(NetClient* pClient, VariantVector& data)
{
	if(!pClient) {
		return;
	}

	TCPHeartBeatEventData eventData;
	eventData.FromVariant(data);

	ServerInfo* pServer = GetServerManager()->GetServerByID(eventData.serverID);
	if(!pServer) {
		return;
	}

	pServer->playerCount = eventData.playerCount;
	pServer->worldCount = eventData.worldCount;

	NetServerInfo* pClientInfo = (NetServerInfo*)pClient->data;
	if(pClientInfo) {
		pClientInfo->lastHeartbeatTime.Reset();
	}

	VariantVector response(2);
	response[0] = TCP_PACKET_HEARTBEAT;
	response[1] = GetServerManager()->GetTotalOnlineCount();

	pClient->Send(response);
}