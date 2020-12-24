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

#include "Formation.h"
#include "Entities/Unit.h"
#include "Database/Database.h"
#include "Policies/Singleton.h"
#include "Entities/Creature.h"
#include "CreatureGroup/CreatureGroup.h"

#include "Movement/MoveSplineInit.h"
#include "Movement/MoveSpline.h"

#include "Pools/PoolManager.h"
#include "MotionGenerators/TargetedMovementGenerator.h"
#include "Timer.h"
#include "Maps/MapManager.h"

INSTANTIATE_SINGLETON_1(FormationMgr);

void FormationMgr::LoadGroupFormation()
{
    sLog.outString("Loading group_formation...");
    // fields indexes                             0             1              2                  3
    const char* sqlFTRequest = "SELECT GroupGuid, FormationType, FormationSpread , FormationOptions from group_formation";
    std::unique_ptr<QueryResult> formationTemplateQR(WorldDatabase.Query(sqlFTRequest));
    if (formationTemplateQR)
    {
        do
        {
            Field* fields = formationTemplateQR->Fetch();

            uint32 groupGuid = fields[0].GetUInt32();
            uint32 formationType = fields[1].GetUInt32();
            float distance = fields[2].GetFloat();
            uint32 options = fields[3].GetUInt32();

            if (formationType >= MAX_GROUP_FORMATION_TYPE)
            {
                sLog.outErrorDb("ERROR LOADING \"group_formation\" formation_type is out of the bound (%u) max is (%u)", formationType, MAX_GROUP_FORMATION_TYPE - 1);
                continue;
            }

            auto& creatureGroup = sCreatureGroupMgr.GetEntryByGroupGuid(groupGuid);
            if (!creatureGroup)
            {
                sLog.outErrorDb("GroupGuid(%u) in `group_formation` is not found in `group_guid` table. Skipping...", groupGuid);
                continue;
            }

            FormationEntrySPtr fEntry = nullptr;
            auto formationItr = m_formationEntries.find(groupGuid);
            if (formationItr == m_formationEntries.end())
            {
                if (creatureGroup->formationEntry)
                {
                    sLog.outErrorDb("GroupGuid(%u) have duplicate entry in group_formation, skipping...", groupGuid);
                    continue;
                }

                m_formationEntries.emplace(groupGuid, new FormationEntry());
                fEntry = m_formationEntries[groupGuid];
                fEntry->formationId = groupGuid;
                fEntry->formationType = GroupFormationType(formationType);
                fEntry->options = options;
                fEntry->distance = distance;
                fEntry->groupTableEntry = creatureGroup;
                creatureGroup->formationEntry = fEntry;
            }

        } while (formationTemplateQR->NextRow());
    }

    sLog.outString(">> Loaded %u group_formation data", uint32(m_formationEntries.size()));
    sLog.outString();
}

void FormationMgr::Initialize()
{
    // load members of the group
    LoadGroupFormation();
}

template<> void FormationMgr::SetFormationSlot<Creature>(Creature* creature, Map* map)
{
    auto& groupData = sCreatureGroupMgr.GetEntryByCreatureGuid(creature->GetGUIDLow(), map->GetId());
    if (groupData)
    {
        auto& slot = groupData->GetSlotByCreatureGuid(creature->GetGUIDLow());
        if (!slot)
            return;

        sLog.outString("Setting formation slot for %s", creature->GetGuidStr().c_str());
        auto fData = map->GetFormationData(groupData);
        fData->FillSlot(slot, creature);
    }
}

FormationSlotInfo const* FormationMgr::GetFormationSlotInfo(uint32 guid)
{
    auto fDataItr = m_slotInfos.find(guid);
    if (fDataItr != m_slotInfos.end())
        return fDataItr->second.get();
    return nullptr;
}

FormationEntrySPtr FormationMgr::GetFormationEntry(uint32 groupId)
{
    auto fEntry = m_formationEntries.find(groupId);
    if (fEntry != m_formationEntries.end())
        return fEntry->second;
    return nullptr;
}

void FormationMgr::Update(FormationDataMap& fDataMap)
{
}

void FormationData::SetFollowersMaster()
{
    Creature* master = GetMaster();
    if (!master)
    {
        return;
    }

    for (auto slotItr : m_slotMap)
    {
        auto& currentSlot = slotItr.second;
        if (currentSlot->GetSlotId() == 0)
            continue;

        auto follower = currentSlot->GetCreature();
        uint32 lowGuid = follower->GetGUIDLow();

        if (follower && follower->IsAlive())
        {
            bool setMgen = false;
            if (follower->GetMotionMaster()->GetCurrentMovementGeneratorType() != FORMATION_MOTION_TYPE)
                setMgen = true;
            else
            {
                auto mgen = static_cast<FormationMovementGenerator const*>(follower->GetMotionMaster()->GetCurrent());
                if (mgen->GetCurrentTarget() != master)
                    setMgen = true;
            }

            if (setMgen)
            {
                follower->GetMotionMaster()->Clear(false, true);
                follower->GetMotionMaster()->MoveInFormation(currentSlot);
                currentSlot->SetNewPositionRequired();
            }
        }
    }
}

