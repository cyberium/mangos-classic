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
    bool SetNewMaster(Creature* creature);
    void Disband();
    void ClearMoveGen();

    void SetMirrorState(bool state) { m_mirrorState = state; };
    bool GetMirrorState() const { return m_mirrorState; }
    void FillSlot(CreatureGroupSlotEntrySPtr& slot, Creature* creature);
    Unit* GetMaster();
    FormationSlotSPtr GetMasterSlot() { return m_masterSlot; };
    void Update(uint32 diff);
    void Reset();

    void OnMasterRemoved() { m_formationEnabled = false; }
    void OnRespawn(Creature* creature);
    void OnDeath(Creature* creature);
    void OnEntityDelete(Unit* entity);

    void Replace(Creature* creature, FormationSlotSPtr slot = nullptr);
    void Compact();
    void Add(Creature* creature);
    void FixSlotsPositions(bool onlyAlive = false);


    FormationSlotMap const& GetSlots() const { return m_slotMap; }
    uint32 GetGroupGuid() const { return m_groupTableEntry->guid; }
    uint32 GetGroupEntryId() const { return m_groupTableEntry->groupTemplateEntry->id; }
    uint32 GetFormationId() const { return m_groupTableEntry->formationEntry->formationId; }
    GroupFormationType GetFormationType() const { return m_currentFormationShape; }
    CreaturesGroupEntrySPtr GetGroupTableEntry() { return m_groupTableEntry; }

private:
    FormationSlotSPtr GetFirstAliveSlot();
    FormationSlotSPtr GetFirstFreeSlot(uint32 guid);
    void SetMasterMovement(Creature* master);
    bool TrySetNewMaster(Creature* masterCandidat = nullptr);
    CreaturesGroupEntrySPtr m_groupTableEntry;
    GroupFormationType m_currentFormationShape;
    FormationSlotSPtr m_masterSlot;
    FormationSlotMap m_slotMap;
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

struct FormationSlot
{
    friend class FormationData;

public:
    FormationSlot(Unit* _entity, FormationData* fData);
    FormationSlot() = delete;

    // some helper
    uint32 GetFormationId() const { return m_formationData->GetFormationId(); }
    CreaturesGroupEntrySPtr GetGroupTableEntry() { return m_formationData->GetGroupTableEntry(); }

    // important for MovGen
    float GetDistance() const { return m_distance; }
    float GetAngle() const;
    Unit* GetEntity() { return m_entity; }
    void SetNewPositionRequired() { m_recomputePosition = true; }
    bool NewPositionRequired();

    Unit* GetMaster() { return m_formationData->GetMaster(); }
    bool IsMasterSlot() const;

    FormationDataSPtr GetFormationData() { return m_formationData; }
    virtual uint32 GetDefaultGuid() const { return 0; }

private:
    float m_angle;
    float m_distance;
    Unit* m_entity;

    FormationDataSPtr m_formationData;
    bool m_recomputePosition;
};

struct CreatureFormationSlot : public FormationSlot
{
public:
    CreatureFormationSlot(Creature* _creature, FormationData* fData);

    uint32 GetDefaultGuid() const override { return m_defaultGuid; }
private:
    uint32 m_defaultGuid;
};

struct PlayerFormationSlot : public FormationSlot
{
public:
    PlayerFormationSlot(Player* _player, FormationData* fData);

    uint32 GetDefaultGuid() const override { return m_defaultGuid; }
private:
    uint32 m_defaultGuid;
};

 #define sFormationMgr MaNGOS::Singleton<FormationMgr>::Instance()

#endif