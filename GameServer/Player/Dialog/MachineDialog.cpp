#include "MachineDialog.h"

#include "Item/ItemInfoManager.h"
#include "Algorithm/LockAlgorithm.h"
#include "Utils/DialogBuilder.h"
#include "Utils/StringUtils.h"
#include "World/TileInfo.h"
#include "../GamePlayer.h"
#include "../../World/WorldManager.h"

namespace {

bool ParseIntField(ParsedTextPacket<8>& packet, uint32 key, int32& out)
{
    auto pField = packet.Find(key);
    if(!pField) {
        return false;
    }

    return ToInt(string(pField->value, pField->size), out) == TO_INT_SUCCESS;
}

bool ParseBoolField(ParsedTextPacket<8>& packet, uint32 key, bool& out)
{
    auto pField = packet.Find(key);
    if(!pField) {
        return false;
    }

    string value(pField->value, pField->size);
    ToLower(value);

    if(value == "1" || value == "true" || value == "yes" || value == "on") {
        out = true;
        return true;
    }

    if(value == "0" || value == "false" || value == "no" || value == "off") {
        out = false;
        return true;
    }

    int32 numericValue = 0;
    if(ToInt(value, numericValue) == TO_INT_SUCCESS) {
        out = numericValue != 0;
        return true;
    }

    return false;
}

bool IsBlockedMachineItem(ItemInfo* pItem)
{
    if(!pItem) {
        return true;
    }

    if(pItem->HasFlag(ITEM_FLAG_UNTRADEABLE)) {
        return true;
    }

    if(pItem->type == ITEM_TYPE_CLOTHES || pItem->type == ITEM_TYPE_GEMS || pItem->type == ITEM_TYPE_FIST || pItem->type == ITEM_TYPE_WRENCH || pItem->type == ITEM_TYPE_LOCK) {
        return true;
    }

    return false;
}

int32 GetWorldLockValue(GamePlayer* pPlayer)
{
    if(!pPlayer) {
        return 0;
    }

    auto& inv = pPlayer->GetInventory();
    return (int32)inv.GetCountOfItem(ITEM_ID_WORLD_LOCK) +
        ((int32)inv.GetCountOfItem(ITEM_ID_DIAMOND_LOCK) * 100) +
        ((int32)inv.GetCountOfItem(ITEM_ID_BLUE_GEM_LOCK) * 10000);
}

void RemoveAllLockTypes(GamePlayer* pPlayer)
{
    if(!pPlayer) {
        return;
    }

    auto& inv = pPlayer->GetInventory();
    inv.RemoveItem(ITEM_ID_WORLD_LOCK, pPlayer);
    inv.RemoveItem(ITEM_ID_DIAMOND_LOCK, pPlayer);
    inv.RemoveItem(ITEM_ID_BLUE_GEM_LOCK, pPlayer);
}

void ReturnLockChange(GamePlayer* pPlayer, int32 totalWorldLocks)
{
    if(!pPlayer || totalWorldLocks <= 0) {
        return;
    }

    int32 returnBGL = 0;
    int32 returnDL = 0;
    int32 returnWL = totalWorldLocks;

    while(returnWL >= 10000) {
        ++returnBGL;
        returnWL -= 10000;
    }

    while(returnWL >= 100) {
        ++returnDL;
        returnWL -= 100;
    }

    if(returnWL > 0) {
        pPlayer->ModifyInventoryItem(ITEM_ID_WORLD_LOCK, (int16)returnWL);
    }

    if(returnDL > 0) {
        pPlayer->ModifyInventoryItem(ITEM_ID_DIAMOND_LOCK, (int16)returnDL);
    }

    if(returnBGL > 0) {
        pPlayer->ModifyInventoryItem(ITEM_ID_BLUE_GEM_LOCK, (int16)returnBGL);
    }
}

bool IsCompatibleVendItem(ItemInfo* pItem)
{
    if(!pItem) {
        return false;
    }

    if(pItem->HasFlag(ITEM_FLAG_UNTRADEABLE) || pItem->HasFlag(ITEM_FLAG_DROPLESS)) {
        return false;
    }

    if(pItem->id == ITEM_ID_WORLD_KEY || pItem->type == ITEM_TYPE_LOCK || IsWorldLock(pItem->id)) {
        return false;
    }

    if(pItem->type == ITEM_TYPE_CLOTHES || pItem->type == ITEM_TYPE_GEMS || pItem->type == ITEM_TYPE_FIST || pItem->type == ITEM_TYPE_WRENCH) {
        return false;
    }

    return true;
}

bool IsCompatibleMagplantItem(uint16 machineID, ItemInfo* pItem)
{
    if(!pItem || !IsCompatibleVendItem(pItem)) {
        return false;
    }

    if(pItem->HasFlag(ITEM_FLAG_UNTRADEABLE) || pItem->HasFlag(ITEM_FLAG_DROPLESS)) {
        return false;
    }

    if(pItem->type == ITEM_TYPE_CLOTHES || pItem->type == ITEM_TYPE_ANCES || pItem->type == ITEM_TYPE_GEMS) {
        return false;
    }

    if(machineID == ITEM_ID_UNSTABLE_TESSERACT && pItem->type == ITEM_TYPE_SEED) {
        return false;
    }

    if(machineID == ITEM_ID_GAIAS_BEACON && pItem->type != ITEM_TYPE_SEED) {
        return false;
    }

    return true;
}

void EmitMachineUpdateEffects(World* pWorld, TileInfo* pTile)
{
    if(!pWorld || !pTile) {
        return;
    }

    const Vector2Int tilePos = pTile->GetPos();
    pWorld->SendParticleEffectToAll((tilePos.x * 32.0f) + 16.0f, (tilePos.y * 32.0f) + 16.0f, 44, 0, 0);
    pWorld->PlaySFXForEveryone("terraform.wav", 0);
}

void UpdateVendingMachineTile(World* pWorld, TileInfo* pTile, bool effects)
{
    if(!pWorld || !pTile) {
        return;
    }

    if(effects) {
        EmitMachineUpdateEffects(pWorld, pTile);
    }

    pWorld->SendTileUpdate(pTile);
}

void ShowVendingPurchaseDialog(GamePlayer* pPlayer, TileInfo* pTile, TileExtra_Vending* pData, int32 buyCount, int32 totalItemsGive, int32 totalPriceWLS)
{
    if(!pPlayer || !pTile || !pData) {
        return;
    }

    ItemInfo* pStock = GetItemInfoManager()->GetItemByID(pData->itemID);

    DialogBuilder db;
    db.SetDefaultColor('o')
        ->AddQuickExit()
        ->AddLabelWithIcon("`wPurchase Confirmation``", ITEM_ID_DUMB_FRIEND, true)
        ->AddSpacer()
        ->AddSpacer()
        ->AddTextBox("`4You'll give:``")
        ->AddSpacer()
        ->AddLabelWithIcon("(`w" + ToString(totalPriceWLS) + "``) `8World Locks``", ITEM_ID_WORLD_LOCK, false)
        ->AddSpacer()
        ->AddTextBox("`2You'll get:``")
        ->AddLabelWithIcon("(`w" + ToString(totalItemsGive) + "``) `2" + string(pStock ? pStock->name : "item") + "``", pData->itemID, false)
        ->AddSpacer()
        ->AddSpacer()
        ->AddTextBox("Are you sure that you really wanna do this purchase?")
        ->EmbedData("tilex", pTile->GetPos().x)
        ->EmbedData("tiley", pTile->GetPos().y)
        ->EmbedData("TileItemID", pTile->GetDisplayedItem())
        ->EmbedData("CurrentVendPrice", pData->price)
        ->EmbedData("CurrentVendItemID", pData->itemID)
        ->EmbedData("BuyCount", buyCount)
        ->EndDialog("VendConfirm", "Confirm", "Cancel");

    pPlayer->SendOnDialogRequest(db.Get());
}

void ShowMagplantAmountDialog(GamePlayer* pPlayer, TileInfo* pTile, TileExtra_Magplant* pData, bool isAdd)
{
    if(!pPlayer || !pTile || !pData) {
        return;
    }

    ItemInfo* pMachine = GetItemInfoManager()->GetItemByID(pTile->GetDisplayedItem());
    ItemInfo* pStock = GetItemInfoManager()->GetItemByID(pData->itemID);
    if(!pStock || pData->itemID == ITEM_ID_BLANK) {
        return;
    }

    const string machineName = pMachine ? string(pMachine->name) : string("Magplant");

    DialogBuilder db;
    db.SetDefaultColor('o')
        ->AddQuickExit()
        ->AddLabelWithIcon("`w" + string(pMachine ? pMachine->name : "Magplant") + "``", pMachine ? pMachine->id : ITEM_ID_MAGPLANT_5000, true)
        ->EmbedData("tilex", pTile->GetPos().x)
        ->EmbedData("tiley", pTile->GetPos().y)
        ->EmbedData("TileItemID", pTile->GetDisplayedItem())
        ->EmbedData("MagItemID", pData->itemID);

    if(isAdd) {
        db.AddTextBox("You have " + ToString(pPlayer->GetInventory().GetCountOfItem(pData->itemID)) + " `2" + string(pStock->name) + "`` in your backpack.")
          ->AddTextBox("How many `2" + string(pStock->name) + "`` would you like to add?")
          ->AddTextInput("AddAmount", "Amount:", ToString(pPlayer->GetInventory().GetCountOfItem(pData->itemID)), 3)
          ->EndDialog("Magplant", "Confirm", "Close");
    }
    else {
        const int32 defaultTake = std::max<int32>(0, std::min<int32>(200, pData->itemCount));
                db.AddTextBox("The " + machineName + " contains " + ToString(pData->itemCount) + " `2" + string(pStock->name) + "``")
          ->AddTextBox("How many `2" + string(pStock->name) + "`` would you like to take?")
          ->AddTextInput("TakeAmount", "Amount:", ToString(defaultTake), 3)
          ->EndDialog("Magplant", "Confirm", "Close");
    }

    pPlayer->SendOnDialogRequest(db.Get());
}

void ShowVendingDialog(GamePlayer* pPlayer, TileInfo* pTile, TileExtra_Vending* pData, bool ownerAccess)
{
    ItemInfo* pMachine = GetItemInfoManager()->GetItemByID(pTile->GetDisplayedItem());
    ItemInfo* pStock = GetItemInfoManager()->GetItemByID(pData->itemID);

    DialogBuilder db;
    db.SetDefaultColor('o')
        ->AddQuickExit()
        ->AddLabelWithIcon("`w" + string(pMachine ? pMachine->name : "Vending Machine") + "``", pMachine ? pMachine->id : ITEM_ID_VENDING_MACHINE, true)
        ->EmbedData("tilex", pTile->GetPos().x)
        ->EmbedData("tiley", pTile->GetPos().y)
        ->EmbedData("TileItemID", pTile->GetDisplayedItem());

    if(ownerAccess) {
        if(pData->stock < 1) {
                        db.AddTextBox("This machine is empty")
              ->AddItemPicker("SelectItem", "`wPut an item in``", "Choose an item to put in the machine!")
              ->EndDialog("VendEdit", "", "Close");
        }
        else {
                        if(pPlayer->GetInventory().GetCountOfItem(pData->itemID) > 0) {
                                db.AddTextBox("You have " + ToString(pPlayer->GetInventory().GetCountOfItem(pData->itemID)) + " " + string(pStock ? pStock->name : "item") + " in your inventory.")
                                    ->AddButton("PutStock", "Put stock into the machine");
                        }

                        db.AddLabelWithIcon("The machine contains a total of " + ToString(pData->stock) + " `2" + string(pStock ? pStock->name : "item") + "``.", pData->itemID, false)
                            ->AddSpacer();

                        if(pData->price == 0) {
                                db.AddTextBox("Not currently for sale!")
                                    ->AddSpacer()
                                    ->AddButton("TakeStock", "Empty the machine")
                                    ->AddTextBox("`5(Vending Machine will not function when price is set to 0)``")
                                    ->AddTextInput("SetPrice", "Price", "0", 9)
                                    ->AddCheckBox("PerItem", "World Locks per Item", true)
                                    ->AddCheckBox("PerWL", "Items per World Lock", false)
                                    ->EndDialog("VendEdit", "Update", "Close");

                                pPlayer->SendOnDialogRequest(db.Get());
                                return;
                        }

            db.AddTextBox("For a cost of:");

            if(pData->price > 0) {
                db.AddLabelWithIcon(ToString(pData->price) + " x `8World Locks``", ITEM_ID_WORLD_LOCK, false);
            }
            else {
                db.AddLabelWithIcon("1 x `8World Locks``", ITEM_ID_WORLD_LOCK, false);
            }

            db.AddSpacer();
            db.AddTextBox("You will get:");

            if(pStock) {
                if(pData->price > 0) {
                    db.AddLabelWithIcon("1 x `2" + pStock->name + "``", pStock->id, false);
                }
                else {
                    db.AddLabelWithIcon(ToString(std::abs(pData->price)) + " x `2" + pStock->name + "``", pStock->id, false);
                }
            }

            db.AddSpacer();
            db.AddButton("TakeStock", "Empty the machine")
                            ->AddTextBox("`5(Vending Machine will not function when price is set to 0)``")
                            ->AddTextInput("SetPrice", "Price", ToString(std::abs(pData->price)), 9)
              ->AddCheckBox("PerItem", "World Locks per Item", pData->price >= 0)
              ->AddCheckBox("PerWL", "Items per World Lock", pData->price < 0)
              ->EndDialog("VendEdit", "Update", "Close");
        }

                if(pTile->GetDisplayedItem() == ITEM_ID_VENDING_MACHINE) {
                        db.AddTextBox("Upgrade to a DigiVend Machine for `44,000 Gems``.")
                            ->AddButton("UpgradeDigi", "Upgrade to DigiVend");
                }

                if(pData->earnings > 0) {
                        db.AddTextBox("This machine has earned `2" + ToString(pData->earnings) + " World Locks`` so far.")
                            ->AddButton("CollectEarnings", "Collect Earnings");
                }
    }
    else {
        if(pData->price == 0 || pData->stock < 1 || (pData->price < 0 && pData->stock < std::abs(pData->price))) {
            db.AddTextBox("This machine is out of order.")
              ->EndDialog("VendEdit", "", "Close");
        } 
        else {
            db.AddTextBox("For a cost of:");
            if(pData->price > 0) {
                db.AddLabelWithIcon(ToString(pData->price) + " x `8World Locks``", ITEM_ID_WORLD_LOCK, false);
            }
            else {
                db.AddLabelWithIcon("1 x `8World Locks``", ITEM_ID_WORLD_LOCK, false);
            }
            db.AddSpacer()
              ->AddTextBox("You will get:");

            if(pStock) {
                if(pData->price > 0) {
                    db.AddLabelWithIcon("1 x `2" + pStock->name + "``", pStock->id, false);
                }
                else {
                    db.AddLabelWithIcon(ToString(std::abs(pData->price)) + " x `2" + pStock->name + "``", pStock->id, false);
                }
            }

                        db.AddSpacer()
                            ->AddTextBox("You have " + ToString(GetWorldLockValue(pPlayer)) + " World Locks.")
                            ->AddTextInput("BuyCount", "How many would you like to buy?", "0", 3)
                            ->EndDialog("VendEdit", "Buy", "Close");
        }
    }

    pPlayer->SendOnDialogRequest(db.Get());
}

void ShowMagplantDialog(GamePlayer* pPlayer, TileInfo* pTile, TileExtra_Magplant* pData, bool ownerAccess)
{
    ItemInfo* pMachine = GetItemInfoManager()->GetItemByID(pTile->GetDisplayedItem());
    ItemInfo* pStock = GetItemInfoManager()->GetItemByID(pData->itemID);

    DialogBuilder db;
    db.SetDefaultColor('o')
        ->AddQuickExit()
        ->AddLabelWithIcon("`w" + string(pMachine ? pMachine->name : "Magplant") + "``", pMachine ? pMachine->id : ITEM_ID_MAGPLANT_5000, true)
        ->EmbedData("tilex", pTile->GetPos().x)
        ->EmbedData("tiley", pTile->GetPos().y)
        ->EmbedData("TileItemID", pTile->GetDisplayedItem());

    if(ownerAccess) {
        if(pData->itemID != ITEM_ID_BLANK) {
            db.AddLabelWithIcon("`2" + string(pStock ? pStock->name : "Blank") + "``", pData->itemID, false);

            if(pData->itemCount > 0) {
                db.AddTextBox("`6The machine contains " + ToString(pData->itemCount) + " `2" + string(pStock ? pStock->name : "Blank") + "``");
            }
            else {
                db.AddTextBox("`6The machine is currently empty!``");
            }

            db.AddSpacer();

            if(pPlayer->GetInventory().GetCountOfItem(pData->itemID) > 0 && pData->itemCount < pData->itemLimit) {
                db.AddButton("AddItem", "Add Items to the machine");
            }

            if((200 - pPlayer->GetInventory().GetCountOfItem(pData->itemID)) > 0 && pData->itemCount > 0) {
                db.AddButton("TakeItem", "Retrieve Items");
            }

            if(pData->itemCount < 1) {
                db.AddItemPicker("SelectItem", "`wChange Item``", "Choose an item to put in the machine!");
            }

            if(pMachine && pMachine->id == ITEM_ID_MAGPLANT_5000) {
                db.AddTextBox("Building Mode: " + string(pData->remote ? "ACTIVE" : "`6DISABLED"));
                if(pData->remote) {
                    db.AddTextBox("Use the MAGPLANT 5000 Remote to build `2" + string(pStock ? pStock->name : "Blank") + "`` directly from the MAGPLANT 5000's storage.");
                }
                else {
                    db.AddTextBox("Punch to activate building mode.");
                }

                if(pPlayer->GetMagplantPos() != pTile->GetPos() && pData->remote) {
                    db.AddButton("GetRemote", "Get Remote");
                }
            }

            db.AddCheckBox("EnableSucking", "Enable Collection", pData->magnetic);

            db.EndDialog("Magplant", "Update", "Close");
        }
        else {
            db.AddTextBox("`6This machine is empty.");
            db.AddItemPicker("SelectItem", "`wChoose Item``", "Choose an item to put in the machine!");
            db.EndDialog("Magplant", "", "Close");
        }
    }
    else {
        if(pData->itemID != ITEM_ID_BLANK) {
            db.AddLabelWithIcon("`2" + string(pStock ? pStock->name : "Blank") + "``", pData->itemID, false);

            if(pData->itemCount > 0) {
                db.AddTextBox("`6The machine contains " + ToString(pData->itemCount) + " `2" + string(pStock ? pStock->name : "Blank") + "``");
            }
            else {
                db.AddTextBox("`6The machine is currently empty!``");
            }

            if(pMachine && pMachine->id == ITEM_ID_MAGPLANT_5000) {
                db.AddTextBox("Building Mode: " + string(pData->remote ? "ACTIVE" : "`6DISABLED"));

                if(pData->remote) {
                    db.AddTextBox("Use the MAGPLANT 5000 Remote to build `2" + string(pStock ? pStock->name : "Blank") + "`` directly from the MAGPLANT 5000's storage.");
                }

                if(pPlayer->GetMagplantPos() != pTile->GetPos() && pData->remote) {
                    db.AddButton("GetRemote", "Get Remote");
                }
            }

            db.EndDialog("Magplant", "", "Close");
        }
        else {
            db.AddTextBox("`6This machine is empty.");
            db.EndDialog("Magplant", "", "Close");
        }
    }

    pPlayer->SendOnDialogRequest(db.Get());
}

}

