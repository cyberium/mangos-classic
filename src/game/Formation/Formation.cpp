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
    sLog.outString("Loading formation_templates...");

    uint32 count = 0;

    // fields indexes                                0        1      2         3
    const char* sqlFTRequest = "SELECT formation_entry, slot_id, angle, distance from group_formation_template";
    std::unique_ptr<QueryResult> formationTemplateQR(WorldDatabase.Query(sqlFTRequest));
    if (formationTemplateQR)
    {
        do
        {
            Field* fields = formationTemplateQR->Fetch();

            uint32 formId = fields[0].GetUInt32();
            uint32 slotId = fields[1].GetUInt32();
            float angle = (fields[2].GetUInt32() * M_PI_F) / 180;
            float distance = fields[3].GetFloat();

            FormationEntrySPtr fEntry = nullptr;
            auto formationItr = m_formationEntries.find(formId);
            if (formationItr == m_formationEntries.end())
            {
                m_formationEntries.emplace(formId, new FormationEntry());
                fEntry = m_formationEntries[formId];
                fEntry->formationId = formId;
                count++;
            }
            else
                fEntry = formationItr->second;
            fEntry->slots.emplace(slotId, new FormationSlotEntry(slotId, angle, distance, fEntry));

        } while (formationTemplateQR->NextRow());
    }

    sLog.outString(">> Loaded %u group_template data", count);
    sLog.outString();

    sLog.outString("Loading group_template...");
    count = 0;

    // fields indexes                            0     1                2
    const char* sqlGTRequest = "SELECT group_entry, name, formation_entry from group_template";
    std::unique_ptr<QueryResult> groupTemplateQR(WorldDatabase.Query(sqlGTRequest));
    if (groupTemplateQR)
    {
        do
        {
            Field* fields = groupTemplateQR->Fetch();

            uint32 groupId = fields[0].GetUInt32();
            std::string gName = fields[1].GetString();
            uint32 fId = fields[2].GetUInt32();

            auto fEntryItr = m_formationEntries.find(fId);
            if (fEntryItr == m_formationEntries.end())
            {
                sLog.outErrorDb("Error in table group_template group_entry(%u) use inexistent formation_entry(%u)", groupId, fId);
                continue;
            }

            m_groupTemplateEntries.emplace(groupId, new GroupTemplateEntry(groupId, gName, fEntryItr->second));
            count++;
        } while (groupTemplateQR->NextRow());
    }

    sLog.outString(">> Loaded %u group template", count);
    sLog.outString();
}

void FormationMgr::LoadCreaturesFormation()
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


    sLog.outString("Loading groups...");

    // fields indexes                         0            1
    const char* sqlGrpRequest = "SELECT group_guid, group_entry from group_guid";

    count = 0;
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

            auto& groupTemplateEntry = groupTemplateEntryItr->second;
            if (groupTemplateEntry->formationEntry == nullptr)
            {
                sLog.outErrorDb("group_template(%u) referenced in table groups(%u) have no formation data in group_formation_template table", groupEntryId, groupGuid);
                continue;
            }

            auto masterSlotItr = groupTemplateEntry->formationEntry->slots.find(0);
            if (masterSlotItr == groupTemplateEntry->formationEntry->slots.end())
            {
                sLog.outErrorDb("formation(%u) referenced in table groups_template(%u) have no master in group_formation_template table",
                    groupTemplateEntry->formationEntry->formationId, groupGuid);
                continue;
            }

            auto bounds = fContainer.equal_range(groupGuid);
            if (std::distance(bounds.first, bounds.second) == 0)
            {
                sLog.outErrorDb("group_template(%u) referenced in table groups(%u) have no data in group_member table", groupEntryId, groupGuid);
                continue;
            }

            GroupsTableEntrySPtr gEntry = nullptr;
            auto groupEntryItr = m_groupsData.find(groupGuid);
            if (groupEntryItr == m_groupsData.end())
            {
                gEntry = GroupsTableEntrySPtr(new GroupsTableEntry(groupGuid, groupTemplateEntry));
                m_groupsData.emplace(groupGuid, gEntry);
            }
            else
                gEntry = groupEntryItr->second;

            FormationSlotInfoMap tempSlotInfoMap;
            bool foundMasterSlot = false;
            uint32 dist = 0;
            for (auto itr = bounds.first; itr != bounds.second; ++itr)
            {
                auto& memberGuid = std::get<0>(itr->second);
                auto& slotId = std::get<1>(itr->second);
                auto slotEntryItr = groupTemplateEntry->formationEntry->slots.find(slotId);

                if (slotId == 0)
                {
                    foundMasterSlot = true;
                    dist = slotEntryItr->second->distance;
                }

                if (slotEntryItr != groupTemplateEntry->formationEntry->slots.end())
                {
                    if (slotId == 0)
                    {
                        foundMasterSlot = true;
                        dist = slotEntryItr->second->distance;
                    }

                    tempSlotInfoMap.emplace(memberGuid, new FormationSlotInfo(memberGuid, slotEntryItr->second, gEntry));
                }
                else
                {
                    if (groupTemplateEntry->formationEntry->formationId >= 10 && groupTemplateEntry->formationEntry->formationId < 17)
                    {
                        // we should create and empty slot with only distance set
                        groupTemplateEntry->formationEntry->slots.emplace(slotId, new FormationSlotEntry(slotId, 0, dist, groupTemplateEntry->formationEntry));
                        slotEntryItr = groupTemplateEntry->formationEntry->slots.find(slotId);
                        tempSlotInfoMap.emplace(memberGuid, new FormationSlotInfo(memberGuid, slotEntryItr->second, gEntry));
                    }
                    else
                        sLog.outErrorDb("Error in table group_member. Slot(%u) is not defined in group_formation_template for guid(%u)", slotId, memberGuid);
                }
            }

            if (foundMasterSlot)
            {
                for (auto itr : tempSlotInfoMap)
                    m_slotInfos.emplace(itr);
            }
            else
                sLog.outErrorDb("Error in table group_member. MasterSlot(0) is not defined for group guid(%u) skipping...", groupGuid);

            ++count;
        } while (groupQR->NextRow());
    }

    sLog.outString(">> Loaded %u groups definitions", count);
    sLog.outString();
}

