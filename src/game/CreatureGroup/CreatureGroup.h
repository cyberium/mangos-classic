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
#include "Formation.h"

class CreaturesGroupMgr
{
public:
    CreaturesGroupMgr() :
        m_groupEntryGuidCounter(CREATURE_GROUP_FIRST_DYNAMIC_GUID)     // reserve for static group
    {}

    void Initialize();
    CreaturesGroupEntrySPtr GetEntryByCreatureGuid(uint32 guid, uint32 map)
    {
        auto& result = m_staticGroupsData[map].find(guid);
        if (result != m_staticGroupsData[map].end())
            return result->second;
        return nullptr;
    }
    CreaturesGroupEntrySPtr GetEntryByGroupGuid(uint32 groupGuid)
    {
        auto& result = m_staticGroupGuids.find(groupGuid);
        if (result != m_staticGroupGuids.end())
            return result->second;
        return nullptr;
    }

    // set creature slot so we can know that its part of a group
    // group should exist in db with creature guid as group member
    void SetGroupSlot(Creature* creature);

    // dynamically add a creature group with provided creature as master
    CreaturesGroupDataSPtr AddDynamicGroup(Creature* creatureMaster);

    // dynamically add a member to an existing group
    CreaturesGroupDataSPtr AddGroupMember(Creature* creatureMaster, Creature* newMember);

    // add a formation to a group
    bool SetFormationGroup(Creature* creatureMaster, GroupFormationType type = GROUP_FORMATION_TYPE_SINGLE_FILE);

private:
    GroupTemplateEntryMap m_groupTemplateEntries;
    CreaturesGroupEntryMap m_staticGroupGuids;
    CreatureGroupStaticMap m_staticGroupsData;
    uint32 m_groupEntryGuidCounter;

    void LoadGroupTemplates();
    void LoadGroups();
};

// used to store data of loaded group.
class CreaturesGroupData : public std::enable_shared_from_this<CreaturesGroupData>
{
public:
    CreaturesGroupData(CreaturesGroupEntrySPtr& _groupEntry, FormationDataSPtr fData = nullptr) :
        gEntry(_groupEntry), guid(_groupEntry->guid), formationData(fData), masterSlot(nullptr), isDynamic(false) {}

    CreaturesGroupData(uint32 _guid) :
        guid(_guid), gEntry(nullptr), formationData(nullptr), masterSlot(nullptr), isDynamic(true) {}

    CreaturesGroupData() = delete;

    CreatureGroupSlotSPtr AddSlot(Creature* newMember, CreatureGroupSlotEntrySPtr slotEntry = nullptr);

    bool Update(uint32 diff);

    CreatureGroupSlotSPtr GetFirstFreeSlot(uint32 guid);
    CreatureGroupSlotSPtr GetFirstAliveSlot();
    CreatureGroupSlotSPtr GetSlotByGuid(uint32 guid);
    CreatureGroupSlotSPtr GetSlotBySlotId(uint32 slotId);

    void OnRespawn(Creature* creature);
    void OnDeath(Creature* creature);
    void OnEntityDelete(Unit* entity);

    uint32 guid;
    CreaturesGroupEntrySPtr gEntry;
    FormationDataSPtr formationData;
    CreatureGroupSlotSPtr masterSlot;
    CreatureGroupSlotMap creatureSlots;

    bool isDynamic;
};

// used to store dynamic data of individual slot
class CreatureGroupSlot
{
friend class CreaturesGroupData;
friend class CreaturesGroupMgr;
public:
    CreatureGroupSlot(CreaturesGroupDataSPtr& groupData, CreatureGroupSlotEntrySPtr& slotEntry, FormationSlotDataSPtr fSlotInfo = nullptr) :
        m_gData(groupData), m_currentGuid(slotEntry->defaultCreatureGuid), m_slotId(slotEntry->slotId),
        m_entity(nullptr), m_formationSlotInfo(fSlotInfo) {}

    CreatureGroupSlot(uint32 slotId, uint32 creatureGuid, CreaturesGroupDataSPtr& gData) :
        m_slotId(slotId), m_currentGuid(creatureGuid), m_gData(gData), m_entity(nullptr),
        m_formationSlotInfo(nullptr) {}

    void SetAsFormationSlot();
    uint32 GetCurrentGuid() const { return m_currentGuid; }
    uint32 GetSlotId() const { return m_slotId; }

    bool IsFormationMaster() const;

    Unit* GetMaster() { return m_gData->formationData ? m_gData->formationData->GetMaster() : nullptr; };

    CreaturesGroupDataSPtr GetGroupData() { return m_gData; };

    FormationDataSPtr GetFormationData() { return m_gData->formationData; }
    FormationSlotDataSPtr GetFormationSlotData() { return m_formationSlotInfo; }

    // important for MovGen
    float GetDistance() const { return m_formationSlotInfo->distance; }
    float GetAngle() const;
    Unit* GetEntity() { return m_entity; }
    void SetNewPositionRequired() { m_formationSlotInfo->recomputePosition = true; }
    bool NewPositionRequired();

private:
    CreaturesGroupDataSPtr m_gData;
    uint32 m_currentGuid;
    uint32 m_slotId;

    Unit* m_entity;

    FormationSlotDataSPtr m_formationSlotInfo;
};

#define sCreatureGroupMgr MaNGOS::Singleton<CreaturesGroupMgr>::Instance()
#endif
