#include "Effect.h"
#include "Utils/StringUtils.h"
#include "../World/WorldManager.h"

const CommandInfo& Effect::GetInfo()
{
    static CommandInfo info =
    {
        "/eff <effect_id> [mode_id]",
        "Send particle effect in current world",
        ROLE_PERM_COMMAND_MOD,
        {
            CompileTimeHashString("eff"),
            CompileTimeHashString("effect")
        }
    };

    return info;
}

void Effect::Execute(GamePlayer* pPlayer, std::vector<string>& args)
{
    if(!pPlayer || args.empty() || !CheckPerm(pPlayer)) {
        return;
    }

    World* pWorld = GetWorldManager()->GetWorldByID(pPlayer->GetCurrentWorld());
    if(!pWorld) {
        return;
    }

    if(args.size() < 2) {
        pPlayer->SendOnConsoleMessage("Effect command incomplete argument. Here's the argument list:");
        pPlayer->SendOnConsoleMessage("-> /eff <effect id>");
        pPlayer->SendOnConsoleMessage("-> /eff <effect id> <mode id>");
        return;
    }

    uint32 effectID = 0;
    if(ToUInt(args[1], effectID) != TO_INT_SUCCESS) {
        pPlayer->SendOnConsoleMessage("`4Invalid effect id.");
        return;
    }

    uint32 effectMode = 0;
    if(args.size() > 2 && ToUInt(args[2], effectMode) != TO_INT_SUCCESS) {
        pPlayer->SendOnConsoleMessage("`4Invalid mode id.");
        return;
    }

    const Vector2Float pos = pPlayer->GetWorldPos();
    pWorld->SendParticleEffectToAll(pos.x + 12.0f, pos.y + 15.0f, effectID, (float)effectMode);

    pPlayer->SendOnConsoleMessage("Sent Effect " + ToString(effectID) + " with mode " + ToString(effectMode) + ".");
}
