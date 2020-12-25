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

#ifndef CMANGOS_FORMATION_H
#define CMANGOS_FORMATION_H

#include "Common.h"
#include "FormationDefs.h"
#include "CreatureGroup/CreatureGroupDefs.h"

namespace G3D
{
class Vector3;
class PointsArray;
}

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
    void LoadGroupFormation();

    FormationEntryMap m_formationEntries;
    FormationSlotInfoMap m_slotInfos;
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
        m_groupTableEntry(groupTableEntry), m_currentFormationShape(groupTableEntry->formationEntry->formationType),
        m_masterSlot(nullptr), m_formationEnabled(true), m_realMaster(nullptr), m_mirrorState(false),
        m_masterMotionType(MasterMotionType::FORMATION_TYPE_MASTER_RANDOM), m_masterCheck(0),
        m_lastWP(0), m_wpPathId(0), m_realMasterGuid(groupTableEntry->masterSlot->defaultCreatureGuid)
    {}
    FormationData() = delete;

    void SetFollowersMaster();
    bool SwitchFormation(uint32 fId);
    void Disband();
    void ClearMoveGen();

    void SetMirrorState(bool state) { m_mirrorState = state; };
    bool GetMirrorState() const { return m_mirrorState; }
    void FillSlot(CreatureGroupSlotEntrySPtr& slot, Creature* creature);
    Creature* GetMaster();
    SlotDataSPtr GetMasterSlot() { return m_masterSlot; };
    void Update(uint32 diff);
    void Reset();

    void OnMasterRemoved() { m_formationEnabled = false; }
    void OnRespawn(Creature* creature);
    void OnDeath(Creature* creature);
    void OnCreatureDelete(Creature* creature);

    void Replace(Creature* creature, SlotDataSPtr slot = nullptr);
    void Compact();
    void Add(Creature* creature);
    void FixSlotsPositions();

    SlotsMap const& GetSlots() const { return m_slotMap; }
    uint32 GetGroupGuid() const { return m_groupTableEntry->guid; }
    uint32 GetGroupEntryId() const { return m_groupTableEntry->groupTemplateEntry->id; }
    uint32 GetFormationId() const { return m_groupTableEntry->formationEntry->formationId; }
    GroupFormationType GetFormationType() const { return m_currentFormationShape; }
    CreaturesGroupEntrySPtr GetGroupTableEntry() { return m_groupTableEntry; }

private:
    SlotDataSPtr GetFirstAliveSlot();
    SlotDataSPtr GetFirstFreeSlot(uint32 guid);
    void SetMasterMovement(Creature* master);
    void TrySetNewMaster(Creature* masterCandidat = nullptr);
    CreaturesGroupEntrySPtr m_groupTableEntry;
    GroupFormationType m_currentFormationShape;
    SlotDataSPtr m_masterSlot;
    SlotsMap m_slotMap;
    bool m_formationEnabled;
    bool m_mirrorState;
    bool m_needToFixPositions;

    uint32 m_lastWP;
    uint32 m_wpPathId;
    uint32 m_realMasterGuid;

    Creature* m_realMaster;

    MasterMotionType m_masterMotionType;
    ShortTimeTracker m_masterCheck;
};

struct SlotData
{
    friend class FormationData;

public:
    SlotData(CreatureGroupSlotEntrySPtr& slot, Creature* _creature, FormationData* fData);
    SlotData() = delete;

    ~SlotData();

    uint32 GetFormationId() const { return m_formationData->GetFormationId(); }
    CreaturesGroupEntrySPtr GetGroupTableEntry() { return m_formationData->GetGroupTableEntry(); }
    uint32 GetSlotId() const { return m_slotId; }
    float GetDistance() const { return m_distance; }
    float GetAngle() const;
    FormationData* GetFormationData() const { return m_formationData; }
    // can be null!
    Creature* GetCreature() const { return m_creature; }
    uint32 GetDefaultGuid() const { return m_defaultGuid; }
    Creature* GetMaster() { return m_formationData->GetMaster(); }
    bool IsMasterSlot() const;

    void SetNewPositionRequired() { m_recomputePosition = true; }
    bool NewPositionRequired();

private:
    void SetCreature(Creature* creature);

    Creature* m_creature;
    FormationData* m_formationData;
    bool m_recomputePosition;

    float m_angle;
    float m_distance;
    uint32 m_slotId;
    uint32 m_defaultGuid;
};

 #define sFormationMgr MaNGOS::Singleton<FormationMgr>::Instance()

#endif