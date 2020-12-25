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

// Needed forward declaration
struct FormationEntry;
struct CreraturesGroupTemplateEntry;
struct CreaturesGroupEntry;
struct CreatureGroupSlotEntry;
class FormationMgr;
class Creature;
class FormationData;
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
typedef std::map<uint32, CreatureGroupSlotEntrySPtr> CreatureGroupSlotMap;
typedef std::shared_ptr<CreatureGroupSlotMap> CreatureGroupSlotMapSPtr;

// Used to store all creatures group related data for one map, key is creature guid
typedef std::map<uint32, CreaturesGroupEntrySPtr> CreatureGroupGuidMap;

// Used to summarize all creatures group for all map. Key is a map id
typedef std::map<uint32, CreatureGroupGuidMap> CreatureGroupMap;

struct CreatureGroupSlotEntry
{
    CreatureGroupSlotEntry(uint32 _slotId, uint32 _creatureGuid, CreaturesGroupEntrySPtr& cEntry) :
        slotId(_slotId), defaultCreatureGuid(_creatureGuid), creatureGroupEntry(cEntry) {}
    CreatureGroupSlotEntry() = delete;

    uint32 slotId;
    uint32 defaultCreatureGuid;
    CreaturesGroupEntrySPtr creatureGroupEntry;
};

struct CreraturesGroupTemplateEntry
{
    CreraturesGroupTemplateEntry(uint32 gId, std::string const& gName) :
        groupName(gName), id(gId) {}
    CreraturesGroupTemplateEntry() = delete;

    std::string groupName;
    uint32 id;
};

struct CreaturesGroupEntry
{
    CreaturesGroupEntry(uint32 _guid, GroupTemplateEntrySPtr& _groupTemplateEntry, CreatureGroupSlotMapSPtr _slot, FormationEntrySPtr fEntry = nullptr) :
        groupTemplateEntry(_groupTemplateEntry), guid(_guid), masterSlot(nullptr), creatureSlot(_slot), formationEntry(fEntry) {}
    CreaturesGroupEntry() = delete;

    CreatureGroupSlotEntrySPtr GetSlotByCreatureGuid(uint32 guid)
    {
        auto& result = creatureSlot->find(guid);
        if (result != creatureSlot->end())
            return result->second;

        return nullptr;
    }

    uint32 guid;
    CreatureGroupSlotEntrySPtr masterSlot;
    CreatureGroupSlotMapSPtr creatureSlot;
    GroupTemplateEntrySPtr groupTemplateEntry;
    FormationEntrySPtr formationEntry;
};

#endif