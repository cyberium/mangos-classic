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
    // fields indexes                           0                1                  2                    3
    const char* sqlFTRequest = "SELECT `GroupGuid`, `FormationType`, `FormationSpread` , `FormationOptions` from `group_formation`";
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

FormationData::FormationData(CreaturesGroupDataSPtr& gData, FormationEntrySPtr& fEntry, uint32 realMasterGuid) :
    m_groupData(gData), m_fEntry(fEntry), m_currentFormationShape(fEntry->formationType),
    m_masterSlot(nullptr), m_formationEnabled(false), m_mirrorState(false), m_needToFixPositions(false),
    m_keepCompact(false), m_validFormation(true), m_lastWP(0), m_wpPathId(0), m_realMaster(nullptr),
    m_realMasterGuid(realMasterGuid),
    m_masterMotionType(MasterMotionType::FORMATION_TYPE_MASTER_RANDOM),
    m_updateDelay(5000) // enforce first formation update 5 sec after spawning
{

}

FormationData::~FormationData()
{
    sLog.outDebug("Deleting formation (%u)!!!!!", m_groupData->guid);
}

void FormationData::SetFollowersMaster()
{
    Unit* master = GetMaster();
    if (!master)
    {
        return;
    }

    for (auto slotItr : m_groupData->creatureSlots)
    {
        auto& currentSlot = slotItr.second;

        // creature might be in group but not in formation
        if (!currentSlot->GetFormationSlotData())
            continue;

        if (currentSlot == m_masterSlot)
            continue;

        auto follower = currentSlot->GetEntity();

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
    if (!(fId < MAX_GROUP_FORMATION_TYPE))
        return false;

    if (m_currentFormationShape == GroupFormationType(fId))
        return false;

    m_currentFormationShape = GroupFormationType(fId);

    m_needToFixPositions = true;
    return true;
}


bool FormationData::SetNewMaster(Creature* creature)
{
    return TrySetNewMaster(creature);
}

// remove all creatures from formation data
void FormationData::Disband()
{
    ClearMoveGen();
    for (auto& slotItr : m_groupData->creatureSlots)
    {
        auto& slot = slotItr.second;

        // creature might be in group but not in formation
        if (!slot->GetFormationSlotData())
            continue;

        slot->GetFormationSlotData().reset();
    }

    m_groupData->formationData = nullptr;
}

// remove all movegen (maybe we should remove only move in formation one)
void FormationData::ClearMoveGen()
{
    for (auto& slotItr : m_groupData->creatureSlots)
    {
        auto& slot = slotItr.second;

        // creature might be in group but not in formation
        if (!slot->GetFormationSlotData())
            continue;

        Unit* slotUnit = slot->GetEntity();
        if (slotUnit && slotUnit->IsAlive())
        {
            if (slotUnit->IsFormationMaster())
            {
                m_lastWP = slotUnit->GetMotionMaster()->getLastReachedWaypoint();
                m_wpPathId = slotUnit->GetMotionMaster()->GetPathId();
            }
            slotUnit->GetMotionMaster()->Clear(true);
        }
    }
}

Unit* FormationData::GetMaster()
{
    if (m_masterSlot)
        return m_masterSlot->GetEntity();
    return nullptr;
}

void FormationData::SetMasterMovement(Creature* newMaster)
{
    auto& newMasterSlot = newMaster->GetGroupSlot();
    newMaster->GetMotionMaster()->Clear(true, true);
    if (m_masterMotionType == MasterMotionType::FORMATION_TYPE_MASTER_WAYPOINT)
    {
        newMaster->GetMotionMaster()->MoveWaypoint(m_wpPathId, 0, 0, 0, m_realMasterGuid, m_lastWP);
        m_wpPathId = 0;
        m_lastWP = 0;
    }
    else if (m_masterMotionType == MasterMotionType::FORMATION_TYPE_MASTER_RANDOM)
    {
        newMaster->GetMotionMaster()->MoveRandomAroundPoint(m_spawnPos.x, m_spawnPos.y, m_spawnPos.z, m_spawnPos.radius);
    }

    if (!m_realMaster)
        m_realMaster = newMaster;

    m_masterSlot = newMasterSlot;
}

bool FormationData::TrySetNewMaster(Creature* masterCandidat /*= nullptr*/)
{
    CreatureGroupSlotSPtr aliveSlot = nullptr;

    if (masterCandidat)
    {
        auto& candidateSlot = masterCandidat->GetGroupSlot();

        // candidate have to be in this group
        if (candidateSlot && candidateSlot->GetGroupData()->guid == m_groupData->guid && masterCandidat->IsAlive())
            aliveSlot = candidateSlot;
    }
    else
    {
        // Get first alive slot
        aliveSlot = m_groupData->GetFirstAliveSlot();
    }

    if (aliveSlot)
    {
        Unit* newMasterUnit = aliveSlot->GetEntity();
        if (newMasterUnit->IsCreature())
        {
            auto newMaster = static_cast<Creature*>(newMasterUnit);
            Replace(newMaster, m_masterSlot);
            SetMasterMovement(newMaster);
            SetFollowersMaster();
        }
        return true;
    }
    else
    {
        // we can remove this formation from memory
        m_validFormation = false;
    }

    return false;
}

bool FormationData::Update(uint32 diff)
{
    m_updateDelay.Update(diff);
    if (m_updateDelay.Passed())
    {
        m_updateDelay.Reset(2000);

        if (!m_formationEnabled)
            return m_validFormation;

        // can happen when temp summon is master
        if (!m_realMaster && !TrySetNewMaster())
            return m_validFormation;

        if (m_needToFixPositions)
        {
            FixSlotsPositions();
            m_needToFixPositions = false;
        }

        auto master = GetMaster();
        if (!master || !master->IsAlive())
            TrySetNewMaster();
    }
    return m_validFormation;
}

void FormationData::Reset()
{
    if (!m_realMaster || !m_realMaster->IsInWorld())
        return;

    m_mirrorState = false;

    SwitchFormation(m_fEntry->formationType);

    // just be sure to fix all position
    m_needToFixPositions = true;
}

void FormationData::OnMasterRemoved()
{
    m_formationEnabled = false;
    m_realMaster = nullptr;
    m_masterSlot = nullptr;
}

void FormationData::OnRespawn(Creature* creature)
{
    auto freeSlot = m_groupData->GetFirstFreeSlot(creature->GetGUIDLow());

    MANGOS_ASSERT(freeSlot != nullptr);

    // respawn of master before FormationData::Update occur
    if (freeSlot->IsFormationMaster())
    {
        TrySetNewMaster(creature);
        return;
    }

    auto master = GetMaster();
    if (master)
        creature->Relocate(master->GetPositionX(), master->GetPositionY(), master->GetPositionZ());

    auto oldSlot = creature->GetGroupSlot();

    if (freeSlot != oldSlot)
        Replace(creature, freeSlot);

    if (m_keepCompact)
        FixSlotsPositions(true);

    creature->GetMotionMaster()->Clear(false, true);
    creature->GetMotionMaster()->MoveInFormation(freeSlot);
}

void FormationData::OnDeath(Creature* creature)
{
    if (creature->IsFormationMaster())
    {
        m_lastWP = creature->GetMotionMaster()->getLastReachedWaypoint();
        m_wpPathId = creature->GetMotionMaster()->GetPathId();

        m_updateDelay.Reset(5000);
    }
}

void FormationData::OnEntityDelete(Unit* entity)
{
    if (entity->IsCreature())
    {
        Creature* creature = static_cast<Creature*>(entity);
        auto& slot = creature->GetGroupSlot();

        sLog.outString("Deleting creature from formation(%u)", m_groupData->guid);
        if (slot->IsFormationMaster())
        {
            OnMasterRemoved();
        }

        if (creature->IsTemporarySummon())
        {
            creature->RemoveFromFormation();
        }
    }
}

void FormationData::OnSlotAdded(Unit* entity)
{
    CreatureGroupSlotSPtr sData = nullptr;
    sData = m_groupData->GetSlotByGuid(entity->GetGUIDLow());
    uint32 slotId = sData->GetSlotId();

    entity->SetActiveObjectState(true);

    sLog.outString("Slot(%u) filled by %s in formation(%u)", slotId, entity->GetGuidStr().c_str(), m_groupData->guid);

    Creature* creature = entity->IsCreature() ? static_cast<Creature*>(entity) : nullptr;

    if (!m_realMaster)
    {
        if ((creature && creature->IsTemporarySummon()) || slotId == 0)
        {
            m_formationEnabled = true;
            m_realMaster = creature;
            m_masterSlot = sData;
            creature->GetRespawnCoord(m_spawnPos.x, m_spawnPos.y, m_spawnPos.z, nullptr, &m_spawnPos.radius);

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
    }

    if (entity->IsAlive())
        SetFollowersMaster();

    if (m_masterSlot)
        FixSlotsPositions();
    else
        m_needToFixPositions = true;
}

void FormationData::OnWaypointStart()
{
    SetMirrorState(false);
}

void FormationData::OnWaypointEnd()
{
    SetMirrorState(true);
}


// replace to either first available slot position or provided one
void FormationData::Replace(Creature* creature, CreatureGroupSlotSPtr slot /*= nullptr*/)
{
    if (!slot)
    {
        slot = m_groupData->GetFirstFreeSlot(creature->GetGUIDLow());
        if (!slot)
        {
            sLog.outError("FormationData::Replace> Failed to replace %s! No available slot!", creature->GetGuidStr());
            return;
        }
    }

    // swap 2 slots positions
    auto& currSData = creature->GetGroupSlot()->GetFormationSlotData();
    auto& slotData = slot->GetFormationSlotData();

    float temp = currSData->angle;
    currSData->angle = slotData->angle;
    slotData->angle = temp;

    temp = currSData->distance;
    currSData->distance = slotData->distance;
    slotData->distance = temp;
}

void FormationData::Compact()
{
    FixSlotsPositions(true);
    m_keepCompact = true;
}

void FormationData::Add(Creature* creature)
{

}

void FormationData::FixSlotsPositions(bool onlyAlive /*= false*/)
{
    float defaultDist =  m_fEntry->distance;
    auto& slots = m_groupData->creatureSlots;
    float totalMembers = 0;
    for (auto& slotItr : slots)
    {
        auto& slot = slotItr.second;
        // creature might be in group but not in formation
        if (!slot->GetFormationSlotData())
            continue;

        if (!slot->GetEntity() || (onlyAlive && !slot->GetEntity()->IsAlive()))
            continue;

        if (slot->IsFormationMaster())
            continue;

        ++totalMembers;
    }

    if (!totalMembers)
        return;

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
            uint32 membCount = 1;
            for (auto& slotItr : slots)
            {
                auto& slot = slotItr.second;
                auto& sData = slot->GetFormationSlotData();
                if (!sData)     // creature might be in group but not in formation
                    continue;

                if (slot->IsFormationMaster())
                {
                    sData->angle = 0;
                    sData->distance = 0;
                    continue;
                }

                if (onlyAlive && (!slot->GetEntity() || !slot->GetEntity()->IsAlive()))
                    continue;

                sData->angle = M_PI_F;
                sData->distance = defaultDist * membCount;
                ++membCount;
            }
            break;
        }

        // side by side formation
        case GROUP_FORMATION_TYPE_SIDE_BY_SIDE:
        {
            uint32 membCount = 1;
            for (auto& slotItr : slots)
            {
                auto& slot = slotItr.second;
                auto& sData = slot->GetFormationSlotData();
                if (!sData)     // creature might be in group but not in formation
                    continue;

                if (slot->IsFormationMaster())
                {
                    sData->angle = 0;
                    sData->distance = 0;
                    continue;
                }

                if (onlyAlive && (!slot->GetEntity() || !slot->GetEntity()->IsAlive()))
                    continue;

                if ((membCount & 1) == 0)
                    sData->angle = (M_PI_F / 2.0f) + M_PI_F;
                else
                    sData->angle = M_PI_F / 2.0f;
                sData->distance = defaultDist * (((membCount-1) / 2) + 1);
                ++membCount;
            }
            break;
        }

        // like a geese formation
        case GROUP_FORMATION_TYPE_LIKE_GEESE:
        {
            uint32 membCount = 1;
            for (auto& slotItr : slots)
            {
                auto& slot = slotItr.second;
                auto& sData = slot->GetFormationSlotData();
                if (!sData)     // creature might be in group but not in formation
                    continue;

                if (slot->IsFormationMaster())
                {
                    sData->angle = 0;
                    sData->distance = 0;
                    continue;
                }

                if (onlyAlive && (!slot->GetEntity() || !slot->GetEntity()->IsAlive()))
                    continue;

                if ((membCount & 1) == 0)
                    sData->angle = M_PI_F + (M_PI_F / 4.0f);
                else
                    sData->angle = M_PI_F - (M_PI_F / 3.0f);
                sData->distance = defaultDist * (((membCount - 1) / 2) + 1);
                ++membCount;
            }
            break;
        }

        // fanned behind formation
        case GROUP_FORMATION_TYPE_FANNED_OUT_BEHIND:
        {
            uint32 membCount = 1;
            for (auto& slotItr : slots)
            {
                auto& slot = slotItr.second;
                auto& sData = slot->GetFormationSlotData();
                if (!sData)     // creature might be in group but not in formation
                    continue;

                if (slot->IsFormationMaster())
                {
                    sData->angle = 0;
                    sData->distance = 0;
                    continue;
                }

                if (onlyAlive && (!slot->GetEntity() || !slot->GetEntity()->IsAlive()))
                    continue;

                sData->angle = (M_PI_F / 2.0f) + (M_PI_F / totalMembers) * (membCount - 1);
                sData->distance = defaultDist;
                ++membCount;
            }
            break;
        }

        // fanned in front formation
        case GROUP_FORMATION_TYPE_FANNED_OUT_IN_FRONT:
        {
            uint32 membCount = 1;
            for (auto& slotItr : slots)
            {
                auto& slot = slotItr.second;
                auto& sData = slot->GetFormationSlotData();
                if (!sData)     // creature might be in group but not in formation
                    continue;

                if (slot->IsFormationMaster())
                {
                    sData->angle = 0;
                    sData->distance = 0;
                    continue;
                }

                if (onlyAlive && (!slot->GetEntity() || !slot->GetEntity()->IsAlive()))
                    continue;

                sData->angle = M_PI_F + (M_PI_F / 2.0f) + (M_PI_F / totalMembers) * (membCount - 1);
                if (sData->angle > M_PI_F * 2.0f)
                    sData->angle = sData->angle - M_PI_F * 2.0f;
                sData->distance = defaultDist;
                ++membCount;
            }
            break;
        }

        // circle formation
        case GROUP_FORMATION_TYPE_CIRCLE_THE_LEADER:
        {
            uint32 membCount = 1;
            for (auto& slotItr : slots)
            {
                auto& slot = slotItr.second;
                auto& sData = slot->GetFormationSlotData();
                if (!sData)     // creature might be in group but not in formation
                    continue;

                if (slot->IsFormationMaster())
                {
                    sData->angle = 0;
                    sData->distance = 0;
                    continue;
                }

                if (onlyAlive && (!slot->GetEntity() || !slot->GetEntity()->IsAlive()))
                    continue;

                sData->angle = ((M_PI_F * 2.0f) / totalMembers) * (membCount - 1);
                sData->distance = defaultDist;
                ++membCount;
            }
            break;
        }
        default:
            break;
    }

    // force a replacement even if the master is not moving
    auto slotItr = slots.begin()++;
    while (slotItr != slots.end())
    {
        auto& slot = slotItr->second;

        auto& sData = slot->GetFormationSlotData();
        if (!sData)     // creature might be in group but not in formation
        {
            ++slotItr;
            continue;
        }

        Unit* slotUnit = slot->GetEntity();
        if (slotUnit && slotUnit->IsAlive())
            slot->SetNewPositionRequired();
        ++slotItr;
    }
}
