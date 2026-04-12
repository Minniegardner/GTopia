#include "StoreNavigate.h"
#include "Buy.h"

void StoreNavigate::Execute(GamePlayer* pPlayer, ParsedTextPacket<8>& packet)
{
    Buy::Execute(pPlayer, packet);
}
