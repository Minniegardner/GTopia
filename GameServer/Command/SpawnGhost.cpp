#include "SpawnGhost.h"

#include "Algorithm/GhostAlgorithm.h"
#include "Utils/StringUtils.h"

const CommandInfo& SpawnGhost::GetInfo()
{
    static CommandInfo info =
    {
        "/spawnghost [type] [tile_x tile_y]",
        "Spawn ghost (type optional: 1,4,6,7,11,12)",
        ROLE_PERM_COMMAND_GHOST,
        {
            CompileTimeHashString("spawnghost"),
            CompileTimeHashString("sghost")
        }
    };

    return info;
}

void SpawnGhost::Execute(GamePlayer* pPlayer, std::vector<string>& args)
{
    if(!pPlayer || args.empty() || !CheckPerm(pPlayer)) {
        return;
    }

    World* pWorld = GetWorldManager()->GetWorldByID(pPlayer->GetCurrentWorld());
    if(!pWorld) {
        pPlayer->SendOnConsoleMessage("`4Failed to find world.");
        return;
    }

    Vector2Float spawnPos = pPlayer->GetWorldPos();
    spawnPos.x += 16.0f;
    spawnPos.y += 16.0f;
    uint8 requestedType = 0;

    if(args.size() == 2) {
        int32 parsedType = 0;
        if(ToInt(args[1], parsedType) != TO_INT_SUCCESS) {
            pPlayer->SendOnConsoleMessage("`4Usage: /spawnghost [type] [tile_x tile_y]");
            return;
        }

        requestedType = (uint8)parsedType;
    }
    else if(args.size() == 3) {
        int32 tileX = 0;
        int32 tileY = 0;
        if(ToInt(args[1], tileX) != TO_INT_SUCCESS || ToInt(args[2], tileY) != TO_INT_SUCCESS) {
            pPlayer->SendOnConsoleMessage("`4Usage: /spawnghost [type] [tile_x tile_y]");
            return;
        }

        spawnPos.x = (tileX * 32.0f) + 16.0f;
        spawnPos.y = (tileY * 32.0f) + 16.0f;
    }
    else if(args.size() == 4) {
        int32 parsedType = 0;
        int32 tileX = 0;
        int32 tileY = 0;
        if(ToInt(args[1], parsedType) != TO_INT_SUCCESS || ToInt(args[2], tileX) != TO_INT_SUCCESS || ToInt(args[3], tileY) != TO_INT_SUCCESS) {
            pPlayer->SendOnConsoleMessage("`4Usage: /spawnghost [type] [tile_x tile_y]");
            return;
        }

        requestedType = (uint8)parsedType;
        spawnPos.x = (tileX * 32.0f) + 16.0f;
        spawnPos.y = (tileY * 32.0f) + 16.0f;
    }
    else if(args.size() != 1) {
        pPlayer->SendOnConsoleMessage("`4Usage: /spawnghost [type] [tile_x tile_y]");
        return;
    }

    if(!GhostAlgorithm::SpawnGhostAt(pWorld, spawnPos, requestedType)) {
        pPlayer->SendOnConsoleMessage("`4Failed to spawn ghost.");
        return;
    }

    pPlayer->SendOnConsoleMessage("`oGhost spawned.");
}
