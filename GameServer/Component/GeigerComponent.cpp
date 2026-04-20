#include "GeigerComponent.h"

#include "../Player/GamePlayer.h"
#include "../World/World.h"
#include "../Server/MasterBroadway.h"
#include "Item/ItemInfo.h"
#include "Item/ItemInfoManager.h"
#include "World/TileInfo.h"
#include "Utils/DialogBuilder.h"

#include <cmath>

namespace {

constexpr const char* kGeigerChargeStartStat = "GEIGER_CHARGE_START_MS";
constexpr const char* kGeigerChargeEndStat = "GEIGER_CHARGE_END_MS";
constexpr const char* kGeigerLastPulseStat = "GEIGER_LAST_PULSE_MS";
constexpr const char* kGeigerIrradiatedUntilStat = "GEIGER_IRRADIATED_UNTIL_MS";

constexpr uint64 kGeigerChargeDurationMS = 30ull * 60ull * 1000ull;
constexpr uint64 kGeigerIrradiatedDurationMS = 30ull * 60ull * 1000ull;

bool HasWorkingCounter(GamePlayer* pPlayer)
{
    return pPlayer && pPlayer->GetInventory().GetCountOfItem(ITEM_ID_GEIGER_COUNTER) > 0;
}

bool HasDeadCounter(GamePlayer* pPlayer)
{
    return pPlayer && pPlayer->GetInventory().GetCountOfItem(ITEM_ID_DEAD_GEIGER_COUNTER) > 0;
}

uint16 RollGeigerDrop()
{
    static const uint16 kDrops[] = {
        ITEM_ID_FOSSIL,
        ITEM_ID_FOSSIL,
        ITEM_ID_FOSSIL_ROCK,
        ITEM_ID_DEEP_FOSSIL_ROCK,
        ITEM_ID_ALIEN_FOSSIL_ROCK,
        ITEM_ID_IGNEOUS_FOSSIL_ROCK,
        ITEM_ID_POLISHED_FOSSIL
    };

    return kDrops[rand() % (sizeof(kDrops) / sizeof(kDrops[0]))];
}

Vector2Int RollGeigerTile(World* pWorld)
{
    if(!pWorld) {
        return { 2, 2 };
    }

    const Vector2Int size = pWorld->GetTileManager()->GetSize();
    const int32 minX = 2;
    const int32 minY = 2;
    const int32 maxX = std::max(minX, size.x - 3);
    const int32 maxY = std::max(minY, size.y - 3);

    return {
        minX + (rand() % std::max(1, maxX - minX + 1)),
        minY + (rand() % std::max(1, maxY - minY + 1))
    };
}

uint32 GetDistanceTile(const Vector2Int& a, const Vector2Int& b)
{
    const int32 dx = a.x - b.x;
    const int32 dy = a.y - b.y;
    return (uint32)std::sqrt((double)((dx * dx) + (dy * dy)));
}

void SendGeigerPulse(GamePlayer* pPlayer, uint32 pulseMode)
{
    if(!pPlayer) {
        return;
    }

    GameUpdatePacket pulsePacket;
    pulsePacket.type = NET_GAME_PACKET_SEND_PARTICLE_EFFECT;
    pulsePacket.posX = pPlayer->GetWorldPos().x + 12.0f;
    pulsePacket.posY = pPlayer->GetWorldPos().y + 15.0f;
    pulsePacket.particleEffectType = 114.0f;
    pulsePacket.field11 = (float)pulseMode;
    pPlayer->SendPacket(&pulsePacket);
}

}

