#pragma once

#include "../Precompiled.h" 
#include "Role.h"

class RoleManager {
public:
    RoleManager();
    ~RoleManager();

public:
    static RoleManager* GetInstance() {
        static RoleManager instance;
        return &instance;
    }

public:
    bool Load(const string& filePath);
    bool ResolveRole(Role* pRole);

    void Kill();

    Role* GetRole(int32 id);
    const std::unordered_map<int32, Role*>& GetRoles() const { return m_roles; }
    Role* FindRoleByName(const string& name) const;
    int32 GetDefaultRoleID() const { return m_defaultRoleID; }

private:
    bool GetRolePermFromString(const string& permStr, eRolePerm& permOut);

private:
    std::unordered_map<int32, Role*> m_roles;  
    std::unordered_map<string, eRolePerm> m_permList;
    int32 m_defaultRoleID;
};

RoleManager* GetRoleManager();