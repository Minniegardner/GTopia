#include "AddTitle.h"
#include "Utils/StringUtils.h"
#include "CommandTargetUtils.h"
#include "../Server/GameServer.h"
#include "../World/WorldManager.h"

namespace {

string GetTitleHelpText()
{
    return "`oAvailable titles: `wlegend, grow4good, mvp, vip, all``";
}

bool ApplyTitlePermission(GamePlayer* pTarget, const string& titleName)
{
    if(titleName == "legend") {
        pTarget->SetIsLegend(true);
        pTarget->SetLegendaryTitleEnabled(true);
        return true;
    }

    if(titleName == "grow4good" || titleName == "mvp+") {
        pTarget->SetIsGrow4Good(true);
        pTarget->SetGrow4GoodTitleEnabled(true);
        return true;
    }

    if(titleName == "mvp") {
        pTarget->SetIsMVP(true);
        pTarget->SetMaxLevelTitleEnabled(true);
        return true;
    }

    if(titleName == "vip") {
        pTarget->SetIsVIP(true);
        pTarget->SetSuperSupporterTitleEnabled(true);
        return true;
    }

    if(titleName == "all") {
        pTarget->SetIsLegend(true);
        pTarget->SetLegendaryTitleEnabled(true);
        pTarget->SetIsGrow4Good(true);
        pTarget->SetGrow4GoodTitleEnabled(true);
        pTarget->SetIsMVP(true);
        pTarget->SetMaxLevelTitleEnabled(true);
        pTarget->SetIsVIP(true);
        pTarget->SetSuperSupporterTitleEnabled(true);
        return true;
    }

    return false;
}

}

const CommandInfo& AddTitle::GetInfo()
{
    static CommandInfo info =
    {
        "/addtitle <target|#userid> <legend|grow4good|mvp|vip|all>",
        "Give a player a title entitlement",
        ROLE_PERM_COMMAND_ROLEMGMT,
        {
            CompileTimeHashString("addtitle")
        }
    };

    return info;
}

void AddTitle::Execute(GamePlayer* pPlayer, std::vector<string>& args)
{
    if(!pPlayer || args.empty() || !CheckPerm(pPlayer)) {
        return;
    }

    if(args.size() < 3) {
        pPlayer->SendOnConsoleMessage("Usage: " + GetInfo().usage);
        pPlayer->SendOnConsoleMessage(GetTitleHelpText());
        return;
    }

    CommandTargetSpec targetSpec;
    string targetError;
    if(!ParseCommandTargetArg(args[1], true, targetSpec, targetError)) {
        pPlayer->SendOnConsoleMessage(targetError);
        return;
    }

    auto matches = ResolveLocalTargets(targetSpec, false, 0);
    if(!targetSpec.exactMatch && !targetSpec.byUserID && matches.size() > 1) {
        SendAmbiguousLocalTargetList(pPlayer, targetSpec.query, matches, "server");
        return;
    }

    GamePlayer* pTarget = matches.empty() ? nullptr : matches[0];
    if(!pTarget) {
        pPlayer->SendOnConsoleMessage("`4Oops:`` There is nobody currently in this server with a name starting with `w" + targetSpec.query + "``.");
        return;
    }

    string titleName = ToLower(JoinString(args, " ", 2));
    RemoveExtraWhiteSpaces(titleName);
    if(titleName.empty()) {
        pPlayer->SendOnConsoleMessage(GetTitleHelpText());
        return;
    }

    if(!ApplyTitlePermission(pTarget, titleName)) {
        pPlayer->SendOnConsoleMessage("`4Unknown title: `w" + titleName + "``");
        pPlayer->SendOnConsoleMessage(GetTitleHelpText());
        return;
    }

    World* pWorld = GetWorldManager()->GetWorldByID(pTarget->GetCurrentWorld());
    if(pWorld) {
        pWorld->SendNameChangeToAll(pTarget);
        pWorld->SendSetCharPacketToAll(pTarget);
    }

    pTarget->SaveToDatabase();

    pPlayer->SendOnConsoleMessage("`oGiven title entitlement `w" + titleName + "`` to ``" + pTarget->GetRawName() + "``.");
    pTarget->SendOnConsoleMessage("`oYou were given title entitlement `w" + titleName + "`` by ``" + pPlayer->GetDisplayName() + "``.");
}