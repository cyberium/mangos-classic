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

#ifndef CMANGOS_CREATURE_GROUP_DEFS_H
#define CMANGOS_CREATURE_GROUP_DEFS_H

#include "Common.h"
#include "Formation/FormationDefs.h"

const uint32 CREATURE_GROUP_FIRST_DYNAMIC_GUID = 0x01FFFFFF;

// Needed forward declaration
struct CreraturesGroupTemplateEntry;
struct CreaturesGroupEntry;
struct CreatureGroupSlotEntry;
class CreatureGroupSlot;
class CreaturesGroupData;
class FormationMgr;
class Creature;
class FormationData;
struct FormationEntry;
typedef std::shared_ptr<FormationEntry> FormationEntrySPtr;
typedef std::shared_ptr<FormationData> FormationDataSPtr;

// Used to save data in group_template, key is group template entry
typedef std::shared_ptr<CreraturesGroupTemplateEntry> GroupTemplateEntrySPtr;
typedef std::map<uint32, GroupTemplateEntrySPtr> GroupTemplateEntryMap;

// Used to save group data, key is group guid
typedef std::shared_ptr<CreaturesGroupEntry> CreaturesGroupEntrySPtr;
typedef std::map<uint32, CreaturesGroupEntrySPtr> CreaturesGroupEntryMap;

// Used to save slot data, key is slot id
typedef std::shared_ptr<CreatureGroupSlotEntry> CreatureGroupSlotEntrySPtr;
typedef std::map<uint32, CreatureGroupSlotEntrySPtr> CreatureGroupStaticSlotMap;
typedef std::shared_ptr<CreatureGroupStaticSlotMap> CreatureGroupStaticSlotMapSPtr;

// Used to store all static creatures group related data for one map, key is creature guid
typedef std::map<uint32, CreaturesGroupEntrySPtr> CreatureGroupGuidMap;

// Used to summarize all static creatures group for all map. Key is a map id
typedef std::map<uint32, CreatureGroupGuidMap> CreatureGroupStaticMap;

// Used to store dynamic data of creatures group
typedef std::shared_ptr<CreatureGroupSlot> CreatureGroupSlotSPtr;
typedef std::map<uint32, CreatureGroupSlotSPtr> CreatureGroupSlotMap;
typedef std::shared_ptr<CreaturesGroupData> CreaturesGroupDataSPtr;
typedef std::map<uint32, CreaturesGroupDataSPtr> CreaturesGroupMap;

// store static slot info for creatures in group
struct CreatureGroupSlotEntry
{
    CreatureGroupSlotEntry(uint32 _slotId, uint32 _creatureGuid, CreaturesGroupEntrySPtr& cEntry) :
        slotId(_slotId), defaultCreatureGuid(_creatureGuid), creatureGroupEntry(cEntry) {}
    CreatureGroupSlotEntry() = delete;

    uint32 slotId;
    uint32 defaultCreatureGuid;
    CreaturesGroupEntrySPtr creatureGroupEntry;
};

// used to store group template entry form table group_template
struct CreraturesGroupTemplateEntry
{
    CreraturesGroupTemplateEntry(uint32 gId, std::string const& gName) :
        groupName(gName), id(gId) {}
    CreraturesGroupTemplateEntry() = delete;

    std::string groupName;
    uint32 id;
};

// used to store all static data related to an unique group
struct CreaturesGroupEntry
{
    CreaturesGroupEntry(uint32 _guid, GroupTemplateEntrySPtr& _groupTemplateEntry, FormationEntrySPtr fEntry = nullptr) :
        groupTemplateEntry(_groupTemplateEntry), guid(_guid), masterSlot(nullptr), formationEntry(fEntry) {}
    CreaturesGroupEntry() = delete;

    CreatureGroupSlotEntrySPtr GetSlotEntryByGuid(uint32 guid)
    {
        auto& result = creatureSlots.find(guid);
        if (result != creatureSlots.end())
            return result->second;

        return nullptr;
    }

    uint32 guid;
    CreatureGroupSlotEntrySPtr masterSlot;
    CreatureGroupStaticSlotMap creatureSlots;
    GroupTemplateEntrySPtr groupTemplateEntry;
    FormationEntrySPtr formationEntry;
};

#endif