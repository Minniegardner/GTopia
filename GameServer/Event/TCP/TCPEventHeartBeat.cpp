#include "TCPEventHeartBeat.h"
#include "Server/MasterBroadway.h"
#include "Server/GameServer.h"

void TCPEventHeartBeat::Execute(NetClient* pClient, VariantVector& data)
{
    if(!pClient || data.size() < 2) {
        return;
    }

    GetMasterBroadway()->SetGlobalOnlineCount(data[1].GetUINT());

    if(data.size() < 5) {
        return;
    }

    const uint32 epochDay = data[2].GetUINT();
    const uint32 eventType = data[3].GetUINT();
    const uint32 eventSeed = data[4].GetUINT();

    TCPDailyQuestData dailyQuest;
    TCPWeeklyEventsData weeklyEvents;
    TCPMonthlyEventsData monthlyEvents;

    const bool hasExtendedData = data.size() >= 29;
    if(hasExtendedData) {
        dailyQuest.questItemOneID = data[5].GetUINT();
        dailyQuest.questItemTwoID = data[6].GetUINT();
        dailyQuest.questItemOneAmount = data[7].GetUINT();
        dailyQuest.questItemTwoAmount = data[8].GetUINT();
        dailyQuest.rewardOneID = data[9].GetUINT();
        dailyQuest.rewardOneAmount = data[10].GetUINT();
        dailyQuest.rewardTwoID = data[11].GetUINT();
        dailyQuest.rewardTwoAmount = data[12].GetUINT();

        weeklyEvents.roleQuestFarmerDay = data[13].GetUINT();
        weeklyEvents.roleQuestBuilderDay = data[14].GetUINT();
        weeklyEvents.roleQuestSurgeonDay = data[15].GetUINT();
        weeklyEvents.roleQuestFishingDay = data[16].GetUINT();
        weeklyEvents.roleQuestChefDay = data[17].GetUINT();
        weeklyEvents.roleQuestCaptainDay = data[18].GetUINT();

        monthlyEvents.lockeDayOne = data[19].GetUINT();
        monthlyEvents.lockeDayTwo = data[20].GetUINT();
        monthlyEvents.carnivalDayOne = data[21].GetUINT();
        monthlyEvents.carnivalDayTwo = data[22].GetUINT();
        monthlyEvents.surgeryDay = data[23].GetUINT();
        monthlyEvents.allHowlsEveDay = data[24].GetUINT();
        monthlyEvents.geigerDay = data[25].GetUINT();
        monthlyEvents.ghostDay = data[26].GetUINT();
        monthlyEvents.mutantKitchenDay = data[27].GetUINT();
        monthlyEvents.voucherDayz = data[28].GetUINT();
    }

    const bool changed = GetMasterBroadway()->SetDailyEventState(
        epochDay,
        eventType,
        eventSeed,
        hasExtendedData ? &dailyQuest : nullptr,
        hasExtendedData ? &weeklyEvents : nullptr,
        hasExtendedData ? &monthlyEvents : nullptr
    );
    if(changed) {
        GetGameServer()->OnDailyEventSync(epochDay, eventType, eventSeed, false);
    }
}
