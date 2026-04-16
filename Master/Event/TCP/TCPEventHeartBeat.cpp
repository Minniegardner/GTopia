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

	VariantVector response(29);
	response[0] = TCP_PACKET_HEARTBEAT;
	response[1] = GetServerManager()->GetTotalOnlineCount();
	response[2] = GetServerManager()->GetDailyEpochDay();
	response[3] = GetServerManager()->GetDailyEventType();
	response[4] = GetServerManager()->GetDailyEventSeed();

	const TCPDailyQuestData& dailyQuest = GetServerManager()->GetDailyQuestData();
	response[5] = dailyQuest.questItemOneID;
	response[6] = dailyQuest.questItemTwoID;
	response[7] = dailyQuest.questItemOneAmount;
	response[8] = dailyQuest.questItemTwoAmount;
	response[9] = dailyQuest.rewardOneID;
	response[10] = dailyQuest.rewardOneAmount;
	response[11] = dailyQuest.rewardTwoID;
	response[12] = dailyQuest.rewardTwoAmount;

	const TCPWeeklyEventsData& weekly = GetServerManager()->GetWeeklyEventsData();
	response[13] = weekly.roleQuestFarmerDay;
	response[14] = weekly.roleQuestBuilderDay;
	response[15] = weekly.roleQuestSurgeonDay;
	response[16] = weekly.roleQuestFishingDay;
	response[17] = weekly.roleQuestChefDay;
	response[18] = weekly.roleQuestCaptainDay;

	const TCPMonthlyEventsData& monthly = GetServerManager()->GetMonthlyEventsData();
	response[19] = monthly.lockeDayOne;
	response[20] = monthly.lockeDayTwo;
	response[21] = monthly.carnivalDayOne;
	response[22] = monthly.carnivalDayTwo;
	response[23] = monthly.surgeryDay;
	response[24] = monthly.allHowlsEveDay;
	response[25] = monthly.geigerDay;
	response[26] = monthly.ghostDay;
	response[27] = monthly.mutantKitchenDay;
	response[28] = monthly.voucherDayz;

	pClient->Send(response);
}