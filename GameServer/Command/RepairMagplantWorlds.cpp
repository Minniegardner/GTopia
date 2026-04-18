#include "RepairMagplantWorlds.h"
#include "../World/WorldManager.h"

const CommandInfo& RepairMagplantWorlds::GetInfo()
{
    static CommandInfo info =
    {
        "/repairmagplantworlds [dry]",
        "Repair legacy magplant world serialization and rewrite world files",
        ROLE_PERM_COMMAND_MOD,
        {
            CompileTimeHashString("repairmagplantworlds"),
            CompileTimeHashString("repairmagplant"),
            CompileTimeHashString("fixmagplantworlds")
        }
    };

    return info;
}

void RepairMagplantWorlds::Execute(GamePlayer* pPlayer, std::vector<string>& args)
{
    if(!pPlayer || !CheckPerm(pPlayer)) {
        return;
    }

    const bool dryRun = args.size() >= 2 && ToLower(args[1]) == "dry";
    pPlayer->SendOnConsoleMessage(dryRun
        ? "`oScanning world files for legacy magplant format (dry run)..."
        : "`oRepairing legacy magplant world format and creating backups..."
    );

    const uint32 repaired = GetWorldManager()->RepairLegacyMagplantWorlds(true, dryRun);
    pPlayer->SendOnConsoleMessage(dryRun
        ? "`oDry run complete. Candidate world files: `w" + ToString(repaired) + "``"
        : "`oMagplant world repair complete. Rewritten files: `w" + ToString(repaired) + "``"
    );
}