void FormationMgr::Initialize()
{
    // load group templates and formations data
    LoadGroupTemplate();

    // load creatures that have formation
    LoadCreaturesFormation();
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
    return SwitchFormation(sFormationMgr.GetFormationEntry(fId));
}

bool FormationData::SwitchFormation(FormationEntrySPtr fEntry)
{
    if (!fEntry)
        return false;

    if (fEntry->slots.size() != m_slotMap.size())
        return false;

    if (fEntry->formationId == GetFormationId())
        return false;

    auto slotItr = m_slotMap.begin();
    auto newSlotEntryItr = fEntry->slots.begin();
    while (slotItr != m_slotMap.end())
    {
        auto& slot = slotItr->second;
        Creature* creature = slot->GetCreature();
        slot->SetCreature(nullptr);
        auto slotInfo = FormationSlotInfoSPtr(new FormationSlotInfo(slot->GetDefaultGuid(), newSlotEntryItr->second, slot->GetGroupTableEntry()));
        SlotDataSPtr newSlot = SlotDataSPtr(new SlotData(slotInfo, creature, slot->GetFormationData()));
        slot = newSlot;
        if (creature)
        {
            creature->SetFormationEntry(newSlot);
            slot->SetNewPositionRequired();
        }

        ++slotItr;
        ++newSlotEntryItr;
    }

    m_currentFormationEntry = fEntry;

    for (auto& slotItr : m_slotMap)
    {
        auto& slot = slotItr.second;

        Creature* slotCreature = slot->GetCreature();
        if (slotCreature)
        {
            sLog.outString("%s switch %u", slotCreature->GetGuidStr().c_str(), slot->GetFormationId());
        }
    }

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
    // fixup formation slot data
    if (slot->GetFormationId() >= 10 && slot->GetFormationId() < 17) // TODO need to be removed
    {
        if (slot->GetFormationId() == 11)
        {
            if (slot->GetSlotId() != 0)
            {
                //slot->slotEntry->angle = (angle * M_PI_F) / 180;
                slot->slotEntry->angle = M_PI_F;
                slot->slotEntry->distance = GetMaster()->GetFormationSlot()->GetDistance() * slot->GetSlotId();
            }
        }
        else if (slot->GetFormationId() == 12)
        {
            if (slot->GetSlotId() != 0)
            {
                //slot->slotEntry->angle = (angle * M_PI_F) / 180;
                if ((slot->GetSlotId() & 1) == 0)
                    slot->slotEntry->angle = M_PI_F / 2.0f;
                else
                    slot->slotEntry->angle = (M_PI_F / 2.0f) + M_PI_F;
                slot->slotEntry->distance = GetMaster()->GetFormationSlot()->GetDistance() * (((slot->GetSlotId() - 1)/ 2) + 1);
            }
        }
        else if (slot->GetFormationId() == 13)
        {
            if (slot->GetSlotId() != 0)
            {
                //slot->slotEntry->angle = (angle * M_PI_F) / 180;
                if ((slot->GetSlotId() & 1) == 0)
                    slot->slotEntry->angle = M_PI_F + (M_PI_F / 4.0f);
                else
                    slot->slotEntry->angle = M_PI_F - (M_PI_F / 3.0f);
                slot->slotEntry->distance = GetMaster()->GetFormationSlot()->GetDistance() * (((slot->GetSlotId() - 1) / 2) + 1);
            }
        }
        else if (slot->GetFormationId() == 14)
        {
            if (slot->GetSlotId() != 0)
            {
                slot->slotEntry->angle = (M_PI_F / 2.0f) + (M_PI_F / float(slot->slotEntry->formationEntry->slots.size() - 1)) * (slot->GetSlotId() - 1);
            }
        }
        else if (slot->GetFormationId() == 15)
        {
            if (slot->GetSlotId() != 0)
            {
                slot->slotEntry->angle = M_PI_F + (M_PI_F / 2.0f) + (M_PI_F / float(slot->slotEntry->formationEntry->slots.size() - 1)) * (slot->GetSlotId() - 1);
                if (slot->slotEntry->angle > M_PI_F * 2.0f)
                    slot->slotEntry->angle = slot->slotEntry->angle - M_PI_F * 2.0f;
            }
        }
        else if (slot->GetFormationId() == 16)
        {
            if (slot->GetSlotId() != 0)
            {
                slot->slotEntry->angle = ((M_PI_F * 2.0f) / float(slot->slotEntry->formationEntry->slots.size() - 1)) * (slot->GetSlotId() - 1);
            }
        }
    }

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

    SwitchFormation(m_groupTableEntry->groupTemplateEntry->formationEntry);
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

SlotData::SlotData(FormationSlotInfoSPtr slot, Creature* _creature, FormationData* fData) :
    m_creature(_creature), m_slot(slot), m_formationData(fData), m_recomputePosition(false)
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
        return m_slot->GetAngle();

    return (2 * M_PI_F) - m_slot->GetAngle();
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
