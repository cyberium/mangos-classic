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
                fEntry->dynamic = false;
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

void FormationMgr::SetFormationSlot(Creature* creature)
{
    Map* map = creature->GetMap();
    auto& groupData = sCreatureGroupMgr.GetEntryByCreatureGuid(creature->GetGUIDLow(), map->GetId());
    if (groupData)
    {
        CreatureGroupSlotEntrySPtr slot = groupData->GetSlotByCreatureGuid(creature->GetGUIDLow());
        if (!slot)
            return;

        sLog.outString("Setting formation slot for %s", creature->GetGuidStr().c_str());
        auto fData = map->GetFormationData(groupData);

        if (!fData)
        {
            fData = std::make_shared<FormationData>(groupData);
            map->AddFormation(fData);
        }

        fData->AddSlot(creature);
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

FormationDataSPtr FormationMgr::CreateDynamicFormation(Creature* creatureMaster, GroupFormationType type /*= GROUP_FORMATION_TYPE_SINGLE_FILE*/)
{
    FormationDataSPtr fData = nullptr;
    if (creatureMaster->GetFormationSlot())
        return creatureMaster->GetFormationSlot()->GetFormationData();

    auto groupData = sCreatureGroupMgr.AddDynamicGroup(creatureMaster);
    if (groupData)
    {
        auto formEmplItr = m_formationEntries.emplace(groupData->guid, new FormationEntry());
        auto fEntry = formEmplItr.first->second;
        fEntry->formationId = groupData->guid;
        fEntry->formationType = type;
        fEntry->options = 0;
        fEntry->dynamic = true;
        fEntry->distance = 1;
        fEntry->groupTableEntry = groupData;
        groupData->formationEntry = fEntry;

        groupData->formationEntry = fEntry;
        fData = std::make_shared<FormationData>(groupData);
        creatureMaster->GetMap()->AddFormation(fData);

        fEntry->slots.emplace(creatureMaster->GetGUIDLow(), new FormationSlotEntry(0, 0, 1, fEntry));

        auto slotEmplItr = groupData->creatureSlot.emplace(creatureMaster->GetGUIDLow(),new CreatureGroupSlotEntry(0, creatureMaster->GetGUIDLow(), groupData));

        fData->AddSlot(creatureMaster);
    }
    return fData;
}

template<typename T>
bool FormationMgr::AddMemberToDynGroup(Creature* master, T* entity)
{
    if (!master || !entity)
        return false;

    if (master->GetMapId() != entity->GetMapId())
        return false;

    auto fData = master->GetFormationSlot()->GetFormationData();
    if (!fData)
        return false;

    fData->AddSlot(entity);

    return true;
}
template bool FormationMgr::AddMemberToDynGroup<Creature>(Creature*, Creature*);
template bool FormationMgr::AddMemberToDynGroup<Player>(Creature*, Player*);

void FormationMgr::Update(FormationDataMap& fDataMap)
{
}

void FormationData::SetFollowersMaster()
{
    Unit* master = GetMaster();
    if (!master)
    {
        return;
    }

    for (auto slotItr : m_slotMap)
    {
        auto& currentSlot = slotItr.second;
        if (currentSlot == m_masterSlot)
            continue;

        auto follower = currentSlot->GetEntity();
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


bool FormationData::SetNewMaster(Creature* creature)
{
    return TrySetNewMaster(creature);
}

// remove all creatures from formation data
void FormationData::Disband()
{
    ClearMoveGen();
    for (auto& slotItr : m_slotMap)
    {
        auto& slot = slotItr.second;

        Unit* slotUnit = slot->GetEntity();
        if (slotUnit && slotUnit->IsAlive())
        {
            slotUnit->RemoveFromFormation();
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

void FormationData::AddSlot(Creature* creature)
{
    FormationSlotSPtr sData = nullptr;
    auto existingSlotItr = m_slotMap.find(creature->GetGUIDLow());
    if (existingSlotItr == m_slotMap.end())
    {
        sData = FormationSlotSPtr(new FormationSlot(creature, this));
        m_slotMap.emplace(creature->GetGUIDLow(), sData);
    }
    else
    {
        sData = existingSlotItr->second;
        sData->m_entity = creature;
    }

    creature->SetFormationSlot(sData);
    creature->SetActiveObjectState(true);

    sLog.outString("Slot filled by %s in formation(%u)", creature->GetGuidStr().c_str(), GetGroupGuid());

    uint32 lowGuid = creature->GetGUIDLow();

    if (!m_realMaster)
    {
        m_formationEnabled = true;
        m_realMaster = creature;
        m_masterSlot = sData;

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

    if (creature->IsAlive())
        SetFollowersMaster();

    m_needToFixPositions = true;
}

void FormationData::AddSlot(Player* player)
{
    if (!m_realMaster)
    {
        sLog.outError("FormationData::AddSlot> cannot add %s to formation(%u). Formation have no master!", player->GetGuidStr().c_str(), GetGroupGuid());
        return;
    }

    FormationSlotSPtr sData = nullptr;
    auto existingSlotItr = m_slotMap.find(player->GetGUIDLow());
    if (existingSlotItr == m_slotMap.end())
    {
        sData = FormationSlotSPtr(new FormationSlot(player, this));
        m_slotMap.emplace(player->GetGUIDLow(), sData);
    }
    else
    {
        sData = existingSlotItr->second;
        sData->m_entity = player;
    }

    player->SetFormationSlot(sData);

    sLog.outString("Slot filled by %s in formation(%u)", player->GetGuidStr().c_str(), GetGroupGuid());
    if (player->IsAlive())
        SetFollowersMaster();
}

Unit* FormationData::GetMaster()
{
    if (m_masterSlot)
        return m_masterSlot->GetEntity();
    return nullptr;
}

FormationSlotSPtr FormationData::GetFirstAliveSlot()
{
    for (auto& slotItr : m_slotMap)
    {
        auto& slot = slotItr.second;

        Unit* slotUnit = slot->GetEntity();
        if (slotUnit && slotUnit->IsAlive())
            return slot;
    }
    return nullptr;
}

FormationSlotSPtr FormationData::GetFirstFreeSlot(uint32 guid)
{
    for (auto& slotItr : m_slotMap)
    {
        auto& slot = slotItr.second;

        Unit* slotUnit = slot->GetEntity();
        if (!slotUnit || !slotUnit->IsAlive() || (slotUnit->GetGUIDLow() == guid && !slotUnit->IsPlayer()))
            return slot;
    }
    return nullptr;
}

void FormationData::SetMasterMovement(Creature* newMaster)
{
    auto& newMasterSlot = newMaster->GetFormationSlot();
    newMaster->GetMotionMaster()->Clear(true, true);
    if (m_masterMotionType == MasterMotionType::FORMATION_TYPE_MASTER_WAYPOINT)
    {
        newMaster->GetMotionMaster()->MoveWaypoint(m_wpPathId, 0, 0, 0, m_realMasterGuid, m_lastWP);
        m_wpPathId = 0;
        m_lastWP = 0;
    }
    else if (m_masterMotionType == MasterMotionType::FORMATION_TYPE_MASTER_RANDOM)
    {
        float x, y, z, radius;
        if (m_realMaster)
            m_realMaster->GetRespawnCoord(x, y, z, nullptr, &radius);
        else
        {
            auto cData = sObjectMgr.GetCreatureData(m_realMasterGuid);
            x = cData->posX;
            y = cData->posY;
            z = cData->posZ;
            radius = cData->spawndist;
        }
        newMaster->GetMotionMaster()->MoveRandomAroundPoint(x, y, z, radius);
    }

    m_masterSlot = newMasterSlot;
}

bool FormationData::TrySetNewMaster(Creature* masterCandidat /*= nullptr*/)
{
    FormationSlotSPtr aliveSlot = nullptr;

    if (masterCandidat)
    {
        auto& candidateSlot = masterCandidat->GetFormationSlot();

        // candidate have to be in this group
        if (candidateSlot && candidateSlot->GetFormationId() == GetFormationId() && masterCandidat->IsAlive())
            aliveSlot = candidateSlot;
    }
    else
    {
        // Get first alive slot
        aliveSlot = GetFirstAliveSlot();
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

    return false;
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

    m_mirrorState = false;

    SwitchFormation(m_groupTableEntry->formationEntry->formationType);

    // just be sure to fix all position
    m_needToFixPositions = true;
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

        m_masterCheck.Reset(5000);
    }

//     auto slot = creature->GetFormationSlot();
//     slot->SetCreature(nullptr);
}

void FormationData::OnEntityDelete(Unit* entity)
{
    auto& slot = entity->GetFormationSlot();

    sLog.outString("Deleting creature from formation(%u)", GetFormationId());
    if (slot->IsMasterSlot())
    {
        OnMasterRemoved();
    }
    slot->m_entity = nullptr;
}

// replace to either first available slot position or provided one
void FormationData::Replace(Creature* creature, FormationSlotSPtr slot /*= nullptr*/)
{
    if (!slot)
    {
        slot = GetFirstFreeSlot(creature->GetGUIDLow());
        if (!slot)
        {
            sLog.outError("FormationData::Replace> Failed to replace %s! No available slot!", creature->GetGuidStr());
            return;
        }
    }

    // swap 2 slots positions
    auto currSlot = creature->GetFormationSlot();

    float temp = currSlot->m_angle;
    currSlot->m_angle = slot->m_angle;
    slot->m_angle = temp;

    temp = currSlot->m_distance;
    currSlot->m_distance = slot->m_distance;
    slot->m_distance = temp;
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
    float defaultDist =  m_groupTableEntry->formationEntry->distance;
    float totalMembers = float(m_slotMap.size() - 1);
    if (onlyAlive)
    {
        totalMembers = 0;
        for (auto& slotItr : m_slotMap)
        {
            auto& slot = slotItr.second;
            if (!slot->GetEntity() || !slot->GetEntity()->IsAlive())
                continue;

            if (slot->IsMasterSlot())
                continue;
            ++totalMembers;
        }
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
            for (auto& slotItr : m_slotMap)
            {
                auto& slot = slotItr.second;
                if (slot->IsMasterSlot())
                {
                    slot->m_angle = 0;
                    slot->m_distance = 0;
                    continue;
                }

                if (onlyAlive && (!slot->GetEntity() || !slot->GetEntity()->IsAlive()))
                    continue;

                slot->m_angle = M_PI_F;
                slot->m_distance = defaultDist * membCount;
                ++membCount;
            }
            break;
        }

        // side by side formation
        case GROUP_FORMATION_TYPE_SIDE_BY_SIDE:
        {
            uint32 membCount = 1;
            for (auto& slotItr : m_slotMap)
            {
                auto& slot = slotItr.second;
                if (slot->IsMasterSlot())
                {
                    slot->m_angle = 0;
                    slot->m_distance = 0;
                    continue;
                }

                if (onlyAlive && (!slot->GetEntity() || !slot->GetEntity()->IsAlive()))
                    continue;

                if ((membCount & 1) == 0)
                    slot->m_angle = (M_PI_F / 2.0f) + M_PI_F;
                else
                    slot->m_angle = M_PI_F / 2.0f;
                slot->m_distance = defaultDist * (((membCount-1) / 2) + 1);
                ++membCount;
            }
            break;
        }

        // like a geese formation
        case GROUP_FORMATION_TYPE_LIKE_GEESE:
        {
            uint32 membCount = 1;
            for (auto& slotItr : m_slotMap)
            {
                auto& slot = slotItr.second;
                if (slot->IsMasterSlot())
                {
                    slot->m_angle = 0;
                    slot->m_distance = 0;
                    continue;
                }

                if (onlyAlive && (!slot->GetEntity() || !slot->GetEntity()->IsAlive()))
                    continue;

                if ((membCount & 1) == 0)
                    slot->m_angle = M_PI_F + (M_PI_F / 4.0f);
                else
                    slot->m_angle = M_PI_F - (M_PI_F / 3.0f);
                slot->m_distance = defaultDist * (((membCount - 1) / 2) + 1);
                ++membCount;
            }
            break;
        }

        // fanned behind formation
        case GROUP_FORMATION_TYPE_FANNED_OUT_BEHIND:
        {
            uint32 membCount = 1;
            for (auto& slotItr : m_slotMap)
            {
                auto& slot = slotItr.second;
                if (slot->IsMasterSlot())
                {
                    slot->m_angle = 0;
                    slot->m_distance = 0;
                    continue;
                }

                if (onlyAlive && (!slot->GetEntity() || !slot->GetEntity()->IsAlive()))
                    continue;

                slot->m_angle = (M_PI_F / 2.0f) + (M_PI_F / totalMembers) * (membCount - 1);
                slot->m_distance = defaultDist;
                ++membCount;
            }
            break;
        }

        // fanned in front formation
        case GROUP_FORMATION_TYPE_FANNED_OUT_IN_FRONT:
        {
            uint32 membCount = 1;
            for (auto& slotItr : m_slotMap)
            {
                auto& slot = slotItr.second;
                if (slot->IsMasterSlot())
                {
                    slot->m_angle = 0;
                    slot->m_distance = 0;
                    continue;
                }

                if (onlyAlive && (!slot->GetEntity() || !slot->GetEntity()->IsAlive()))
                    continue;

                slot->m_angle = M_PI_F + (M_PI_F / 2.0f) + (M_PI_F / totalMembers) * (membCount - 1);
                if (slot->m_angle > M_PI_F * 2.0f)
                    slot->m_angle = slot->m_angle - M_PI_F * 2.0f;
                slot->m_distance = defaultDist;
                ++membCount;
            }
            break;
        }

        // circle formation
        case GROUP_FORMATION_TYPE_CIRCLE_THE_LEADER:
        {
            uint32 membCount = 1;
            for (auto& slotItr : m_slotMap)
            {
                auto& slot = slotItr.second;
                if (slot->IsMasterSlot())
                {
                    slot->m_angle = 0;
                    slot->m_distance = 0;
                    continue;
                }

                if (onlyAlive && (!slot->GetEntity() || !slot->GetEntity()->IsAlive()))
                    continue;

                slot->m_angle = ((M_PI_F * 2.0f) / totalMembers) * (membCount - 1);
                slot->m_distance = defaultDist;
                ++membCount;
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
        Unit* slotUnit = slot->GetEntity();
        if (slotUnit && slotUnit->IsAlive())
            slot->SetNewPositionRequired();
        ++slotItr;
    }
}

// Slot base class responsible to keep track of an entity in the slow and its position relative to master
FormationSlot::FormationSlot(Unit* _entity, FormationData* fData) :
    m_entity(_entity), m_formationData(fData), m_recomputePosition(false),
    m_angle(0), m_distance(0)
{
}

float FormationSlot::GetAngle() const
{
    if (!m_formationData->GetMirrorState())
        return m_angle;

    return (2 * M_PI_F) - m_angle;
}

bool FormationSlot::NewPositionRequired()
{
    if (!m_recomputePosition)
        return false;

    m_recomputePosition = false;
    return true;
}

bool FormationSlot::IsMasterSlot() const
{
    auto masterSlot = m_formationData->GetMasterSlot();
    if (masterSlot)
        return masterSlot.get() == this;
    return false;
}

CreatureFormationSlot::CreatureFormationSlot(Creature* _creature, FormationData* fData) :
    FormationSlot(_creature, fData), m_defaultGuid(_creature->GetGUIDLow())
{
}

PlayerFormationSlot::PlayerFormationSlot(Player* _player, FormationData* fData) :
    FormationSlot(_player, fData), m_defaultGuid(_player->GetGUIDLow())
{
}
