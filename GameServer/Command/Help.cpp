#include "Help.h"
#include "Utils/StringUtils.h"
#include "Item/ItemUtils.h"
#include "../Server/GameServer.h"

#include <algorithm>

namespace {

string GetPrimaryCommandName(const CommandInfo& info)
{
    if(info.usage.empty()) {
        return "";
    }

    size_t start = 0;
    while(start < info.usage.size() && info.usage[start] == ' ') {
        ++start;
    }

    if(start < info.usage.size() && info.usage[start] == '/') {
        ++start;
    }

    size_t end = info.usage.find(' ', start);
    if(end == string::npos) {
        end = info.usage.size();
    }

    return info.usage.substr(start, end - start);
}

bool MatchesHelpQuery(const CommandInfo& info, const string& query)
{
    if(query.empty()) {
        return false;
    }

    string normalizedQuery = query;
    if(!normalizedQuery.empty() && normalizedQuery[0] == '/') {
        normalizedQuery.erase(normalizedQuery.begin());
    }

    if(normalizedQuery.empty()) {
        return false;
    }

    const uint32 queryHash = HashString(ToLower(normalizedQuery));
    for(uint32 aliasHash : info.aliases) {
        if(aliasHash == queryHash) {
            return true;
        }
    }

    return false;
}

}

const CommandInfo& Help::GetInfo()
{
    static CommandInfo info =
    {
        "/help",
        "Show available commands",
        ROLE_PERM_NONE,
        {
            CompileTimeHashString("help"),
            CompileTimeHashString("?")
        }
    };

    return info;
}

void Help::Execute(GamePlayer* pPlayer, std::vector<string>& args)
{
    if(!pPlayer || args.empty() || !CheckPerm(pPlayer)) {
        return;
    }

    if(args.size() >= 2) {
        const string query = args[1];
        const auto& commandInfos = GetGameServer()->GetCommandInfos();

        for(const CommandInfo* pInfo : commandInfos) {
            if(!pInfo || pInfo->disabled) {
                continue;
            }

            if(!GetGameServer()->CanAccessCommand(pPlayer, *pInfo)) {
                continue;
            }

            if(!MatchesHelpQuery(*pInfo, query)) {
                continue;
            }

            pPlayer->SendOnConsoleMessage("`o>> `w/" + GetPrimaryCommandName(*pInfo) + "``");
            pPlayer->SendOnConsoleMessage("`oUsage: `w" + pInfo->usage + "``");
            pPlayer->SendOnConsoleMessage("`oInfo: `w" + pInfo->desc + "``");
            return;
        }

        pPlayer->SendOnConsoleMessage("`4Unknown command.``  Enter `$/help`` for a list of valid commands.");
        return;
    }

    const auto& commandInfos = GetGameServer()->GetCommandInfos();
    std::vector<string> commandNames;
    commandNames.reserve(commandInfos.size());

    for(const CommandInfo* pInfo : commandInfos) {
        if(!pInfo || pInfo->aliases.empty()) {
            continue;
        }

        if(!GetGameServer()->CanAccessCommand(pPlayer, *pInfo)) {
            continue;
        }

        commandNames.push_back("/" + GetPrimaryCommandName(*pInfo));
    }

    std::sort(commandNames.begin(), commandNames.end(), [](const string& lhs, const string& rhs) {
        return ToLower(lhs) < ToLower(rhs);
    });

    string commands = "`o>> Commands: ";
    for(size_t i = 0; i < commandNames.size(); ++i) {
        if(i == 0) {
            commands += commandNames[i];
        }
        else {
            commands += ", " + commandNames[i];
        }
    }

    commands += "``";
    pPlayer->SendOnConsoleMessage(commands);
}
