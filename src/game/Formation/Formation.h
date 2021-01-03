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

    void SetFormationSlot(Creature* creature);
    FormationEntrySPtr GetFormationEntry(uint32 groupId);

    FormationDataSPtr CreateDynamicFormation(Creature* creatureMaster, GroupFormationType type = GROUP_FORMATION_TYPE_SINGLE_FILE);

    template <typename T>
    bool AddMemberToDynGroup(Creature* master, T* entity);

    void Update(FormationDataMap& fDataMap);

private:
    void LoadGroupFormation();

    FormationEntryMap m_formationEntries;
};

enum class MasterMotionType
{
    FORMATION_TYPE_MASTER_RANDOM,
    FORMATION_TYPE_MASTER_WAYPOINT
};

class FormationData
{
public:
    FormationData(CreaturesGroupDataSPtr& gData, FormationEntrySPtr& fEntry);
    FormationData() = delete;
    ~FormationData();

    void SetFollowersMaster();
    bool SwitchFormation(uint32 fId);
    bool SetNewMaster(Creature* creature);
    void Disband();
    void ClearMoveGen();

    void SetMirrorState(bool state) { m_mirrorState = state; };
    bool GetMirrorState() const { return m_mirrorState; }

    Unit* GetMaster();
    float GetDistance() const { return m_fEntry->distance; }
    CreatureGroupSlotSPtr GetMasterSlot() { return m_masterSlot; };
    uint32 GetRealMasterGuid() const { return m_realMasterGuid; }
    bool Update(uint32 diff);
    void Reset();

    void OnMasterRemoved();
    void OnRespawn(Creature* creature);
    void OnDeath(Creature* creature);
    void OnEntityDelete(Unit* entity);

    void OnSlotAdded(Creature* creature);

    void Replace(Creature* creature, CreatureGroupSlotSPtr slot = nullptr);
    void Compact();
    void Add(Creature* creature);
    void FixSlotsPositions(bool onlyAlive = false);


    GroupFormationType GetFormationType() const { return m_currentFormationShape; }

private:
    void SetMasterMovement(Creature* master);
    bool TrySetNewMaster(Creature* masterCandidat = nullptr);
    CreaturesGroupDataSPtr m_groupData;
    FormationEntrySPtr m_fEntry;
    GroupFormationType m_currentFormationShape;
    bool m_formationEnabled;
    bool m_mirrorState;
    bool m_needToFixPositions;
    bool m_keepCompact;
    bool m_validFormation;

    uint32 m_lastWP;
    uint32 m_wpPathId;
    Creature* m_realMaster;
    uint32 m_realMasterGuid;

    MasterMotionType m_masterMotionType;
    ShortTimeTracker m_updateDelay;

    CreatureGroupSlotSPtr m_masterSlot;
    RespawnPosistion m_spawnPos;
};

 #define sFormationMgr MaNGOS::Singleton<FormationMgr>::Instance()

#endif