void GeigerComponent::RequestChargerDialog(GamePlayer* pPlayer, TileInfo* pTile)
{
    if(!pPlayer || !pTile) {
        return;
    }

    const uint64 nowMS = Time::GetSystemTime();
    const uint64 chargingStartMS = pPlayer->GetCustomStatValue(kGeigerChargeStartStat);
    const uint64 chargingEndMS = pPlayer->GetCustomStatValue(kGeigerChargeEndStat);

    uint32 chargePercent = 0;
    if(HasWorkingCounter(pPlayer)) {
        chargePercent = 100;
    }
    else if(chargingEndMS > chargingStartMS && nowMS >= chargingStartMS) {
        const uint64 elapsed = std::min<uint64>(chargingEndMS - chargingStartMS, nowMS - chargingStartMS);
        chargePercent = (uint32)((elapsed * 100ull) / (chargingEndMS - chargingStartMS));
    }

    DialogBuilder db;
    db.SetDefaultColor('o')
        ->AddLabelWithIcon("`wGeiger Charger``", ITEM_ID_GEIGER_CHARGER, true)
        ->AddLabel("Current Geiger charge: `w" + ToString(chargePercent) + "%``")
        ->AddLabel("Use this machine to restore your Geiger Counter.")
        ->EmbedData("tilex", pTile->GetPos().x)
        ->EmbedData("tiley", pTile->GetPos().y)
        ->EmbedData("TileItemID", pTile->GetDisplayedItem())
        ->AddButton("Charge", "`2Charge Counter``")
        ->EndDialog("GeigerChargerEdit", "", "Close")
        ->AddQuickExit();

    pPlayer->SendOnDialogRequest(db.Get());
}

bool GeigerComponent::TryChargeCounter(GamePlayer* pPlayer, World* pWorld, TileInfo* pTile)
{
    if(!pPlayer || !pWorld || !pTile || pTile->GetDisplayedItem() != ITEM_ID_GEIGER_CHARGER) {
        return false;
    }

    if(HasWorkingCounter(pPlayer)) {
        pPlayer->SendOnTalkBubble("`2Your Geiger Counter is already charged!", true);
        return false;
    }

    if(!HasDeadCounter(pPlayer)) {
        pPlayer->SendOnTalkBubble("`4You need a Dead Geiger Counter first!", true);
        return false;
    }

    const uint64 nowMS = Time::GetSystemTime();
    const uint64 chargingEndMS = pPlayer->GetCustomStatValue(kGeigerChargeEndStat);
    if(chargingEndMS > nowMS) {
        pPlayer->SendOnTalkBubble("`wYour Geiger Counter is already charging.", true);
        return false;
    }

    pPlayer->SetCustomStatValue(kGeigerChargeStartStat, nowMS);
    pPlayer->SetCustomStatValue(kGeigerChargeEndStat, nowMS + kGeigerChargeDurationMS);
    pPlayer->SendOnTalkBubble("`wYour Geiger Counter has started charging.", true);
    pWorld->PlaySFXForEveryone("powerup.wav", 0);
    return true;
}

