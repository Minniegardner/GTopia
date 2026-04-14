#include "Find.h"
#include "Utils/DialogBuilder.h"
#include "Item/ItemInfoManager.h"
#include "Item/ItemUtils.h"
#include "Utils/StringUtils.h"

namespace {

void SendFindDialog(GamePlayer* pPlayer, const string& query)
{
    if(!pPlayer) {
        return;
    }

    DialogBuilder db;
    db.SetDefaultColor('o')
        ->AddLabelWithIcon("`wFind Item``", ITEM_ID_GROWSCAN_9000, true)
        ->AddTextInput("SearchString", "Search", query, 64)
        ->AddButton("Find", "Find")
        ->AddSpacer();

    string loweredQuery = ToLower(query);
    uint32 foundItems = 0;
    const uint32 maxItems = 128;

    for(const ItemInfo& item : GetItemInfoManager()->GetItems()) {
        if(loweredQuery.empty()) {
            continue;
        }

        string loweredName = ToLower(item.name);
        if(loweredName.find(loweredQuery) == string::npos) {
            continue;
        }

        db.AddTextBox("`wID: `o" + ToString((int)item.id) + "`` - `w" + item.name + "``");

        ++foundItems;
        if(foundItems >= maxItems) {
            break;
        }
    }

    if(loweredQuery.empty()) {
        db.AddTextBox("`oEnter an item keyword and press Find.");
    }
    else if(foundItems == 0) {
        db.AddTextBox("`4No items found.``");
    }
    else if(foundItems >= maxItems) {
        db.AddTextBox("`oResults capped at `w" + ToString((int)maxItems) + "``. Be more specific.");
    }

    db.EndDialog("find_item_dialog", "", "Exit");
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

    if(args.size() < 2) {
        SendFindDialog(pPlayer, "");
        return;
    }

    string query = JoinString(args, " ", 1);
    RemoveExtraWhiteSpaces(query);
    if(query.size() > 64) {
        query.resize(64);
    }

    SendFindDialog(pPlayer, query);
}
