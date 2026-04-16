#include "News.h"
#include "Utils/DialogBuilder.h"
#include "IO/File.h"
#include "Item/ItemUtils.h"
#include "Server/GameServer.h"

const CommandInfo& News::GetInfo()
{
    static CommandInfo info =
    {
        "/news",
        "Show server news",
        ROLE_PERM_NONE,
        {
            CompileTimeHashString("news")
        }
    };

    return info;
}

void News::Execute(GamePlayer* pPlayer, std::vector<string>& args)
{
    if(!pPlayer || args.empty() || !CheckPerm(pPlayer)) {
        return;
    }

    string content;
    File file;
    if(file.Open("news.txt")) {
        const uint32 size = file.GetSize();
        content.resize(size);
        file.Read(content.data(), size);
        file.Close();
    }

    DialogBuilder db;
    if(!content.empty()) {
        db.AddCustomLine(content);
        db.AddSpacer();
    }

    db.AddLabel("`wDaily and Seasonal Events");
    db.AddTextBox(GetGameServer()->GetDailyEventStatusLine());
    db.EndDialog("command_news", "", "Close");
    pPlayer->SendOnDialogRequest(db.Get());
}
