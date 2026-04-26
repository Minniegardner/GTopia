#pragma once

#include "Precompiled.h"
#include "../../Server/TelnetServer.h"
#include "Utils/StringUtils.h"

struct TelnetCommandInfo
{
    string usage = "";
    string desc = "";
    int32 minAdminLevel = 999; // set it high
    std::vector<uint32> aliases;
};

template<typename T>
class TelnetCommandBase {
public:
    static const TelnetCommandInfo& GetInfo()
    {
        return T::GetInfo();
    }

    static void Execute(TelnetClient* pNetClient, std::vector<string>& args) 
    {
        if(!CheckPerm(pNetClient)) {
            return;
        }
        
        T::Execute(pNetClient, args);
    }

    static bool CheckPerm(TelnetClient* pNetClient)
    {
        if(!pNetClient || pNetClient->GetAdminLevel() < GetInfo().minAdminLevel) {
            pNetClient->SendMessage("Unknown command.", true);
            return false;
        }

        return true;
    }

    static void SendUsage(TelnetClient* pNetClient)
    {
        if(!pNetClient) {
            return;
        }

        pNetClient->SendMessage("Command usage: " + GetInfo().usage, false);
    }
};