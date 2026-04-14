#include "LockAlgorithm.h"

bool IsWorldLock(uint16 itemID)
{
    switch(itemID) {
        case ITEM_ID_WORLD_LOCK:
        case ITEM_ID_DIAMOND_LOCK:
        case ITEM_ID_HARMONIC_LOCK:
        case ITEM_ID_ROBOTIC_LOCK:
        case ITEM_ID_BLUE_GEM_LOCK:
            return true;

        default:
            return false;
    }
}

bool IsAreaLock(uint16 itemID)
{
    switch(itemID) {
        case ITEM_ID_SMALL_LOCK:
        case ITEM_ID_BIG_LOCK:
        case ITEM_ID_HUGE_LOCK:
        case ITEM_ID_BUILDERS_LOCK:
            return true;

        default:
            return false;
    }
}

uint16 GetMaxTilesToLock(uint16 itemID)
{
    switch(itemID) {
        case ITEM_ID_SMALL_LOCK:
            return 10;

        case ITEM_ID_BIG_LOCK:
            return 48;

        case ITEM_ID_HUGE_LOCK:
        case ITEM_ID_BUILDERS_LOCK:
            return 200;

        default:
            return 0;
    }
}