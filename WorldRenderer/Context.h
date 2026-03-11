#pragma once

#include "ContextBase.h"
#include "Utils/GameConfig.h"

enum eFontType
{
    FONT_TYPE_CENTURY_GOTHIC_BOLD
};

class Context : public ContextBase {
public:
    Context();
    ~Context();

public:
    static Context* GetInstance() 
    {
        static Context instance;
        return &instance;
    }

public:
    void Init() override;
    void Kill() override;

public:
    GameConfig* GetGameConfig() { return m_pGameConfig; }
    void LoadPreResources();

private:
    GameConfig* m_pGameConfig;
};

Context* GetContext();