#include "Find.h"
#include "Utils/DialogBuilder.h"
#include "Item/ItemInfoManager.h"
#include "Item/ItemUtils.h"
#include "Utils/StringUtils.h"

namespace {

constexpr const char* kFindDialogName = "FindItem";
constexpr uint16 kFindDialogIcon = ITEM_ID_GROWSCAN_9000;
constexpr uint32 kFindMaxItems = 128;
constexpr uint32 kFindMaxQueryLength = 250;

bool IsRestrictedFindItem(const ItemInfo& item)
{
    if(item.type == ITEM_TYPE_LOCK) {
        return true;
    }

    if(item.HasFlag(ITEM_FLAG_MOD) || item.HasFlag(ITEM_FLAG_UNTRADEABLE) || item.HasFlag(ITEM_FLAG_BETA) || item.HasFlag(ITEM_FLAG_DROPLESS)) {
        return true;
    }

    string loweredName = ToLower(item.name);
    return loweredName.find("voucher") != string::npos || loweredName.find("crate") != string::npos;
}

void SendFindDialog(GamePlayer* pPlayer, const string& query, const string& message = "")
{
    if(!pPlayer) {
        return;
    }

    DialogBuilder db;
    db.SetDefaultColor('o')
        ->AddLabelWithIcon("`wFind Item``", kFindDialogIcon, true)
        ->AddTextInput("SearchString", "Search", query, kFindMaxQueryLength)
        ->AddButton("Find", "Find")
        ->AddSpacer();

    if(!message.empty()) {
        db.AddTextBox(message);
    }

    string loweredQuery = ToLower(query);
    RemoveExtraWhiteSpaces(loweredQuery);

    if(loweredQuery.empty()) {
        db.AddTextBox("`oSearch for any item and press Find.");
        db.EndDialog(kFindDialogName, "", "Exit");
        pPlayer->SendOnDialogRequest(db.Get());
        return;
    }

    uint32 foundItems = 0;
    for(const ItemInfo& item : GetItemInfoManager()->GetItems()) {
        string loweredName = ToLower(item.name);
        if(loweredName.find(loweredQuery) == string::npos) {
            continue;
        }

        string line = "`w" + item.name + "`` `o(#" + ToString((int)item.id) + ")``";
        if(IsRestrictedFindItem(item)) {
            line += " `4[Restricted]``";
        }

        db.AddButton(ToString((int)item.id), line);

        ++foundItems;
        if(foundItems >= kFindMaxItems) {
            break;
        }
    }

    if(foundItems == 0) {
        db.AddTextBox("`4No items found.``");
    }
    else {
        db.AddTextBox("`oFound `w" + ToString((int)foundItems) + "`` item(s). Tap an item to claim.");
        if(foundItems >= kFindMaxItems) {
            db.AddTextBox("`oResults capped at `w" + ToString((int)kFindMaxItems) + "``. Be more specific.");
        }
    }

    db.EndDialog(kFindDialogName, "", "Exit");
    pPlayer->SendOnDialogRequest(db.Get());
}

}

const CommandInfo& Find::GetInfo()
{
    static CommandInfo info =
    {
        "/find [item_name]",
        "Find items by name",
        ROLE_PERM_NONE,
        {
            CompileTimeHashString("find")
        }
    };

    return info;
}

void Find::Execute(GamePlayer* pPlayer, std::vector<string>& args)
{
    if(!pPlayer || args.empty() || !CheckPerm(pPlayer)) {
        return;
    }

    string query;
    if(args.size() >= 2) {
        query = JoinString(args, " ", 1);
        RemoveExtraWhiteSpaces(query);
        if(query.size() > kFindMaxQueryLength) {
            query.resize(kFindMaxQueryLength);
        }
    }

    SendFindDialog(pPlayer, query);
}
