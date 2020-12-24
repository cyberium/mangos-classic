/*
 * This file is part of the CMaNGOS Project. See AUTHORS file for Copyright information
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ifndef CMANGOS_CREATURE_GROUP_H
#define CMANGOS_CREATURE_GROUP_H

#include "Common.h"
#include "CreatureGroupDefs.h"

class CreaturesGroupMgr
{
public:
    CreaturesGroupMgr() {}

    void Initialize();
    CreaturesGroupEntrySPtr GetEntryByCreatureGuid(uint32 guid, uint32 map)
    {
        auto& result = m_groupsData[map].find(guid);
        if (result != m_groupsData[map].end())
            return result->second;
        return nullptr;
    }
    CreaturesGroupEntrySPtr GetEntryByGroupGuid(uint32 groupGuid)
    {
        auto& result = m_groupGuids.find(groupGuid);
        if (result != m_groupGuids.end())
            return result->second;
        return nullptr;
    }

private:
    GroupTemplateEntryMap m_groupTemplateEntries;
    CreaturesGroupEntryMap m_groupGuids;
    CreatureGroupMap m_groupsData;

    void LoadGroupTemplates();
    void LoadGroups();
};

#define sCreatureGroupMgr MaNGOS::Singleton<CreaturesGroupMgr>::Instance()
#endif
