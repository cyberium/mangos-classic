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

#ifndef CMANGOS_FORMATION_DEFS_H
#define CMANGOS_FORMATION_DEFS_H

#include "Common.h"
#include "CreatureGroup/CreatureGroupDefs.h"

struct FormationEntry;
struct FormationSlotEntry;
struct FormationSlot;
struct FormationSlotInfo;
class FormationMgr;
class Creature;
class FormationData;
class MovementGenerator;
class PathFinder;
class Map;

const uint32 MAX_GROUP_FORMATION_TYPE = 7;
enum GroupFormationType
{
    GROUP_FORMATION_TYPE_RANDOM = 0,
    GROUP_FORMATION_TYPE_SINGLE_FILE = 1,
    GROUP_FORMATION_TYPE_SIDE_BY_SIDE = 2,
    GROUP_FORMATION_TYPE_LIKE_GEESE = 3,
    GROUP_FORMATION_TYPE_FANNED_OUT_BEHIND = 4,
    GROUP_FORMATION_TYPE_FANNED_OUT_IN_FRONT = 5,
    GROUP_FORMATION_TYPE_CIRCLE_THE_LEADER = 6
};

typedef std::shared_ptr<FormationSlotEntry> FormationSlotEntrySPtr;
typedef std::map<uint32, FormationSlotEntrySPtr> FormationSlotEntryMap;
typedef std::shared_ptr<FormationSlotInfo> FormationSlotInfoSPtr;
typedef std::map<uint32, FormationSlotInfoSPtr> FormationSlotInfoMap;
typedef std::shared_ptr<FormationEntry> FormationEntrySPtr;
typedef std::map<uint32, FormationEntrySPtr> FormationEntryMap;
typedef std::shared_ptr<FormationData> FormationDataSPtr;
typedef std::map<uint32, FormationDataSPtr> FormationDataMap;
typedef std::shared_ptr<FormationSlot> FormationSlotSPtr;
typedef std::unordered_map<uint32, FormationSlotSPtr> FormationSlotMap;

struct FormationSlotEntry
{
    FormationSlotEntry(uint32 _slotId, float _angle, float _distance, FormationEntrySPtr& fEntry) :
        slotId(_slotId), angle(_angle), distance(_distance), formationEntry(fEntry) {}
    FormationSlotEntry() = delete;

    void operator=(FormationSlotEntry const& other)
    {
        slotId = other.slotId;
        angle = other.angle;
        distance = other.distance;
        formationEntry = other.formationEntry;
    }

    uint32 slotId;
    float angle;
    float distance;
    FormationEntrySPtr formationEntry;
};

struct FormationEntry
{
    uint32 formationId;
    GroupFormationType formationType;
    uint32 options;
    bool dynamic;
    float distance;
    CreaturesGroupEntrySPtr groupTableEntry;

    FormationSlotEntryMap slots;
};

struct FormationSlotInfo
{
    FormationSlotInfo() : defaultGuid(0), slotEntry(nullptr), groupsEntry(nullptr) {}
    FormationSlotInfo(uint32 _guid, FormationSlotEntrySPtr& _slot, CreaturesGroupEntrySPtr& _groups) :
        defaultGuid(_guid), slotEntry(_slot), groupsEntry(_groups) {}

    uint32 GetSlotId() const { return slotEntry->slotId; }
    uint32 GetGroupEntryId() const { return groupsEntry->groupTemplateEntry->id; }
    uint32 GetGroupGuid() const { return groupsEntry->guid; }
    uint32 GetFormationId() const { return slotEntry->formationEntry->formationId; }
    uint32 GetDefaultGuid() const { return defaultGuid; }
    FormationEntrySPtr GetFormationEntry() { return slotEntry->formationEntry; }

    CreaturesGroupEntrySPtr GetGroupTableEntry() { return groupsEntry; }
    void ChangeFormationEntry(FormationSlotEntrySPtr& fEntry) { slotEntry = fEntry; }

    float GetAngle() const { return slotEntry->angle; }
    float GetDistance() const { return slotEntry->distance; }

    uint32 defaultGuid;
    FormationSlotEntrySPtr slotEntry;
    CreaturesGroupEntrySPtr groupsEntry;
};

#endif