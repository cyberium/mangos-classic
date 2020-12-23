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
#include "Movement/MoveSplineInit.h"
#include "Movement/MoveSpline.h"

#include "Pools/PoolManager.h"
#include "MotionGenerators/TargetedMovementGenerator.h"
#include "Timer.h"
#include "Maps/MapManager.h"

INSTANTIATE_SINGLETON_1(FormationMgr);

void FormationMgr::LoadGroupTemplate()
{
    sLog.outString("Loading group_template...");
    uint32 count = 0;

    // fields indexes                            0     1
    const char* sqlGTRequest = "SELECT group_entry, name from group_template";
    std::unique_ptr<QueryResult> groupTemplateQR(WorldDatabase.Query(sqlGTRequest));
    if (groupTemplateQR)
    {
        do
        {
            Field* fields = groupTemplateQR->Fetch();

            uint32 groupId = fields[0].GetUInt32();
            std::string gName = fields[1].GetString();

            m_groupTemplateEntries.emplace(groupId, new CreraturesGroupTemplateEntry(groupId, gName));
            count++;
        } while (groupTemplateQR->NextRow());
    }

    sLog.outString(">> Loaded %u group template", count);
    sLog.outString();
}

void FormationMgr::LoadGroupGuids()
{
    sLog.outString("Loading groups...");

    // fields indexes                         0            1
    const char* sqlGrpRequest = "SELECT group_guid, group_entry from group_guid";

    uint32 count = 0;
    std::unique_ptr<QueryResult> groupQR(WorldDatabase.Query(sqlGrpRequest));

    if (groupQR)
    {
        do
        {
            Field* fields = groupQR->Fetch();

            uint32 groupGuid = fields[0].GetUInt32();
            uint32 groupEntryId = fields[1].GetUInt32();

            auto& groupTemplateEntryItr = m_groupTemplateEntries.find(groupEntryId);
            if (groupTemplateEntryItr == m_groupTemplateEntries.end())
            {
                sLog.outErrorDb("Template(%u) referenced in table groups(%u) is not found in group_template table", groupEntryId, groupGuid);
                continue;
            }

            m_groupGuids.emplace(groupGuid, groupEntryId);

            ++count;
        } while (groupQR->NextRow());
    }
    sLog.outString(">> Loaded %u groups definitions", count);
    sLog.outString();
}

