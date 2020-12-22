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
#include "Common.h"

#ifndef CMANGOS_FORMATION_H
#define CMANGOS_FORMATION_H

struct FormationEntry;
struct FormationSlotEntry;
struct SlotData;
struct CreraturesGroupTemplateEntry;
struct FormationSlotInfo;
struct CreaturesGroupEntry;
class FormationMgr;
class Creature;
class FormationData;
class MovementGenerator;
class PathFinder;
class Map;


const uint32 MAX_GROUP_FORMATION_TYPE = 7;
enum GroupFormationType
{
    GROUP_FORMATION_TYPE_RANDOM              = 0,
    GROUP_FORMATION_TYPE_SINGLE_FILE         = 1,
    GROUP_FORMATION_TYPE_SIDE_BY_SIDE        = 2,
    GROUP_FORMATION_TYPE_LIKE_GEESE          = 3,
    GROUP_FORMATION_TYPE_FANNED_OUT_BEHIND   = 4,
    GROUP_FORMATION_TYPE_FANNED_OUT_IN_FRONT = 5,
    GROUP_FORMATION_TYPE_CIRCLE_THE_LEADER   = 6
};

namespace G3D
{
class Vector3;
class PointsArray;
}

typedef std::shared_ptr<FormationSlotEntry> FormationSlotEntrySPtr;
typedef std::map<uint32, FormationSlotEntrySPtr> FormationSlotEntryMap;
typedef std::shared_ptr<CreraturesGroupTemplateEntry> GroupTemplateEntrySPtr;
typedef std::map<uint32, GroupTemplateEntrySPtr> GroupTemplateEntryMap;
typedef std::shared_ptr<FormationSlotInfo> FormationSlotInfoSPtr;
typedef std::map<uint32, FormationSlotInfoSPtr> FormationSlotInfoMap;
typedef std::shared_ptr<FormationEntry> FormationEntrySPtr;
typedef std::map<uint32, FormationEntrySPtr> FormationEntryMap;
typedef std::shared_ptr<FormationData> FormationDataSPtr;
typedef std::map<uint32, FormationDataSPtr> FormationDataMap;
typedef std::shared_ptr<SlotData> SlotDataSPtr;
typedef std::shared_ptr<CreaturesGroupEntry> CreaturesGroupEntrySPtr;
typedef std::map<uint32, CreaturesGroupEntrySPtr> CreaturesGroupEntryMap;
typedef std::map<uint32, uint32> GroupGuidMap;

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
    float distance;
    CreaturesGroupEntrySPtr groupTableEntry;

    FormationSlotEntryMap slots;
};

struct CreraturesGroupTemplateEntry
{
    CreraturesGroupTemplateEntry(uint32 gId, std::string const& gName, FormationEntrySPtr fEntry) :
        groupName(gName), formationEntry(fEntry), id(gId) {}
   //GroupTemplateEntry() : formationEntry(nullptr), id(0) {}

    std::string groupName;
    uint32 id;
    FormationEntrySPtr formationEntry;
};

struct CreaturesGroupEntry
{
    CreaturesGroupEntry(uint32 _guid, GroupTemplateEntrySPtr& _groupTemplateEntry) :
        groupTemplateEntry(_groupTemplateEntry), guid(_guid) {}
    CreaturesGroupEntry() = delete;

    uint32 guid;
    GroupTemplateEntrySPtr groupTemplateEntry;
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

class FormationMgr
{
public:
    FormationMgr() {}

    void Initialize();

    template<typename T> void SetFormationSlot(T* obj, Map* map) {};
    FormationSlotInfo const* GetFormationSlotInfo(uint32 guid);
    FormationEntrySPtr GetFormationEntry(uint32 groupId);

    void Update(FormationDataMap& fDataMap);

private:
    void LoadGroupTemplate();
    void LoadGroupGuids();
    void LoadGroupMembers();
    void oldloader();

