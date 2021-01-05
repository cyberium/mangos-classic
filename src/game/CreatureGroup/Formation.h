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
#include "CreatureGRoup/CreatureGroupDefs.h"

struct FormationEntry;
struct FormationSlotData;
class FormationMgr;
class Creature;
class FormationData;
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

typedef std::map<uint32, uint32> FormationSlotEntryMap;
typedef std::shared_ptr<FormationEntry> FormationEntrySPtr;
typedef std::map<uint32, FormationEntrySPtr> FormationEntryMap;
typedef std::shared_ptr<FormationData> FormationDataSPtr;
typedef std::map<uint32, FormationDataSPtr> FormationDataMap;
typedef std::shared_ptr<FormationSlotData> FormationSlotDataSPtr;

struct RespawnPosistion
{
    float x, y, z, radius;
};

struct FormationEntry
{
    uint32 formationId;
    GroupFormationType formationType;
    uint32 options;
    float distance;
    CreaturesGroupEntrySPtr groupTableEntry;
};

struct FormationSlotData
{
    FormationSlotData() : angle(0), distance(1), recomputePosition(true) {}
    FormationSlotData(float _angle, float _distance = 1) : angle(_angle), distance(_distance), recomputePosition(true) {}

    float angle;
    float distance;
    bool recomputePosition;
};

class FormationMgr
{
public:
    FormationMgr() {}

    void Initialize();

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
    FormationData(CreaturesGroupDataSPtr& gData, FormationEntrySPtr& fEntry, uint32 realMasterGuid);
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
    uint32 GetRealMasterGuid() const { return m_realMasterGuid; }
    bool Update(uint32 diff);
    void Reset();

    void OnMasterRemoved();
    void OnRespawn(Creature* creature);
    void OnDeath(Creature* creature);
    void OnEntityDelete(Unit* entity);

    void OnSlotAdded(Unit* entity);
	void OnWaypointStart();
	void OnWaypointEnd();

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