bool FormationData::SwitchFormation(uint32 fId)
{
    if (m_slotMap.size() < 2 || !(fId < MAX_GROUP_FORMATION_TYPE))
        return false;

    if (m_currentFormationShape == GroupFormationType(fId))
        return false;

    m_currentFormationShape = GroupFormationType(fId);

    m_needToFixPositions = true;
    return true;
}


// remove all creatures from formation data
void FormationData::Disband()
{
    ClearMoveGen();
    for (auto& slotItr : m_slotMap)
    {
        auto& slot = slotItr.second;

        Creature* slotCreature = slot->GetCreature();
        if (slotCreature && slotCreature->IsAlive())
        {
            slotCreature->RemoveFromFormation();
        }
    }
    m_slotMap.clear();
}

// remove all movegen (maybe we should remove only move in formation one)
void FormationData::ClearMoveGen()
{
    for (auto& slotItr : m_slotMap)
    {
        auto& slot = slotItr.second;

        Creature* slotCreature = slot->GetCreature();
        if (slotCreature && slotCreature->IsAlive())
        {
            if (slotCreature->IsFormationMaster())
            {
                m_lastWP = slotCreature->GetMotionMaster()->getLastReachedWaypoint();
                m_wpPathId = slotCreature->GetMotionMaster()->GetPathId();
            }
            slotCreature->GetMotionMaster()->Clear(true);
        }
    }
}

void FormationData::FillSlot(CreatureGroupSlotEntrySPtr& slot, Creature* creature)
{
    SlotDataSPtr sData = nullptr;
    auto existingSlotItr = m_slotMap.find(slot->slotId);
    if (existingSlotItr == m_slotMap.end())
    {
        sData = SlotDataSPtr(new SlotData(slot, creature, this));
        m_slotMap.emplace(slot->slotId, sData);
    }
    else
    {
        sData = existingSlotItr->second;
        sData->SetCreature(creature);
    }

    creature->SetFormationSlot(sData);
    creature->SetActiveObjectState(true);

    sLog.outString("Slot(%u) filled by %s in formation(%u)", slot->slotId, creature->GetGuidStr().c_str(), slot->creatureGroupEntry->formationEntry->formationId);

    uint32 lowGuid = creature->GetGUIDLow();

    if (slot->slotId == 0 && !m_realMaster)
    {
        m_formationEnabled = true;
        m_realMaster = creature;

        switch (creature->GetDefaultMovementType())
        {
            case RANDOM_MOTION_TYPE:
                m_masterMotionType = MasterMotionType::FORMATION_TYPE_MASTER_RANDOM;
                break;
            case WAYPOINT_MOTION_TYPE:
                m_masterMotionType = MasterMotionType::FORMATION_TYPE_MASTER_WAYPOINT;
                break;
            default:
                sLog.outError("FormationData::FillSlot> Master have not recognized default movement type for formation! Forced to random.");
                m_masterMotionType = MasterMotionType::FORMATION_TYPE_MASTER_RANDOM;
                break;
        }
    }

    if (m_realMaster && creature->IsAlive())
        SetFollowersMaster();
}

Creature* FormationData::GetMaster()
{
    return m_slotMap[0]->GetCreature();
}

SlotDataSPtr FormationData::GetFirstAliveSlot()
{
    for (auto& slotItr : m_slotMap)
    {
        auto& slot = slotItr.second;

        Creature* slotCreature = slot->GetCreature();
        if (slotCreature && slotCreature->IsAlive())
            return slot;
    }
    return nullptr;
}

SlotDataSPtr FormationData::GetFirstFreeSlot(uint32 guid)
{
    for (auto& slotItr : m_slotMap)
    {
        auto& slot = slotItr.second;

        Creature* slotCreature = slot->GetCreature();
        if (!slotCreature || !slotCreature->IsAlive() || slotCreature->GetGUIDLow() == guid)
            return slot;
    }
    return nullptr;
}