void MachineDialog::Request(GamePlayer* pPlayer, TileInfo* pTile)
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

    const bool ownerAccess = pWorld->PlayerHasAccessOnTile(pPlayer, pTile);

    if(pItem->type == ITEM_TYPE_VENDING) {
        TileExtra_Vending* pData = pTile->GetExtra<TileExtra_Vending>();
        if(!pData) {
            return;
        }

        ShowVendingDialog(pPlayer, pTile, pData, ownerAccess);
        return;
    }

    if(pTile->GetDisplayedItem() == ITEM_ID_MAGPLANT_5000 || pTile->GetDisplayedItem() == ITEM_ID_UNSTABLE_TESSERACT || pTile->GetDisplayedItem() == ITEM_ID_GAIAS_BEACON) {
        TileExtra_Magplant* pData = pTile->GetExtra<TileExtra_Magplant>();
        if(!pData) {
            return;
        }

        ShowMagplantDialog(pPlayer, pTile, pData, ownerAccess);
        return;
    }
}

void MachineDialog::Handle(GamePlayer* pPlayer, ParsedTextPacket<8>& packet)
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

    ItemInfo* pMachine = GetItemInfoManager()->GetItemByID(pTile->GetDisplayedItem());
    if(!pMachine) {
        return;
    }

    const bool ownerAccess = pWorld->PlayerHasAccessOnTile(pPlayer, pTile);

    auto pDialogName = packet.Find(CompileTimeHashString("dialog_name"));
    if(!pDialogName) {
        return;
    }

    string dialogName(pDialogName->value, pDialogName->size);

    int32 tileItemID = 0;
    if(ParseIntField(packet, CompileTimeHashString("TileItemID"), tileItemID)) {
        if(tileItemID <= 0 || tileItemID > UINT16_MAX || pTile->GetDisplayedItem() != (uint16)tileItemID) {
            return;
        }
    }

    if(dialogName == "VendEdit" && !ownerAccess) {
        TileExtra_Vending* pData = pTile->GetExtra<TileExtra_Vending>();
        if(!pData) {
            return;
        }

        auto pButtonClicked = packet.Find(CompileTimeHashString("buttonClicked"));
        string buttonClicked = pButtonClicked ? string(pButtonClicked->value, pButtonClicked->size) : "";
        if(buttonClicked != "Buy") {
            return;
        }

        if(pData->price == 0 || pData->itemID == ITEM_ID_BLANK || pData->stock < 1 || (pData->price < 0 && pData->stock < std::abs(pData->price))) {
            pPlayer->SendOnTalkBubble("This machine is out of order.", true);
            return;
        }

        int32 buyCount = 0;
        ParseIntField(packet, CompileTimeHashString("BuyCount"), buyCount);
        if(buyCount < 1) {
            return;
        }

        if(buyCount > 200) {
            buyCount = 200;
        }

        int32 totalItemsGive = 0;
        int32 totalPriceWLS = 0;

        if(pData->price > 0) {
            if(buyCount > pData->stock) {
                buyCount = pData->stock;
            }

            totalItemsGive = buyCount;
            totalPriceWLS = pData->price * buyCount;
        }
        else {
            if(buyCount > pData->stock) {
                buyCount = pData->stock;
            }

            totalItemsGive = (buyCount / std::abs(pData->price)) * std::abs(pData->price);
            totalPriceWLS = buyCount / std::abs(pData->price);

            if(buyCount < std::abs(pData->price)) {
                pPlayer->SendOnTalkBubble("You can't buy less than the item's price per World Lock!", true);
                return;
            }
        }

        const int32 totalWorldLocks = GetWorldLockValue(pPlayer);
        if(totalWorldLocks < totalPriceWLS) {
            pPlayer->SendOnTalkBubble("You can't afford that. You are `4" + ToString(totalPriceWLS - totalWorldLocks) + "`` world locks short.", true);
            return;
        }

        ShowVendingPurchaseDialog(pPlayer, pTile, pData, buyCount, totalItemsGive, totalPriceWLS);
        return;
    }

    if(dialogName == "VendConfirm") {
        TileExtra_Vending* pData = pTile->GetExtra<TileExtra_Vending>();
        if(!pData) {
            return;
        }

        auto pButtonClicked = packet.Find(CompileTimeHashString("buttonClicked"));
        string buttonClicked = pButtonClicked ? string(pButtonClicked->value, pButtonClicked->size) : "";
        if(buttonClicked != "Confirm") {
            return;
        }

        if(pData->price == 0) {
            pPlayer->SendOnTalkBubble("This vending machine is disabled.", true);
            return;
        }

        if(pData->stock <= 0 || pData->itemID == ITEM_ID_BLANK) {
            pPlayer->SendOnTalkBubble("The machine is already empty!", true);
            return;
        }

        int32 currentVendPrice = 0;
        int32 currentVendItemID = 0;
        ParseIntField(packet, CompileTimeHashString("CurrentVendPrice"), currentVendPrice);
        ParseIntField(packet, CompileTimeHashString("CurrentVendItemID"), currentVendItemID);

        if(currentVendPrice != pData->price || currentVendItemID != pData->itemID) {
            pPlayer->SendOnTalkBubble("This vending machine has been modified. Please try again.", true);
            return;
        }

        int32 buyCount = 0;
        ParseIntField(packet, CompileTimeHashString("BuyCount"), buyCount);
        if(buyCount < 1) {
            return;
        }

        if(buyCount > 200) {
            buyCount = 200;
        }

        int32 totalItemsGive = 0;
        int32 totalPriceWLS = 0;

        if(pData->price > 0) {
            if(buyCount > pData->stock) {
                buyCount = pData->stock;
            }

            totalItemsGive = buyCount;
            totalPriceWLS = pData->price * buyCount;
        }
        else {
            if(buyCount < std::abs(pData->price)) {
                pPlayer->SendOnTalkBubble("You can't buy less than the item's price per World Lock!", true);
                return;
            }

            if(buyCount > pData->stock) {
                buyCount = pData->stock;
            }

            totalItemsGive = (buyCount / std::abs(pData->price)) * std::abs(pData->price);
            totalPriceWLS = buyCount / std::abs(pData->price);
        }

        if(totalItemsGive < 1 || totalPriceWLS < 1) {
            pPlayer->SendOnTalkBubble("This vending machine is disabled.", true);
            return;
        }

        const int32 totalWorldLocks = GetWorldLockValue(pPlayer);
        if(totalWorldLocks < totalPriceWLS) {
            pPlayer->SendOnTalkBubble("You can't afford that. You are `4" + ToString(totalPriceWLS - totalWorldLocks) + "`` world locks short.", true);
            return;
        }

        if(!pPlayer->GetInventory().HaveRoomForItem(pData->itemID, (uint8)std::min<int32>(totalItemsGive, 200))) {
            pPlayer->SendOnTalkBubble("That won't fit into your inventory", true);
            return;
        }

        RemoveAllLockTypes(pPlayer);
        ReturnLockChange(pPlayer, totalWorldLocks - totalPriceWLS);

        const uint16 boughtItemID = pData->itemID;
        pPlayer->ModifyInventoryItem(boughtItemID, (int16)totalItemsGive);

        pData->stock -= totalItemsGive;
        pData->earnings += totalPriceWLS;

        if(pData->stock < 1) {
            pData->itemID = ITEM_ID_BLANK;
            pData->stock = 0;
        }

        ItemInfo* pStock = GetItemInfoManager()->GetItemByID(boughtItemID);
        pWorld->SendConsoleMessageToAll("`7[```9" + pPlayer->GetRawName() + " bought " + ToString(totalItemsGive) + " " + string(pStock ? pStock->name : "item") + " for " + ToString(totalPriceWLS) + " World Locks.```7]``");
        pWorld->PlaySFXForEveryone("cash_register.wav");
        pWorld->SendTileUpdate(pTile);
        return;
    }

    if(dialogName == "VendEdit" && ownerAccess) {
        TileExtra_Vending* pData = pTile->GetExtra<TileExtra_Vending>();
        if(!pData) {
            return;
        }

        int32 selectedItemInt = -1;
        if(!ParseIntField(packet, CompileTimeHashString("SelectItem"), selectedItemInt)) {
            ParseIntField(packet, CompileTimeHashString("SelectedItem"), selectedItemInt);
        }

        if(selectedItemInt >= 0) {
            if(pData->stock > 0) {
                return;
            }

            if(selectedItemInt >= 0 && selectedItemInt <= UINT16_MAX) {
                ItemInfo* pSelectedItem = GetItemInfoManager()->GetItemByID((uint16)selectedItemInt);
                if(!IsCompatibleVendItem(pSelectedItem)) {
                    pPlayer->SendOnTalkBubble("This item is not compatible with this machine.", true);
                    return;
                }

                pData->itemID = (uint16)selectedItemInt;
                pData->price = 0;
                pData->stock = 0;
                pData->earnings = 0;

                const uint8 owned = pPlayer->GetInventory().GetCountOfItem(pData->itemID);
                if(owned > 0) {
                    pData->stock = owned;
                    pPlayer->ModifyInventoryItem(pData->itemID, (int16)-owned);
                }

                pPlayer->SendOnTalkBubble("`7[``" + pPlayer->GetRawName() + "`2 places `2" + string(pSelectedItem->name) + "`` in the Vending Machine.`7]``", true);
                pWorld->SendConsoleMessageToAll("`7[``" + pPlayer->GetRawName() + "`2 places `2" + string(pSelectedItem->name) + "`` in the Vending Machine.`7]``");

                UpdateVendingMachineTile(pWorld, pTile, true);
                return;
            }
        }

        auto pButtonClicked = packet.Find(CompileTimeHashString("buttonClicked"));
        string buttonClicked = pButtonClicked ? string(pButtonClicked->value, pButtonClicked->size) : "";

        if(buttonClicked == "PutStock") {
            if(pData->itemID == ITEM_ID_BLANK) {
                return;
            }

            const uint8 owned = pPlayer->GetInventory().GetCountOfItem(pData->itemID);
            if(owned < 1) {
                return;
            }

            ItemInfo* pStockItem = GetItemInfoManager()->GetItemByID(pData->itemID);
            pData->stock += owned;
            pPlayer->ModifyInventoryItem(pData->itemID, (int16)-owned);
            pPlayer->SendOnTalkBubble("Added `9" + ToString(owned) + " " + string(pStockItem ? pStockItem->name : "item") + "`` into the machine.", true);
            pPlayer->SendOnConsoleMessage("Added `9" + ToString(owned) + " " + string(pStockItem ? pStockItem->name : "item") + "`` into the machine.");
            UpdateVendingMachineTile(pWorld, pTile, false);
            return;
        }

        if(buttonClicked == "TakeStock") {
            if(pData->itemID != ITEM_ID_BLANK && pData->stock > 0) {
                const int32 freeSpace = std::max<int32>(0, 200 - pPlayer->GetInventory().GetCountOfItem(pData->itemID));
                int32 takeAmount = std::min<int32>(pData->stock, freeSpace);

                if(takeAmount < 1 || !pPlayer->GetInventory().HaveRoomForItem(pData->itemID, (uint8)takeAmount)) {
                    pPlayer->SendOnTalkBubble("That won't fit in your backpack!", true);
                    return;
                }

                pPlayer->ModifyInventoryItem(pData->itemID, (int16)takeAmount);
                pData->stock -= takeAmount;

                ItemInfo* pStockItem = GetItemInfoManager()->GetItemByID(pData->itemID);
                pPlayer->SendOnTalkBubble("You picked up `9" + ToString(takeAmount) + " " + string(pStockItem ? pStockItem->name : "item") + "``.", true);
                pPlayer->SendOnConsoleMessage("You picked up `9" + ToString(takeAmount) + " " + string(pStockItem ? pStockItem->name : "item") + "``.");
            }

            if(pData->stock < 1) {
                pData->itemID = ITEM_ID_BLANK;
                pData->stock = 0;
            }

            UpdateVendingMachineTile(pWorld, pTile, false);
            return;
        }

        if(buttonClicked == "CollectEarnings") {
            if(pData->earnings <= 0) {
                return;
            }

            const int32 totalLocks = GetWorldLockValue(pPlayer);
            const int32 maxCollect = 2020200 - totalLocks;
            if(maxCollect <= 0) {
                pPlayer->SendOnTalkBubble("That won't fit in your backpack!", true);
                return;
            }

            int32 vendEarnings = pData->earnings;
            if(vendEarnings > maxCollect) {
                vendEarnings = maxCollect;
            }

            if(vendEarnings <= 0) {
                pPlayer->SendOnTalkBubble("That won't fit in your backpack!", true);
                return;
            }

            RemoveAllLockTypes(pPlayer);
            ReturnLockChange(pPlayer, totalLocks + vendEarnings);

            pData->earnings -= vendEarnings;
            if(pData->earnings < 0) {
                pData->earnings = 0;
            }

            pPlayer->SendOnTalkBubble("You collected `9" + ToString(vendEarnings) + " `wWorld Locks.", true);
            pPlayer->SendOnConsoleMessage("You collected `9" + ToString(vendEarnings) + " `wWorld Locks.");
            UpdateVendingMachineTile(pWorld, pTile, false);
            return;
        }

        if(buttonClicked == "UpgradeDigi") {
            return;
        }

        if(buttonClicked != "Update") {
            return;
        }

        int32 price = 0;

        if(!ParseIntField(packet, CompileTimeHashString("SetPrice"), price)) {
            ParseIntField(packet, CompileTimeHashString("VendPrice"), price);
        }

        if(price < 0) {
            price = std::abs(price);
        }

        bool perItem = false;
        bool perWL = false;
        const bool hasPerItem = ParseBoolField(packet, CompileTimeHashString("PerItem"), perItem);
        const bool hasPerWL = ParseBoolField(packet, CompileTimeHashString("PerWL"), perWL);

        if(!hasPerItem && !hasPerWL) {
            perItem = true;
            perWL = false;
        }

        ItemInfo* pStock = GetItemInfoManager()->GetItemByID(pData->itemID);
        if(!IsCompatibleVendItem(pStock)) {
            pPlayer->SendOnTalkBubble("This item is not compatible with this machine.", true);
            return;
        }

        const int32 clampedPriceAbs = std::min(200, price);
        bool disabledVend = false;
        bool changed = false;

        if(clampedPriceAbs < 1 || perItem == perWL) {
            if(pData->price != 0) {
                changed = true;
            }
            pData->price = 0;
            disabledVend = true;
        }
        else if(perWL) {
            if(pData->price != -clampedPriceAbs) {
                changed = true;
            }
            pData->price = -clampedPriceAbs;
        }
        else {
            if(pData->price != clampedPriceAbs) {
                changed = true;
            }
            pData->price = clampedPriceAbs;
        }

        if(pData->itemID == ITEM_ID_BLANK || pData->stock <= 0) {
            pData->price = 0;
            changed = true;
        }

        if(changed && pData->itemID != ITEM_ID_BLANK) {
            ItemInfo* pChangedItem = GetItemInfoManager()->GetItemByID(pData->itemID);
            const string changedName = pChangedItem ? pChangedItem->name : "item";

            if(pData->price == 0 || disabledVend) {
                pPlayer->SendOnTalkBubble("`7[``" + pPlayer->GetRawName() + "`2 disabled the vending machine.```7]``", true);
                pWorld->SendConsoleMessageToAll("`7[``" + pPlayer->GetRawName() + "`2 disabled the vending machine.```7]``");
            }
            else if(pData->price < 0) {
                pPlayer->SendOnTalkBubble("`7[``" + pPlayer->GetRawName() + "`2 changed the price of `2" + changedName + "`` to `6" + ToString(std::abs(pData->price)) + " per World Lock.```7]``", true);
                pWorld->SendConsoleMessageToAll("`7[``" + pPlayer->GetRawName() + "`2 changed the price of `2" + changedName + "`` to `6" + ToString(std::abs(pData->price)) + " per World Lock.```7]``");
            }
            else {
                pPlayer->SendOnTalkBubble("`7[``" + pPlayer->GetRawName() + "`2 changed the price of `2" + changedName + "`` to `5" + ToString(pData->price) + " World Locks each.```7]``", true);
                pWorld->SendConsoleMessageToAll("`7[``" + pPlayer->GetRawName() + "`2 changed the price of `2" + changedName + "`` to `5" + ToString(pData->price) + " World Locks each.```7]``");
            }
        }

        UpdateVendingMachineTile(pWorld, pTile, changed);
        return;
    }

    if(dialogName == "Magplant") {
        TileExtra_Magplant* pData = pTile->GetExtra<TileExtra_Magplant>();
        if(!pData) {
            return;
        }

        auto pButtonClicked = packet.Find(CompileTimeHashString("buttonClicked"));
        string buttonClicked = pButtonClicked ? string(pButtonClicked->value, pButtonClicked->size) : "";

        if(buttonClicked == "GetRemote") {
            if(pTile->GetDisplayedItem() == ITEM_ID_MAGPLANT_5000 && pData->remote && pPlayer->GetMagplantPos() != pTile->GetPos()) {
                const uint8 remoteCount = pPlayer->GetInventory().GetCountOfItem(ITEM_ID_MAGPLANT_5000_REMOTE);
                if(remoteCount > 0) {
                    pPlayer->ModifyInventoryItem(ITEM_ID_MAGPLANT_5000_REMOTE, (int16)-remoteCount);
                }

                pPlayer->SetMagplantPos(pTile->GetPos());

                pPlayer->ModifyInventoryItem(ITEM_ID_MAGPLANT_5000_REMOTE, 1);
                pPlayer->SendOnTalkBubble("`wYou received a `2Magplant 5000 `wRemote!", true);
            }

            pWorld->SendTileUpdate(pTile);
            return;
        }

        if(!ownerAccess) {
            return;
        }

        auto pMagnetic = packet.Find(CompileTimeHashString("EnableSucking"));
        if(pMagnetic) {
            int32 value = 0;
            if(ToInt(string(pMagnetic->value, pMagnetic->size), value) == TO_INT_SUCCESS) {
                pData->magnetic = value == 1;
            }
        }

        int32 selectedItemInt = -1;
        if(!ParseIntField(packet, CompileTimeHashString("SelectItem"), selectedItemInt)) {
            ParseIntField(packet, CompileTimeHashString("SelectedItem"), selectedItemInt);
        }

        if(selectedItemInt >= 0 && pData->itemCount < 1) {
            if(selectedItemInt >= 0 && selectedItemInt <= UINT16_MAX) {
                ItemInfo* pSelectedItem = GetItemInfoManager()->GetItemByID((uint16)selectedItemInt);
                if(!IsCompatibleMagplantItem(pTile->GetDisplayedItem(), pSelectedItem)) {
                    pPlayer->SendOnTalkBubble("This item is not compatible with this machine.", true);
                    return;
                }

                pData->itemID = (uint16)selectedItemInt;
                pData->itemCount = 0;
                EmitMachineUpdateEffects(pWorld, pTile);
                pWorld->SendTileUpdate(pTile);
                return;
            }
        }

        if(pTile->GetDisplayedItem() == ITEM_ID_MAGPLANT_5000) {
            pData->remote = true;
        }

        if(buttonClicked == "AddItem") {
            if(pData->itemID == ITEM_ID_BLANK) {
                return;
            }

            ShowMagplantAmountDialog(pPlayer, pTile, pData, true);
            return;
        }

        if(buttonClicked == "TakeItem") {
            if(pData->itemID == ITEM_ID_BLANK) {
                return;
            }

            ShowMagplantAmountDialog(pPlayer, pTile, pData, false);
            return;
        }

        int32 magItemID = 0;
        ParseIntField(packet, CompileTimeHashString("MagItemID"), magItemID);
        if(magItemID > 0 && magItemID <= UINT16_MAX && (uint16)magItemID != pData->itemID) {
            return;
        }

        int32 addAmount = 0;
        if(ParseIntField(packet, CompileTimeHashString("AddAmount"), addAmount)) {
            if(addAmount > 200) {
                addAmount = 200;
            }

            if(addAmount < 1) {
                return;
            }

            if(addAmount > 0 && pData->itemID != ITEM_ID_BLANK) {
                ItemInfo* pStock = GetItemInfoManager()->GetItemByID(pData->itemID);
                if(pStock) {
                    if(addAmount > pPlayer->GetInventory().GetCountOfItem(pData->itemID)) {
                        addAmount = pPlayer->GetInventory().GetCountOfItem(pData->itemID);
                    }

                    const int32 fit = std::max<int32>(0, pData->itemLimit - pData->itemCount);
                    if(fit < 1) {
                        pPlayer->SendOnTalkBubble("That won't fit into the `2" + string(pMachine ? pMachine->name : "machine") + "``!", true);
                        return;
                    }

                    if(addAmount > fit) {
                        pPlayer->SendOnTalkBubble("That won't fit into the `2" + string(pMachine ? pMachine->name : "machine") + "``!", true);
                        addAmount = fit;
                    }

                    if(addAmount > 0) {
                        pPlayer->ModifyInventoryItem(pData->itemID, (int16)-addAmount);
                        pData->itemCount += addAmount;
                        pPlayer->SendOnConsoleMessage("Added " + ToString(addAmount) + " " + string(pStock->name) + " to the " + string(pMachine ? pMachine->name : "machine"));
                        pPlayer->SendOnTalkBubble("Added " + ToString(addAmount) + " " + string(pStock->name) + " to the `2" + string(pMachine ? pMachine->name : "machine") + "``!", true);
                        EmitMachineUpdateEffects(pWorld, pTile);
                    }
                }
            }
        }

        int32 takeAmount = 0;
        if(ParseIntField(packet, CompileTimeHashString("TakeAmount"), takeAmount)) {
            if(takeAmount > 200) {
                takeAmount = 200;
            }

            if(takeAmount > pData->itemCount) {
                takeAmount = pData->itemCount;
            }

            if(takeAmount > 0 && pData->itemID != ITEM_ID_BLANK) {
                ItemInfo* pStock = GetItemInfoManager()->GetItemByID(pData->itemID);
                if(pStock) {
                    if(!pPlayer->GetInventory().HaveRoomForItem(pData->itemID, (uint8)takeAmount)) {
                        pPlayer->SendOnTalkBubble("That won't fit into your inventory.", true);
                        return;
                    }

                    pPlayer->ModifyInventoryItem(pData->itemID, (int16)takeAmount);
                    pData->itemCount -= takeAmount;
                    pPlayer->SendOnConsoleMessage("Took " + ToString(takeAmount) + " " + string(pStock->name) + " from the " + string(pMachine ? pMachine->name : "machine"));
                    pPlayer->SendOnTalkBubble("Took " + ToString(takeAmount) + " " + string(pStock->name) + " from the `2" + string(pMachine ? pMachine->name : "machine") + "``!", true);
                    EmitMachineUpdateEffects(pWorld, pTile);
                }
            }
        }

        if(pData->itemCount <= 0) {
            pData->itemCount = 0;
        }

        pWorld->SendTileUpdate(pTile);
    }
}
