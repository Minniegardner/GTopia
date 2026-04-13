#include "State.h"
#include "Utils/Timer.h"

void State::Execute(GamePlayer* pPlayer, World* pWorld, GameUpdatePacket* pPacket)
{
    if(!pPlayer || !pWorld || !pPacket) {
        return;
    }

    if(pPacket->posX == 0 || pPacket->posY == 0) {
        return;
    }

    Vector2Int worldSize = pWorld->GetTileManager()->GetSize();
    const float worldMaxX = static_cast<float>(worldSize.x * 32);
    const float worldMaxY = static_cast<float>(worldSize.y * 32);

    if(pPacket->posX < 0 || pPacket->posY < 0 || pPacket->posX > worldMaxX || pPacket->posY > worldMaxY) {
        pPlayer->SendFakePingReply();
        return;
    }

    if(!pPlayer->CanProcessMovePacket(pPacket->posX, pPacket->posY, Time::GetSystemTime())) {
        pPlayer->SendFakePingReply();
        return;
    }

    if(pPacket->HasFlag(NET_GAME_PACKET_FLAGS_FACINGLEFT)) {
        pPlayer->GetCharData().SetPlayerFlag(PLAYER_FLAG_FACING_LEFT);
    }
    else {
        pPlayer->GetCharData().RemovePlayerFlag(PLAYER_FLAG_FACING_LEFT);
    }

    pPlayer->SetWorldPos(pPacket->posX, pPacket->posY);
    pPlayer->SendPositionToWorldPlayers();

    pPacket->netID = pPlayer->GetNetID();
    pWorld->SendGamePacketToAll(pPacket);
}