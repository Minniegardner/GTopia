#pragma once

#include "Precompiled.h"
#include "Utils/Timer.h"

// actually not sure need thread safe queue so need to decide concurrent or mutex
#include <concurrentqueue.h>

struct WorldRenderInfo
{
    uint64 reqTime = 0;
    uint32 worldID = 0;
    uint32 userID = 0;
};

class WorldRendererManager {
public:
    WorldRendererManager();
    ~WorldRendererManager();

public:
    static WorldRendererManager* GetInstance()
    {
        static WorldRendererManager instance;
        return &instance;
    }

public:
    void Update();
    void AddTask(uint32 userID, uint32 worldID);

private:
    moodycamel::ConcurrentQueue<WorldRenderInfo> m_renderQueue;
    Timer m_lastRenderTime;
};

WorldRendererManager* GetWorldRendererManager();