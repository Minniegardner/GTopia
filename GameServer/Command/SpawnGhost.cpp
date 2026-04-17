#include "SpawnGhost.h"

#include "Algorithm/GhostAlgorithm.h"
#include "Utils/StringUtils.h"

const CommandInfo& SpawnGhost::GetInfo()
{
    static CommandInfo info =
    {
        "/spawnghost [tile_x tile_y]",
        "Spawn a ghost for testing in current world",
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

    if(args.size() == 3) {
        int32 tileX = 0;
        int32 tileY = 0;
        if(ToInt(args[1], tileX) != TO_INT_SUCCESS || ToInt(args[2], tileY) != TO_INT_SUCCESS) {
            pPlayer->SendOnConsoleMessage("`4Usage: /spawnghost [tile_x tile_y]");
            return;
        }

        spawnPos.x = (tileX * 32.0f) + 16.0f;
        spawnPos.y = (tileY * 32.0f) + 16.0f;
    }
    else if(args.size() != 1) {
        pPlayer->SendOnConsoleMessage("`4Usage: /spawnghost [tile_x tile_y]");
        return;
    }

    if(!GhostAlgorithm::SpawnGhostAt(pWorld, spawnPos)) {
        pPlayer->SendOnConsoleMessage("`4Failed to spawn ghost.");
        return;
    }

    pPlayer->SendOnConsoleMessage("`oGhost spawned.");
}
