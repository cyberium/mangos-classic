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

#include "CreatureGroup.h"
#include "Entities/Unit.h"
#include "Database/Database.h"
#include "Policies/Singleton.h"
#include "Entities/Creature.h"

INSTANTIATE_SINGLETON_1(CreaturesGroupMgr);

void CreaturesGroupMgr::Initialize()
{
    LoadGroupTemplates();
    LoadGroups();
}

CreaturesGroupEntrySPtr CreaturesGroupMgr::AddDynamicGroup(Creature* creatureMaster)
{
    // first check if creature is valid and in a map
    if (!creatureMaster || !creatureMaster->GetMap())
        return nullptr;

    // base data
    uint32 masterGuid = creatureMaster->GetGUIDLow();
    uint32 currentMap = creatureMaster->GetMapId();
    uint32 slotId = 0;
    uint32 newGroupGuid = m_groupEntryGuidCounter++;

    // check if group doesn't already exist
    auto& result = creatureMaster->GetMap()->GetGroupData(masterGuid);
    if (result)
    {
        sLog.outError("CreaturesGroupMgr::AddDynGroupEntry> Failed to create a group for %s, group already exist!", creatureMaster->GetGuidStr().c_str());
        return result;
    }

    // search for special entry 0 for all dynamics groups
    auto gEntryItr = m_groupTemplateEntries.find(0);
    if (gEntryItr == m_groupTemplateEntries.end())
    {
        // create special template for dynamic groups
        auto templItr = m_groupTemplateEntries.emplace(0, new CreraturesGroupTemplateEntry(0, "Dynamic group"));
        gEntryItr = templItr.first;
    }

    CreaturesGroupEntrySPtr gEntry = CreaturesGroupEntrySPtr(new CreaturesGroupEntry(newGroupGuid, gEntryItr->second, nullptr));

    // slots creation
    CreatureGroupSlotMapSPtr slotMap = CreatureGroupSlotMapSPtr(new CreatureGroupSlotMap());
    auto slotEmplItr = slotMap->emplace(masterGuid, new CreatureGroupSlotEntry(slotId, masterGuid, gEntry));

    // set it as master slot
    gEntry->masterSlot = slotEmplItr.first->second;

    // add it to map data store
    creatureMaster->GetMap()->AddGroupData(gEntry, masterGuid);

    // add it to guid lookup table
    m_groupGuids.emplace(newGroupGuid, gEntry);
    return gEntry;
}

void CreaturesGroupMgr::LoadGroupTemplates()
{
    sLog.outString("Loading group_template...");
    uint32 count = 0;

    // fields indexes                             0       1
    const char* sqlGTRequest = "SELECT `group_entry`, `name` from `group_template`";
    std::unique_ptr<QueryResult> groupTemplateQR(WorldDatabase.Query(sqlGTRequest));
    if (groupTemplateQR)
    {
        do
        {
            Field* fields = groupTemplateQR->Fetch();

            uint32 groupId = fields[0].GetUInt32();
            std::string gName = fields[1].GetString();

            m_groupTemplateEntries.emplace(groupId, new CreraturesGroupTemplateEntry(groupId, gName));
            count++;
        } while (groupTemplateQR->NextRow());
    }

    sLog.outString(">> Loaded %u group template", count);
    sLog.outString();
}