void FormationData::SetMasterMovement(Creature* master)
{
    auto& masterSlot = m_slotMap[0];
    master->GetMotionMaster()->Clear(true, true);
    if (m_masterMotionType == MasterMotionType::FORMATION_TYPE_MASTER_WAYPOINT)
    {
        master->GetMotionMaster()->MoveWaypoint(m_wpPathId, 0, 0, 0, masterSlot->GetDefaultGuid(), m_lastWP);
        m_wpPathId = 0;
        m_lastWP = 0;
    }
    else if (m_masterMotionType == MasterMotionType::FORMATION_TYPE_MASTER_RANDOM)
    {
        float x, y, z, radius;
        m_realMaster->GetRespawnCoord(x, y, z, nullptr, &radius);
        master->GetMotionMaster()->MoveRandomAroundPoint(x, y, z, radius);
    }
}

void FormationData::TrySetNewMaster(Creature* masterCandidate/* = nullptr*/)
{
    auto& masterSlot = m_slotMap[0];
    SlotDataSPtr aliveSlot = nullptr;

    if (masterCandidate)
    {
        auto& candidateSlot = masterCandidate->GetFormationSlot();

        // candidate have to be in this group
        if (candidateSlot && candidateSlot->GetFormationId() == GetFormationId() && masterCandidate->IsAlive())
            aliveSlot = candidateSlot;
    }
    else
    {
        // Get first alive slot
        aliveSlot = GetFirstAliveSlot();
    }

    if (aliveSlot)
    {
        Creature* newMaster = aliveSlot->GetCreature();
        SetNewSlot(newMaster, masterSlot);
        SetMasterMovement(newMaster);
        SetFollowersMaster();
    }
}

void FormationData::Update(uint32 diff)
{
    if (!m_realMaster)
        return;

    if (m_needToFixPositions)
    {
        FixSlotsPositions();
        m_needToFixPositions = false;
    }

    m_masterCheck.Update(diff);
    if (m_masterCheck.Passed())
    {
        m_masterCheck.Reset(2000);
        auto master = GetMaster();
        if (!master || !master->IsAlive())
            TrySetNewMaster();
    }
}

void FormationData::Reset()
{
    if (!m_realMaster || !m_realMaster->IsInWorld())
        return;

    //SwitchFormation(m_groupTableEntry->groupTemplateEntry->formationEntry);
    m_mirrorState = false;

    for (auto& slotItr : m_slotMap)
    {
        auto& slot = slotItr.second;

        Creature* slotCreature = slot->GetCreature();

        if (!slotCreature || slotCreature->GetGUIDLow() != slot->GetDefaultGuid())
        {
            auto cData = sObjectMgr.GetCreatureData(slot->GetDefaultGuid());
            if (!cData)
                continue;

            auto foundCreature = m_realMaster->GetMap()->GetCreature(cData->GetObjectGuid(slot->GetDefaultGuid()));
            if (foundCreature)
            {
                slot->SetCreature(foundCreature);
                if (slot->IsMasterSlot())
                    SetMasterMovement(foundCreature);
            }
        }
    }
    SetFollowersMaster();
}

void FormationData::OnRespawn(Creature* creature)
{
    auto freeSlot = GetFirstFreeSlot(creature->GetGUIDLow());

    MANGOS_ASSERT(freeSlot != nullptr);

    // respawn of master before FormationData::Update occur
    if (freeSlot->IsMasterSlot())
    {
        TrySetNewMaster(creature);
        return;
    }

    auto master = GetMaster();
    if (master)
        creature->Relocate(master->GetPositionX(), master->GetPositionY(), master->GetPositionZ());

    auto oldSlot = creature->GetFormationSlot();

    SetNewSlot(creature, freeSlot);
    creature->GetMotionMaster()->Clear(false, true);
    creature->GetMotionMaster()->MoveInFormation(freeSlot);
}

void FormationData::OnDeath(Creature* creature)
{
    if (creature->IsFormationMaster())
    {
        m_lastWP = creature->GetMotionMaster()->getLastReachedWaypoint();
        m_wpPathId = creature->GetMotionMaster()->GetPathId();
    }

//     auto slot = creature->GetFormationSlot();
//     slot->SetCreature(nullptr);
}

void FormationData::OnCreatureDelete(Creature* creature)
{
    auto& slot = creature->GetFormationSlot();

    if (creature == slot->GetCreature())
    {
        sLog.outString("Deleting creature in slot(%u), formation(%u)", slot->GetSlotId(), GetFormationId());
        if (slot->GetSlotId() == 0)
        {
            OnMasterRemoved();
        }
        slot->SetCreature(nullptr);
    }
}

void FormationData::SetNewSlot(Creature* creature, SlotDataSPtr& slot)
{
    Creature* creatureInNewSlot = slot->GetCreature();
    auto oldSlot = creature->GetFormationSlot();

    slot->SetCreature(creature);
    creature->SetFormationSlot(slot);

    if (oldSlot)
        oldSlot->SetCreature(creatureInNewSlot);

    if (creatureInNewSlot)
        creatureInNewSlot->SetFormationSlot(oldSlot);
}