    FormationEntryMap m_formationEntries;
    GroupTemplateEntryMap m_groupTemplateEntries;
    CreaturesGroupEntryMap m_groupsData;
    FormationSlotInfoMap m_slotInfos;
    GroupGuidMap m_groupGuids;
};
template<> void FormationMgr::SetFormationSlot<Creature>(Creature* creature, Map* map);

enum class MasterMotionType
{
    FORMATION_TYPE_MASTER_RANDOM,
    FORMATION_TYPE_MASTER_WAYPOINT
};

class FormationData
{
private:
    typedef std::map<uint32, SlotDataSPtr> SlotsMap;

public:
    FormationData(CreaturesGroupEntrySPtr groupTableEntry) :
        m_groupTableEntry(groupTableEntry), m_currentFormationShape(groupTableEntry->groupTemplateEntry->formationEntry->formationType),
        m_formationEnabled(true), m_realMaster(nullptr), m_mirrorState(false),
        m_masterMotionType(MasterMotionType::FORMATION_TYPE_MASTER_RANDOM), m_masterCheck(0),
        m_lastWP(0), m_wpPathId(0)
    {}
    FormationData() = delete;

    void SetFollowersMaster();
    bool SwitchFormation(uint32 fId);
    void Disband();

    void SetMirrorState(bool state) { m_mirrorState = state; };
    bool GetMirrorState() const { return m_mirrorState; }
    void FillSlot(FormationSlotInfoSPtr& slot, Creature* creature);
    Creature* GetMaster();
    void Update(uint32 diff);
    void Reset();

    void OnMasterRemoved() { m_formationEnabled = false; }
    void OnRespawn(Creature* creature);
    void OnDeath(Creature* creature);
    void OnCreatureDelete(Creature* creature);

    void SetNewSlot(Creature* creature, SlotDataSPtr& slot);

    SlotsMap const& GetSlots() const { return m_slotMap; }
    uint32 GetGroupGuid() const { return m_groupTableEntry->guid; }
    uint32 GetGroupEntryId() const { return m_groupTableEntry->groupTemplateEntry->id; }
    uint32 GetFormationId() const { return m_groupTableEntry->groupTemplateEntry->formationEntry->formationId; }
    GroupFormationType GetFormationType() const { return m_currentFormationShape; }
    CreaturesGroupEntrySPtr GetGroupTableEntry() { return m_groupTableEntry; }

    void FixSlotsPositions();

private:
    SlotDataSPtr GetFirstAliveSlot();
    SlotDataSPtr GetFirstFreeSlot(uint32 guid);
    void SetMasterMovement(Creature* master);
    void TrySetNewMaster(Creature* masterCandidat = nullptr);
    CreaturesGroupEntrySPtr m_groupTableEntry;
    GroupFormationType m_currentFormationShape;
    SlotsMap m_slotMap;
    bool m_formationEnabled;
    bool m_mirrorState;
    bool m_needToFixPositions;

    uint32 m_lastWP;
    uint32 m_wpPathId;

    Creature* m_realMaster;

    MasterMotionType m_masterMotionType;
    ShortTimeTracker m_masterCheck;
};

struct SlotData
{
    friend class FormationData;

public:
    SlotData(FormationSlotInfoSPtr slot, Creature* _creature, FormationData* fData);
    SlotData() = delete;

    ~SlotData();

    uint32 GetFormationId() const { return m_formationId; }
    CreaturesGroupEntrySPtr GetGroupTableEntry() { return m_formationData->GetGroupTableEntry(); }
    uint32 GetSlotId() const { return m_formationId; }
    float GetDistance() const { return m_distance; }
    float GetAngle() const;
    bool IsMasterSlot() const { return GetSlotId() == 0; }
    FormationData* GetFormationData() const { return m_formationData; }
    // can be null!
    Creature* GetCreature() const { return m_creature; }
    uint32 GetDefaultGuid() const { return m_defaultGuid; }
    Creature* GetMaster() { return m_formationData->GetMaster(); }

    void SetNewPositionRequired() { m_recomputePosition = true; }
    bool NewPositionRequired();

private:
    void SetCreature(Creature* creature);

    Creature* m_creature;
    FormationData* m_formationData;
    bool m_recomputePosition;

    float m_angle;
    float m_distance;
    uint32 m_formationId;
    uint32 m_defaultGuid;
};


 #define sFormationMgr MaNGOS::Singleton<FormationMgr>::Instance()

#endif