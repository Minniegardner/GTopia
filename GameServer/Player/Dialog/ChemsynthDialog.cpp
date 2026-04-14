#include "ChemsynthDialog.h"

#include "Item/ItemInfoManager.h"
#include "Utils/DialogBuilder.h"
#include "Utils/StringUtils.h"
#include "World/TileInfo.h"
#include "World/WorldManager.h"
#include "Item/ChemsynthAlgorithm.h"

namespace {

bool ParseIntField(ParsedTextPacket<8>& packet, uint32 key, int32& out)
{
    auto pField = packet.Find(key);
    if(!pField) {
        return false;
    }

    return ToInt(string(pField->value, pField->size), out) == TO_INT_SUCCESS;
}

void ShowInstructionManual(GamePlayer* pPlayer)
{
    if(!pPlayer) {
        return;
    }

    DialogBuilder db;
    db.SetDefaultColor('o')
        ->AddLabelWithIcon("`9GrowTech Chemsynth Manual``", ITEM_ID_CHEMSYNTH_PROCESSOR, true)
        ->AddSpacer()
        ->AddTextBox("Place a Chemsynth Processor with exactly 10 Chemsynth Tanks connected in a straight line to its right.")
        ->AddTextBox("To start synthesis, you need at least 5 of each basic Chemical: R, Y, G, B, and P.")
        ->AddTextBox("While synthesis is running, the active tank colors move forward until a tank matches its target color.")
        ->AddTextBox("When all 10 tanks match their targets, the processor rewards 1 Synthetic Chemical.")
        ->EndDialog("ChemsynthManual", "", "Close");

    pPlayer->SendOnDialogRequest(db.Get());
}

}

namespace ChemsynthDialog {

void Request(GamePlayer* pPlayer, TileInfo* pTile)
{
    if(!pPlayer || !pTile || pPlayer->GetCurrentWorld() == 0) {
        return;
    }

    World* pWorld = GetWorldManager()->GetWorldByID(pPlayer->GetCurrentWorld());
    if(!pWorld) {
        return;
    }

    ItemInfo* pItem = GetItemInfoManager()->GetItemByID(pTile->GetDisplayedItem());
    if(!pItem) {
        return;
    }

    TileInfo* pProcessorTile = pTile;
    if(pItem->type == ITEM_TYPE_CHEMTANK) {
        pProcessorTile = ChemsynthAlgorithm::FindProcessorTile(pWorld, pTile);
        if(!pProcessorTile) {
            return;
        }
    }

    const bool ready = ChemsynthAlgorithm::IsChemsynthReady(pWorld, pProcessorTile);
    const uint32 percent = ChemsynthAlgorithm::GetPercentFinished(pWorld, pProcessorTile);
    const bool active = pProcessorTile->HasFlag(TILE_FLAG_IS_OPEN_TO_PUBLIC);

    DialogBuilder db;
    db.SetDefaultColor('o')
        ->AddLabelWithIcon("`9GrowTech Chemsynth``", ITEM_ID_CHEMSYNTH_PROCESSOR, true)
        ->AddSpacer()
        ->EmbedData("tilex", pProcessorTile->GetPos().x)
        ->EmbedData("tiley", pProcessorTile->GetPos().y)
        ->EmbedData("TileItemID", pProcessorTile->GetDisplayedItem());

    if(!ready) {
        db.AddLabel("The Chemsynth can't function without exactly 10 Chemsynth Tanks connected in a straight line to its right.");
        db.AddSpacer()
            ->AddButton("Help", "Instruction Manual")
            ->EndDialog("Chemsynth", "", "Exit");
    }
    else if(active) {
        db.AddLabel("Chemical Synthesis System online. Synthesis in progress: " + ToString(percent) + "% complete.")
            ->AddSpacer()
            ->AddButton("Help", "Instruction Manual")
            ->AddButton("Shutdown", "Shutdown")
            ->EndDialog("Chemsynth", "", "Exit");
    }
    else {
        db.AddLabel("Chemical Synthesis System online. Press Start to begin synthesis.")
            ->AddSpacer()
            ->AddButton("Help", "Instruction Manual")
            ->EndDialog("Chemsynth", "Start", "Exit");
    }

    pPlayer->SendOnDialogRequest(db.Get());
}

void Handle(GamePlayer* pPlayer, ParsedTextPacket<8>& packet)
{
    if(!pPlayer || pPlayer->GetCurrentWorld() == 0) {
        return;
    }

    int32 tileX = 0;
    int32 tileY = 0;
    if(!ParseIntField(packet, CompileTimeHashString("tilex"), tileX) || !ParseIntField(packet, CompileTimeHashString("tiley"), tileY)) {
        return;
    }

    World* pWorld = GetWorldManager()->GetWorldByID(pPlayer->GetCurrentWorld());
    if(!pWorld) {
        return;
    }

    TileInfo* pTile = pWorld->GetTileManager()->GetTile(tileX, tileY);
    if(!pTile) {
        return;
    }

    auto pButtonClicked = packet.Find(CompileTimeHashString("buttonClicked"));
    string buttonClicked = pButtonClicked ? string(pButtonClicked->value, pButtonClicked->size) : "";

    ItemInfo* pItem = GetItemInfoManager()->GetItemByID(pTile->GetDisplayedItem());
    if(!pItem) {
        return;
    }

    if(buttonClicked == "Help") {
        ShowInstructionManual(pPlayer);
        return;
    }

    if(buttonClicked == "Start" || buttonClicked == "OK" || buttonClicked == "Confirm") {
        TileInfo* pProcessorTile = pTile->GetDisplayedItem() == ITEM_ID_CHEMSYNTH_PROCESSOR ? pTile : ChemsynthAlgorithm::FindProcessorTile(pWorld, pTile);
        if(!pProcessorTile) {
            pPlayer->SendOnTalkBubble("Chemsynth processor not found.", true);
            pPlayer->SendOnConsoleMessage("Chemsynth processor not found.");
            return;
        }

        if(!ChemsynthAlgorithm::IsChemsynthReady(pWorld, pProcessorTile)) {
            pPlayer->SendOnTalkBubble("Chemsynth needs exactly 10 tanks in a straight line to the right.", true);
            pPlayer->SendOnConsoleMessage("Chemsynth needs exactly 10 tanks in a straight line to the right.");
            return;
        }

        if(pProcessorTile->HasFlag(TILE_FLAG_IS_OPEN_TO_PUBLIC)) {
            pPlayer->SendOnTalkBubble("Chemical synthesis is already running.", true);
            pPlayer->SendOnConsoleMessage("Chemical synthesis is already running.");
            return;
        }

        ChemsynthAlgorithm::StartChemsynth(pPlayer, pWorld, pProcessorTile);
        return;
    }

    if(buttonClicked == "Shutdown") {
        TileInfo* pProcessorTile = pTile->GetDisplayedItem() == ITEM_ID_CHEMSYNTH_PROCESSOR ? pTile : ChemsynthAlgorithm::FindProcessorTile(pWorld, pTile);
        if(pProcessorTile) {
            ChemsynthAlgorithm::CancelChemsynth(pWorld, pProcessorTile);
        }
        return;
    }

    if(buttonClicked == "Exit" || buttonClicked == "No") {
        return;
    }
}

}