void GeigerComponent::UpdatePlayerScan(GamePlayer* pPlayer, World* pWorld)
{
    if(!pPlayer || !pWorld || !pPlayer->HasState(PLAYER_STATE_IN_GAME)) {
        return;
    }

    const uint64 nowMS = Time::GetSystemTime();
    const uint64 chargingEndMS = pPlayer->GetCustomStatValue(kGeigerChargeEndStat);
    if(chargingEndMS != 0) {
        if(nowMS >= chargingEndMS) {
            if(HasDeadCounter(pPlayer)) {
                pPlayer->ModifyInventoryItem(ITEM_ID_DEAD_GEIGER_COUNTER, -1);
                pPlayer->ModifyInventoryItem(ITEM_ID_GEIGER_COUNTER, 1);
                pPlayer->SendOnConsoleMessage("`2Your Geiger Counter is fully charged.");
            }

            pPlayer->SetCustomStatValue(kGeigerChargeStartStat, 0);
            pPlayer->SetCustomStatValue(kGeigerChargeEndStat, 0);
        }
        else {
            return;
        }
    }

    if(!HasWorkingCounter(pPlayer)) {
        return;
    }

    if(pPlayer->GetCustomStatValue(kGeigerIrradiatedUntilStat) > nowMS) {
        return;
    }

    const Vector2Float playerPos = pPlayer->GetWorldPos();
    const Vector2Int currentTilePos = { (int32)(playerPos.x / 32.0f), (int32)(playerPos.y / 32.0f) };
    const uint32 distance = GetDistanceTile(currentTilePos, pWorld->GetGeigerTilePos());

    const uint64 lastPulseMS = pPlayer->GetCustomStatValue(kGeigerLastPulseStat);

    if(distance == 0) {
        if(nowMS - lastPulseMS < 2500ull) {
            return;
        }

        pPlayer->SetCustomStatValue(kGeigerLastPulseStat, nowMS);

        const uint16 rewardItemID = RollGeigerDrop();
        const ItemInfo* pRewardItem = GetItemInfoManager()->GetItemByID(rewardItemID);
        if(!pRewardItem) {
            return;
        }

        pPlayer->IncreaseStat("FOUND_RADIOACTIVE_ITEMS");
        pPlayer->SendOnTalkBubble("`wI found `21 " + pRewardItem->name + "`w!", false);

        pWorld->SendConsoleMessageToAll("`o" + pPlayer->GetDisplayName() + "`` `ofound `21 " + pRewardItem->name + "`o!");
        pWorld->SetGeigerTilePos(RollGeigerTile(pWorld));

        if(pPlayer->GetInventory().HaveRoomForItem(rewardItemID, 1)) {
            pPlayer->ModifyInventoryItem(rewardItemID, 1);
        }
        else {
            WorldObject obj;
            obj.itemID = rewardItemID;
            obj.count = 1;
            obj.pos = pPlayer->GetWorldPos();
            pWorld->DropObject(obj);
        }

        GameUpdatePacket fxPacket;
        fxPacket.type = NET_GAME_PACKET_ITEM_EFFECT;
        fxPacket.posX = pPlayer->GetWorldPos().x + 16.0f;
        fxPacket.posY = pPlayer->GetWorldPos().y + 16.0f;
        fxPacket.field2 = 5;
        fxPacket.itemID = rewardItemID;
        fxPacket.netID = pPlayer->GetNetID();
        fxPacket.field9 = 1.0f;
        pWorld->SendGamePacketToAll(&fxPacket);

        if(pPlayer->GetInventory().GetCountOfItem(ITEM_ID_GEIGER_COUNTER) > 0) {
            pPlayer->ModifyInventoryItem(ITEM_ID_GEIGER_COUNTER, -1);
            pPlayer->ModifyInventoryItem(ITEM_ID_DEAD_GEIGER_COUNTER, 1);
        }

        pPlayer->SetCustomStatValue(kGeigerIrradiatedUntilStat, nowMS + kGeigerIrradiatedDurationMS);
        SendGeigerPulse(pPlayer, 3);
        return;
    }

    uint64 pulseDelayMS = 0;
    uint32 pulseMode = 0;

    if(distance >= 1 && distance < 4) {
        pulseDelayMS = std::max<uint64>(800ull, (uint64)distance * 450ull);
        pulseMode = 2;
    }
    else if(distance >= 4 && distance < 8) {
        pulseDelayMS = 1500;
        pulseMode = 2;
    }
    else if(distance >= 8 && distance < 15) {
        pulseDelayMS = 1500;
        pulseMode = 1;
    }
    else if(distance >= 15 && distance <= 30) {
        pulseDelayMS = 1500;
        pulseMode = 0;
    }

    if(pulseDelayMS == 0) {
        return;
    }

    if(nowMS - lastPulseMS < pulseDelayMS) {
        return;
    }

    pPlayer->SetCustomStatValue(kGeigerLastPulseStat, nowMS);
    SendGeigerPulse(pPlayer, pulseMode);
}

void GeigerComponent::TrySpawnRadioactiveDrop(GamePlayer* pPlayer, World* pWorld, TileInfo* pTile, const ItemInfo* pBrokenItem)
{
    (void)pPlayer;
    (void)pWorld;
    (void)pTile;
    (void)pBrokenItem;
}