void FormationData::FixSlotsPositions()
{
    float defaultDist =  m_groupTableEntry->formationEntry->distance;
    switch (GetFormationType())
    {
        // random formation
        case GROUP_FORMATION_TYPE_RANDOM:
        {
            break;
        }

        // single file formation
        case GROUP_FORMATION_TYPE_SINGLE_FILE:
        {
            for (auto& slotItr : m_slotMap)
            {
                auto& slot = slotItr.second;
                if (slot->GetSlotId() == 0)
                    continue;

                slot->m_angle = M_PI_F;
                slot->m_distance = defaultDist * slot->GetSlotId();
            }
            break;
        }

        // side by side formation
        case GROUP_FORMATION_TYPE_SIDE_BY_SIDE:
        {
            for (auto& slotItr : m_slotMap)
            {
                auto& slot = slotItr.second;
                if (slot->GetSlotId() == 0)
                    continue;

                if ((slot->GetSlotId() & 1) == 0)
                    slot->m_angle = (M_PI_F / 2.0f) + M_PI_F;
                else
                    slot->m_angle = M_PI_F / 2.0f;
                slot->m_distance = defaultDist * (((slot->GetSlotId() - 1) / 2) + 1);
            }
            break;
        }

        // like a geese formation
        case GROUP_FORMATION_TYPE_LIKE_GEESE:
        {
            for (auto& slotItr : m_slotMap)
            {
                auto& slot = slotItr.second;
                if (slot->GetSlotId() == 0)
                    continue;

                if ((slot->GetSlotId() & 1) == 0)
                    slot->m_angle = M_PI_F + (M_PI_F / 4.0f);
                else
                    slot->m_angle = M_PI_F - (M_PI_F / 3.0f);
                slot->m_distance = defaultDist * (((slot->GetSlotId() - 1) / 2) + 1);
            }
            break;
        }

        // fanned behind formation
        case GROUP_FORMATION_TYPE_FANNED_OUT_BEHIND:
        {
            for (auto& slotItr : m_slotMap)
            {
                auto& slot = slotItr.second;
                if (slot->GetSlotId() == 0)
                    continue;

                slot->m_angle = (M_PI_F / 2.0f) + (M_PI_F / float(m_slotMap.size() - 1)) * (slot->GetSlotId() - 1);
                slot->m_distance = defaultDist;
            }
            break;
        }

        // fanned in front formation
        case GROUP_FORMATION_TYPE_FANNED_OUT_IN_FRONT:
        {
            for (auto& slotItr : m_slotMap)
            {
                auto& slot = slotItr.second;
                if (slot->GetSlotId() == 0)
                    continue;

                slot->m_angle = M_PI_F + (M_PI_F / 2.0f) + (M_PI_F / float(m_slotMap.size() - 1)) * (slot->GetSlotId() - 1);
                if (slot->m_angle > M_PI_F * 2.0f)
                    slot->m_angle = slot->m_angle - M_PI_F * 2.0f;
                slot->m_distance = defaultDist;
            }
            break;
        }

        // circle formation
        case GROUP_FORMATION_TYPE_CIRCLE_THE_LEADER:
        {
            for (auto& slotItr : m_slotMap)
            {
                auto& slot = slotItr.second;
                if (slot->GetSlotId() == 0)
                    continue;

                slot->m_angle = ((M_PI_F * 2.0f) / float(m_slotMap.size() - 1)) * (slot->GetSlotId() - 1);
                slot->m_distance = defaultDist;
            }
            break;
        }
        default:
            break;
    }

    // force a replacement even if the master is not moving
    auto slotItr = m_slotMap.begin()++;
    while (slotItr != m_slotMap.end())
    {
        auto& slot = slotItr->second;
        Creature* creature = slot->GetCreature();
        if (creature)
            slot->SetNewPositionRequired();
        ++slotItr;
    }
}

SlotData::SlotData(CreatureGroupSlotEntrySPtr& slot, Creature* _creature, FormationData* fData) :
    m_creature(_creature), m_formationData(fData), m_recomputePosition(false),
    m_angle(0), m_distance(0), m_slotId(slot->slotId), m_defaultGuid(slot->defaultCreatureGuid)
{
}

SlotData::~SlotData()
{
    if (m_creature)
        m_creature->RemoveFromFormation();
}

float SlotData::GetAngle() const
{
    if (!m_formationData->GetMirrorState())
        return m_angle;

    return (2 * M_PI_F) - m_angle;
}

bool SlotData::NewPositionRequired()
{
    if (!m_recomputePosition)
        return false;

    m_recomputePosition = false;
    return true;
}

void SlotData::SetCreature(Creature* creature)
{
    m_creature = creature;
}