void FormationMgr::LoadGroupMembers()
{
    sLog.outString("Loading group_member...");

    // fields indexes                          0            1        2
    const char* sqlGMRequest = "SELECT group_guid, member_guid, slot_id from group_member";

    uint32 count = 0;
    std::unique_ptr<QueryResult> groupMemberQR(WorldDatabase.Query(sqlGMRequest));

    std::multimap<uint32, std::tuple<uint32, uint32>> fContainer;
    std::map<uint32, std::set<uint32>> unique_membersGuids;
    std::set <std::pair<uint32, uint32>> uniqueSlotIdPerGroup;

    if (groupMemberQR)
    {
        do
        {
            Field* fields = groupMemberQR->Fetch();

            uint32 groupGuid = fields[0].GetUInt32();
            uint32 memberGuid = fields[1].GetUInt32();
            uint32 slotId = fields[2].GetUInt32();

            auto sData = std::make_pair(groupGuid, slotId);


            auto poolId = sPoolMgr.IsPartOfAPool<Creature>(memberGuid);
            if (poolId)
            {
                sLog.outErrorDb("Creature guid(%u) have valid PoolId(%u) that will not work with formation. Disabling pool...", memberGuid, poolId);
                sPoolMgr.RemoveAutoSpawnForPool(poolId);
                sPoolMgr.DespawnPoolInMaps(poolId);
                sPoolMgr.RemoveFromPool<Creature>(memberGuid);
                CreatureData const* cData = sObjectMgr.GetCreatureData(memberGuid);
                sObjectMgr.AddCreatureToGrid(memberGuid, cData);
            }

            auto cData = sObjectMgr.GetCreatureData(memberGuid);
            if (cData)
            {
                auto linkInfo = sCreatureLinkingMgr.GetLinkedTriggerInformation(cData->id, 0, cData->mapid);
                if (linkInfo)
                {
                    sLog.outErrorDb("Creature guid(%u) have its entry(%u) in linked creature table that will not work with formation. Disabling Linking...", memberGuid, cData->id);
                    sCreatureLinkingMgr.DeleteEntry(cData->id, cData->mapid);
                }
            }

            auto linkInfo = sCreatureLinkingMgr.GetLinkedTriggerInformation(0, memberGuid, 0);
            if (linkInfo)
            {
                sLog.outErrorDb("Creature guid(%u) is in linked creature table, that will not work with formation. Disabling Linking...", memberGuid);
                sCreatureLinkingMgr.DeleteGuid(memberGuid);
            }

            auto itr = uniqueSlotIdPerGroup.find(sData);
            if (itr == uniqueSlotIdPerGroup.end())
            {
                fContainer.emplace(groupGuid, std::make_tuple(memberGuid, slotId));
                uniqueSlotIdPerGroup.emplace(sData);
            }
            else
                sLog.outErrorDb("group_member slot(%u) is already assigned skipping...", slotId);

            ++count;
        } while (groupMemberQR->NextRow());
    }

    sLog.outString(">> Loaded %u group members", count);
    sLog.outString();

    sLog.outString("Loading group_formation...");

    count = 0;

    // fields indexes                           0             1              2                    3
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

            FormationEntrySPtr fEntry = nullptr;
            auto formationItr = m_formationEntries.find(groupGuid);
            if (formationItr == m_formationEntries.end())
            {
                auto groupGuidItr = m_groupGuids.find(groupGuid);
                if (groupGuidItr == m_groupGuids.end())
                {
                    sLog.outErrorDb("GroupGuid(%u) in `group_formation` is not found in `group_guid` table", groupGuid);
                    continue;
                }
                auto groupTemplateItr = m_groupTemplateEntries.find(groupGuidItr->second);
                if (groupTemplateItr == m_groupTemplateEntries.end())
                {
                    sLog.outErrorDb("Group entry(%u) in `group_formation` is not found in `group_guid` table", groupGuidItr->first);
                    continue;
                }
                auto& groupEntry = groupTemplateItr->second;

                CreaturesGroupEntrySPtr gEntry = nullptr;
                auto groupEntryItr = m_groupsData.find(groupGuid);
                if (groupEntryItr == m_groupsData.end())
                {
                    gEntry = CreaturesGroupEntrySPtr(new CreaturesGroupEntry(groupGuid, groupEntry));
                    m_groupsData.emplace(groupGuid, gEntry);
                }
                else
                {
                    sLog.outErrorDb("GroupGuid(%u) have duplicate entry in group_formation, skipping...", groupGuid);
                    continue;
                }

                auto bounds = fContainer.equal_range(groupGuid);
                if (std::distance(bounds.first, bounds.second) == 0)
                {
                    sLog.outErrorDb("There is no member defined for formation[GroupGuid(%u)] in group_member table", groupGuid);
                    continue;
                }

                m_formationEntries.emplace(groupGuid, new FormationEntry());
                fEntry = m_formationEntries[groupGuid];
                fEntry->formationId = groupGuid;
                fEntry->formationType = GroupFormationType(formationType);
                fEntry->options = options;
                fEntry->distance = distance;
                gEntry->formationEntry = fEntry;

                // slot check and creations
                FormationSlotInfoMap tempSlotInfoMap;
                bool foundMasterSlot = false;
                for (auto itr = bounds.first; itr != bounds.second; ++itr)
                {
                    auto& memberGuid = std::get<0>(itr->second);
                    auto& slotId = std::get<1>(itr->second);
                    fEntry->slots.emplace(slotId, new FormationSlotEntry(slotId, 0, distance, fEntry));
                    auto& slotEntry = fEntry->slots[slotId];

                    if (slotId == 0)
                    {
                        foundMasterSlot = true;
                    }

                    tempSlotInfoMap.emplace(memberGuid, new FormationSlotInfo(memberGuid, slotEntry, gEntry));
                }

                if (foundMasterSlot)
                {
                    for (auto itr : tempSlotInfoMap)
                        m_slotInfos.emplace(itr);
                }
                else
                    sLog.outErrorDb("Error in table group_member. MasterSlot(0) is not defined for group guid(%u) skipping...", groupGuid);

                ++count;
            }

        } while (formationTemplateQR->NextRow());
    }

    sLog.outString(">> Loaded %u group_formation data", count);
    sLog.outString();
}

void FormationMgr::Initialize()
{
    // load group template
    LoadGroupTemplate();

    // load formations data
    LoadGroupGuids();

    // load members of the group
    LoadGroupMembers();

    // load creatures that have formation
    //LoadFormations();
}

template<> void FormationMgr::SetFormationSlot<Creature>(Creature* creature, Map* map)
{
    auto fDataItr = m_slotInfos.find(creature->GetGUIDLow());
    if (fDataItr != m_slotInfos.end())
    {
        sLog.outString("Setting formation slot for %s", creature->GetGuidStr().c_str());
        auto fData = map->GetFormationData(fDataItr->second->GetGroupTableEntry());
        fData->FillSlot(fDataItr->second, creature);
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

void FormationData::Disband()
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

void FormationData::FillSlot(FormationSlotInfoSPtr& slot, Creature* creature)
{
    SlotDataSPtr sData = nullptr;
    auto existingSlotItr = m_slotMap.find(slot->GetSlotId());
    if (existingSlotItr == m_slotMap.end())
    {
        sData = SlotDataSPtr(new SlotData(slot, creature, this));
        m_slotMap.emplace(slot->slotEntry->slotId, sData);
    }
    else
    {
        sData = existingSlotItr->second;
        sData->SetCreature(creature);
    }

    creature->SetFormationEntry(sData);
    creature->SetActiveObjectState(true);

    sLog.outString("Slot(%u) filled by %s in formation(%u)", slot->slotEntry->slotId, creature->GetGuidStr().c_str(), slot->slotEntry->formationEntry->formationId);

    uint32 lowGuid = creature->GetGUIDLow();

    if (slot->GetSlotId() == 0 && !m_realMaster)
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
    creature->SetFormationEntry(slot);

    if (oldSlot)
        oldSlot->SetCreature(creatureInNewSlot);

    if (creatureInNewSlot)
        creatureInNewSlot->SetFormationEntry(oldSlot);
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

SlotData::SlotData(FormationSlotInfoSPtr slot, Creature* _creature, FormationData* fData) :
    m_creature(_creature), m_formationData(fData), m_recomputePosition(false),
    m_angle(0), m_distance(0), m_formationId(slot->GetSlotId()), m_defaultGuid(slot->GetDefaultGuid())
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
