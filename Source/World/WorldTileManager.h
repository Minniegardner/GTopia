#pragma once

#include "../Precompiled.h"
#include "TileInfo.h"
#include "../Memory/MemoryBuffer.h"
#include "../Math/Rect.h"

#define WORLD_DEFAULT_WIDTH 100
#define WORLD_DEFAULT_HEIGHT 60

struct TileMapFillData
{
    uint32 itemID = 0;
    float chance = 100.0f;
};

typedef std::vector<TileMapFillData> TileMapFillVector;

class WorldTileManager {
public:
    WorldTileManager();

public:
    bool Serialize(MemoryBuffer& memBuffer, bool write, bool database);

    void Clear(bool reInit = false);

    Vector2Int GetSize() const { return m_size; }
    void SetSize(const Vector2Int& size) { m_size = size; }

    TileInfo* GetTile(int32 x, int32 y);

    void GenerateDefaultMap();
    void GenerateClearMap();
    void GenerateBeachMap();

    void FillRectWith(const RectInt& rect, uint16 fgItem, uint16 bgItem, float chance);
    bool FillRectWith(const RectInt& rect, const TileMapFillVector& fgItems, const TileMapFillVector& bgItems);

    void SetMainDoorTile(TileInfo* pTile) { m_pMainDoorTile = pTile; }
    TileInfo* GetMainDoorTile() { return m_pMainDoorTile; }

    void SetGuardPineappleTile(TileInfo* pTile) { m_pGuardPineappleTile = pTile; }
    TileInfo* GetGuardPineappleTile() { return m_pGuardPineappleTile; }

    bool IsSameTile(TileInfo* pTile, int32 x, int32 y, bool forBackground);

private:
    void FillRectWithThickness(uint16 thickness, RectInt& rect, uint16 fgItem, uint16 bgItem, float chance);
    void FillRectWithThickness(uint16 thickness, RectInt& rect, const TileMapFillVector& fgItems, const TileMapFillVector& bgItems);
    
private:
    Vector2Int m_size;
    std::vector<TileInfo> m_tiles;

    TileInfo* m_pMainDoorTile;
    TileInfo* m_pGuardPineappleTile;
};