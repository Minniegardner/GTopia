#include "TileExtra.h"
#include "../Item/ItemUtils.h"
#include "TileInfo.h"
#include "../IO/Log.h"

TileExtra::TileExtra()
: m_type(0)
{
}

bool TileExtra::HasExtra(uint8 itemType)
{
    switch(itemType) {
        case ITEM_TYPE_USER_DOOR: case ITEM_TYPE_DOOR:
        case ITEM_TYPE_PORTAL: case ITEM_TYPE_SUNGATE:
        case ITEM_TYPE_SIGN:
            return true;

        default:
            return false;
    }
}

bool TileExtra::Setup(uint8 itemType)
{
    switch(itemType) {
        case ITEM_TYPE_USER_DOOR: case ITEM_TYPE_DOOR:
        case ITEM_TYPE_PORTAL: case ITEM_TYPE_SUNGATE: {
            m_type = TILE_EXTRA_TYPE_DOOR;
            break;
        }

        case ITEM_TYPE_SIGN: {
            m_type = TILE_EXTRA_TYPE_SIGN;
            break;
        }

        default:
            return false;
    }

    return true;
}

bool TileExtra::Serialize(MemoryBuffer& memBuffer, bool write, bool database, TileInfo* pTile)
{
    if(!pTile || m_type == TILE_EXTRA_TYPE_NONE) {
        return false;
    }

    memBuffer.ReadWrite(m_type, write);

    switch(m_type) {
        case TILE_EXTRA_TYPE_DOOR: {
            memBuffer.ReadWriteString(m_name, write);

            if(database) {
                memBuffer.ReadWriteString(m_text, write);
                memBuffer.ReadWriteString(m_id, write);
            }

            uint8 unk = 0;
            memBuffer.ReadWrite(unk, write);
            break;
        }

        case TILE_EXTRA_TYPE_SIGN: {
            memBuffer.ReadWriteString(m_text, write);

            int32 unk = -1; // something with owner union but eh
            memBuffer.ReadWrite(unk, write);
            break;
        }
    }

    return true;
}
