#include "MenuClickSocial.h"

#include "Utils/DialogBuilder.h"

void MenuClickSocial::Execute(GamePlayer* pPlayer, ParsedTextPacket<8>& packet)
{
    if(!pPlayer || !pPlayer->HasState(PLAYER_STATE_IN_GAME)) {
        return;
    }

    DialogBuilder db;
    db.SetDefaultColor('o');
    db.AddLabelWithIcon("`wSocial Portal``", ITEM_ID_WRENCH, true);
    db.AddSpacer();
    db.AddButton("GotoFriendsMenu", "Show Friends");
    db.AddButton("GotoGuildMenu", "Guild Menu");
    db.AddButton("CreateGuild", "Create Guild");
    db.AddButton("GotoTradeHistory", "Trade History");
    db.AddQuickExit();
    db.EndDialog("SocialPortal", "", "OK");

    pPlayer->SendOnDialogRequest(db.Get());
}