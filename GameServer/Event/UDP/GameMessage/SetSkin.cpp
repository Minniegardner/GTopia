#include "SetSkin.h"
#include "Utils/StringUtils.h"
#include "World/WorldManager.h"

void SetSkin::Execute(GamePlayer* pPlayer, ParsedTextPacket<8>& packet)
{
    if(!pPlayer || !pPlayer->HasState(PLAYER_STATE_IN_GAME)) {
        return;
    }

    auto pColor = packet.Find(CompileTimeHashString("color"));
    if(!pColor) {
        return;
    }

    uint32 packetSkinColor = 0;
    if(ToUInt(string(pColor->value, pColor->size), packetSkinColor) != TO_INT_SUCCESS) {
        return;
    }

    pPlayer->GetCharData().SetSkinColor(Color(packetSkinColor));

    World* pWorld = GetWorldManager()->GetWorldByID(pPlayer->GetCurrentWorld());
    if(pWorld) {
        pWorld->SendSkinColorUpdateToAll(pPlayer);
    }

}