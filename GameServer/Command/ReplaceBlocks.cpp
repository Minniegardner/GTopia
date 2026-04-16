#include "ReplaceBlocks.h"
#include "Utils/StringUtils.h"
#include "../World/WorldManager.h"
#include "Item/ItemInfoManager.h"

const CommandInfo& ReplaceBlocks::GetInfo()
{
    static CommandInfo info =
    {
        "/replaceblocks <id> <new_id>",
        "Replace blocks in current world",
        ROLE_PERM_COMMAND_MOD,
        {
            CompileTimeHashString("replaceblocks")
        }
    };

    return info;
}

void ReplaceBlocks::Execute(GamePlayer* pPlayer, std::vector<string>& args)
{
    if(!pPlayer || args.empty() || !CheckPerm(pPlayer)) {
        return;
    }

    World* pWorld = GetWorldManager()->GetWorldByID(pPlayer->GetCurrentWorld());
    if(!pWorld) {
        pPlayer->SendOnConsoleMessage("`4Failed to find world.");
        return;
    }

    if(args.size() != 3) {
        pPlayer->SendOnConsoleMessage("ReplaceBlocks incomplete argument. Here's the argument list:");
        pPlayer->SendOnConsoleMessage("-> /ReplaceBlocks <id> <new_id>");
        return;
    }

    uint32 oldID = 0;
    uint32 newID = 0;
    if(ToUInt(args[1], oldID) != TO_INT_SUCCESS || ToUInt(args[2], newID) != TO_INT_SUCCESS) {
        pPlayer->SendOnConsoleMessage("`4Oops:`` ReplaceBlocks requires numeric item ids.");
        return;
    }

    ItemInfo* pNewItem = GetItemInfoManager()->GetItemByID(newID);
    if(!pNewItem) {
        pPlayer->SendOnConsoleMessage("`4Oops:`` Invalid new block id.");
        return;
    }

    int32 count = 0;
    std::vector<TileInfo*> changedTiles;

    Vector2Int size = pWorld->GetTileManager()->GetSize();
    for(int32 y = 0; y < size.y; ++y) {
        for(int32 x = 0; x < size.x; ++x) {
            TileInfo* pTile = pWorld->GetTileManager()->GetTile(x, y);
            if(!pTile) {
                continue;
            }

            if((uint32)pTile->GetDisplayedItem() != oldID) {
                continue;
            }

            ItemInfo* pCurrent = GetItemInfoManager()->GetItemByID(pTile->GetDisplayedItem());
            if(pCurrent && pCurrent->type == ITEM_TYPE_LOCK) {
                continue;
            }

            pTile->SetFG((uint16)newID, pWorld->GetTileManager());
            changedTiles.push_back(pTile);
            ++count;
        }
    }

    if(!changedTiles.empty()) {
        pWorld->SendTileUpdateMultiple(changedTiles);
    }

    pPlayer->SendOnConsoleMessage("Replaced " + ToString(count) + " tiles.");
}
