#include "SetSkin.h"
#include "Utils/StringUtils.h"

void SetSkin::Execute(GamePlayer* pPlayer, ParsedTextPacket<8>& packet)
{
    auto pColor = packet.Find(CompileTimeHashString("color"));
    if(!pColor) {
        return;
    }

    uint32 packetSkinColor = 0;
    /*really need to convert to string?*/
    if(ToUInt(string(pColor->value, pColor->size), packetSkinColor) != TO_INT_SUCCESS) {
        return;
    }

    /**
     * check colors for special colors like supporter/super supp
     * and return if no access
     */

    
}