void CreaturesGroupMgr::LoadGroups()
{
    sLog.outString("Loading groups...");

    // fields indexes                            0              1          2
    const char* sqlGMRequest = "SELECT `group_guid`, `member_guid`, `slot_id` FROM `group_member` ORDER BY `group_guid`, `slot_id`";

    uint32 count = 0;
    std::unique_ptr<QueryResult> groupMemberQR(WorldDatabase.Query(sqlGMRequest));

    std::multimap<uint32, std::tuple<uint32, uint32>> fContainer;
    std::map<uint32, std::set<uint32>> unique_membersGuids;
    std::set <std::pair<uint32, uint32>> uniqueSlotIdPerGroup;

    if (groupMemberQR)
    {
        do
        {
            Field* fields = groupMemberQR->Fetch();

            uint32 groupGuid = fields[0].GetUInt32();
            uint32 memberGuid = fields[1].GetUInt32();
            uint32 slotId = fields[2].GetUInt32();
            auto sData = std::make_pair(groupGuid, slotId);

            auto poolId = sPoolMgr.IsPartOfAPool<Creature>(memberGuid);
            if (poolId)
            {
                sLog.outErrorDb("Creature guid(%u) have valid PoolId(%u) that will not work with formation. Disabling pool...", memberGuid, poolId);
                sPoolMgr.RemoveAutoSpawnForPool(poolId);
                sPoolMgr.DespawnPoolInMaps(poolId);
                sPoolMgr.RemoveFromPool<Creature>(memberGuid);
                CreatureData const* cData = sObjectMgr.GetCreatureData(memberGuid);
                sObjectMgr.AddCreatureToGrid(memberGuid, cData);
            }

            auto cData = sObjectMgr.GetCreatureData(memberGuid);
            if (!cData)
            {
                sLog.outErrorDb("Creature guid(%u), member of goup_guid(%u) have no data in creatures table, skipping...", memberGuid, groupGuid);
                continue;
            }

            auto linkInfo = sCreatureLinkingMgr.GetLinkedTriggerInformation(cData->id, memberGuid, cData->mapid);
            if (linkInfo)
            {
                sLog.outErrorDb("Creature guid(%u) have its entry(%u) in linked creature table that will not work with formation. Disabling Linking...", memberGuid, cData->id);
                sCreatureLinkingMgr.DeleteEntry(cData->id, cData->mapid);
            }

            linkInfo = sCreatureLinkingMgr.GetLinkedTriggerInformation(0, memberGuid, 0);
            if (linkInfo)
            {
                sLog.outErrorDb("Creature guid(%u) have its entry(%u) in linked creature table that will not work with formation. Disabling Linking...", memberGuid, cData->id);
                sCreatureLinkingMgr.DeleteGuid(memberGuid);
            }

            auto itr = uniqueSlotIdPerGroup.find(sData);
            if (itr != uniqueSlotIdPerGroup.end())
            {
                sLog.outErrorDb("group_member slot(%u) is already assigned skipping...", slotId);
                continue;
            }

            fContainer.emplace(groupGuid, std::make_tuple(memberGuid, slotId));
            uniqueSlotIdPerGroup.emplace(sData);

            ++count;
        } while (groupMemberQR->NextRow());
    }

    // fields indexes                             0              1
    const char* sqlGrpRequest = "SELECT `group_guid`, `group_entry` from `group_guid`";

    count = 0;
    std::unique_ptr<QueryResult> groupQR(WorldDatabase.Query(sqlGrpRequest));

    if (groupQR)
    {
        do
        {
            Field* fields = groupQR->Fetch();

            uint32 groupGuid = fields[0].GetUInt32();
            uint32 groupEntryId = fields[1].GetUInt32();

            auto& groupTemplateEntryItr = m_groupTemplateEntries.find(groupEntryId);
            if (groupTemplateEntryItr == m_groupTemplateEntries.end())
            {
                sLog.outErrorDb("Template(%u) referenced in table groups(%u) is not found in group_template table. Skipping...", groupEntryId, groupGuid);
                continue;
            }

            auto grpGuidItr = m_groupGuids.find(groupGuid);
            if (grpGuidItr == m_groupGuids.end())
            {
                auto groupTemplateItr = m_groupTemplateEntries.find(groupEntryId);
                if (groupTemplateItr == m_groupTemplateEntries.end())
                {
                    sLog.outErrorDb("Group entry(%u) in `group_formation` is not found in `group_guid` table", groupEntryId);
                    continue;
                }
                auto& groupEntry = groupTemplateItr->second;

                auto bounds = fContainer.equal_range(groupGuid);
                if (std::distance(bounds.first, bounds.second) == 0)
                {
                    sLog.outErrorDb("There is no member defined for formation[GroupGuid(%u)] in group_member table", groupGuid);
                    continue;
                }

                // enforce master slot presence and check map consistence
                bool foundMasterSlot = false;
                int32 currentMap = -1;
                for (auto itr = bounds.first; itr != bounds.second; ++itr)
                {
                    auto& slotId = std::get<1>(itr->second);
                    auto& memberGuid = std::get<0>(itr->second);
                    auto cData = sObjectMgr.GetCreatureData(memberGuid);

                    if (slotId == 0)
                        foundMasterSlot = true;
                    if (currentMap < 0)
                        currentMap = cData->mapid;
                    else
                    {
                        if (currentMap != cData->mapid)
                        {
                            currentMap = -1;
                            break;
                        }
                    }
                }

                if (!foundMasterSlot)
                {
                    sLog.outErrorDb("Error in table group_member. MasterSlot(0) is not defined for group guid(%u) skipping...", groupGuid);
                    continue;
                }

                if (currentMap < 0)
                {
                    sLog.outErrorDb("Error in table group_member. one or more member of group guid(%u) are not in same map skipping...", groupGuid);
                    continue;
                }

                CreaturesGroupEntrySPtr gEntry = CreaturesGroupEntrySPtr(new CreaturesGroupEntry(groupGuid, groupEntry, nullptr));

                // slots creation
                CreatureGroupSlotMap& slotMap = gEntry->creatureSlot;
                for (auto itr = bounds.first; itr != bounds.second; ++itr)
                {
                    auto& memberGuid = std::get<0>(itr->second);
                    auto& slotId = std::get<1>(itr->second);

                    slotMap.emplace(memberGuid, new CreatureGroupSlotEntry(slotId, memberGuid, gEntry));

                    if (slotId == 0)
                        gEntry->masterSlot = slotMap[memberGuid];

                    m_groupsData[currentMap].emplace(memberGuid, gEntry);
                }

                m_groupGuids.emplace(groupGuid, gEntry);
                ++count;
            }

        } while (groupQR->NextRow());
    }

    sLog.outString(">> Loaded %u group members", count);
    sLog.outString();
}