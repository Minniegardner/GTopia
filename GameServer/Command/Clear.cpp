#include "Clear.h"
#include "Utils/StringUtils.h"
#include "Math/Rect.h"
#include "Source/World/WorldObjectManager.h"
#include "../World/WorldManager.h"

const CommandInfo& Clear::GetInfo()
{
    static CommandInfo info =
    {
        "/clear <drop|tile|npc> <all|id>",
        "Clear drops, tiles, or npcs in current world",
        ROLE_PERM_COMMAND_GIVEITEM,
        {
            CompileTimeHashString("clear")
        }
    };

    return info;
}

void Clear::Execute(GamePlayer* pPlayer, std::vector<string>& args)
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
        pPlayer->SendOnConsoleMessage("Clear incomplete argument. Here's the argument list:");
        pPlayer->SendOnConsoleMessage("-> /clear drop all");
        pPlayer->SendOnConsoleMessage("-> /clear drop <item id>");
        pPlayer->SendOnConsoleMessage("-> /clear tile all");
        pPlayer->SendOnConsoleMessage("-> /clear tile <item id>");
        pPlayer->SendOnConsoleMessage("-> /clear npc all");
        pPlayer->SendOnConsoleMessage("-> /clear npc <npc type>");
        return;
    }

    const string mode = ToLower(args[1]);
    const string filter = ToLower(args[2]);

    if(mode != "drop") {
        pPlayer->SendOnConsoleMessage("`4This source currently supports only `/clear drop`.");
        return;
    }

    uint32 itemIDFilter = 0;
    const bool clearAll = filter == "all";
    if(!clearAll && ToUInt(filter, itemIDFilter) != TO_INT_SUCCESS) {
        pPlayer->SendOnConsoleMessage("`4Invalid item id. Use `/clear drop <item id>` or `/clear drop all`.");
        return;
    }

    const Vector2Int worldSize = pWorld->GetTileManager()->GetSize();
    RectFloat worldRect(0.0f, 0.0f, worldSize.x * 32.0f, worldSize.y * 32.0f);
    auto objects = pWorld->GetObjectManager()->GetObjectsInRect(worldRect);

    std::vector<uint32> removeIDs;
    removeIDs.reserve(objects.size());

    for(WorldObject* pObj : objects) {
        if(!pObj) {
            continue;
        }

        if(clearAll || pObj->itemID == (uint16)itemIDFilter) {
            removeIDs.push_back(pObj->objectID);
        }
    }

    for(uint32 objectID : removeIDs) {
        pWorld->RemoveObject(objectID);
    }

    pPlayer->SendOnConsoleMessage("Removed " + ToString((uint32)removeIDs.size()) + " objects.");
}
