#include "MenuClickSocial.h"

void MenuClickSocial::Execute(GamePlayer* pPlayer, ParsedTextPacket<8>& packet)
{
    if(!pPlayer || !pPlayer->HasState(PLAYER_STATE_IN_GAME)) {
        return;
    }

    pPlayer->SendSocialPortal();
}