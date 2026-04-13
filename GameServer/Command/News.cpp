#include "News.h"
#include "Utils/DialogBuilder.h"
#include "IO/File.h"
#include "Item/ItemUtils.h"

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
    db.SetDefaultColor('o')
        ->AddLabelWithIcon("`wNews``", ITEM_ID_NEWSPAPER, true);

    if(content.empty()) {
        db.AddTextBox("`oNo news available.");
    }
    else {
        db.AddTextBox(content);
    }

    db.EndDialog("command_news", "", "Close");
    pPlayer->SendOnDialogRequest(db.Get());
}
