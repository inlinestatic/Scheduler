/*
 *  Copyright (c) 2021, Aleksandr Don
 *  All rights reserved.
 *
 *  This source code is licensed under the BSD-style license found in the
 *  LICENSE file in the root directory of this source tree. An additional grant
 *  of patent rights can be found in the PATENTS file in the same directory.
 */

#pragma once
#include "guard.h"

bool cascade_mutex::CheckLocksOrder()
{
    bool result = true;
    for (auto item : MutexID_Map)
    {
        if (item.first > MutexID)
            result = false;
    }
    return result;
}

cascade_mutex::cascade_mutex(short UniqueID) :std::shared_mutex()
{
    MutexID = UniqueID;
}

void cascade_mutex::lock_unique()
{
    size_t this_id = std::hash<std::thread::id>{}(std::this_thread::get_id());
    MutexID_Guard.lock();
    bool orderPreserved = CheckLocksOrder();
    if (orderPreserved)
    {
        if (MutexMode_Map.count(MutexID) > 0 && !MutexMode_Map[MutexID])
        {
            MutexID_Guard.unlock();
            throw "Attempt to lock get writing lock at time of reading on the same thread";
        }
        if (owner == this_id)// Lock belongs to current thread
        {
            MutexID_Map[MutexID]++;//Lock is retaken, just increase counter
        }
        else
        {
            MutexID_Guard.unlock();
            shared_mutex::lock(); // cannot be taken untill all threads release shared mutex
            //Protect map
            MutexID_Guard.lock();
            owner = this_id;
            MutexID_Map[MutexID] = 1;
            MutexMode_Map[MutexID] = true; //Unique
        }
        MutexID_Guard.unlock();
    }
    else
    {
        MutexID_Guard.unlock();
        throw "Attempt to take a lock outside of order, potential deadlock";
    }
}

void cascade_mutex::lock_shared()
{
    size_t this_id = std::hash<std::thread::id>{}(std::this_thread::get_id());
    MutexID_Guard.lock();
    bool orderPreserved = CheckLocksOrder();
    if (orderPreserved)
    {
        if (owner == this_id)// Lock belongs to current thread
        {
            MutexID_Map[MutexID]++;//Lock is retaken, just increase counter
        }
        else
        {
            MutexID_Guard.unlock();
            shared_mutex::lock_shared();
            MutexID_Guard.lock();
            owner = this_id;
            MutexID_Map[MutexID] = 1;
            MutexMode_Map[MutexID] = false; //Shared
        }
        MutexID_Guard.unlock();
    }
    else
    {
        MutexID_Guard.unlock();
        throw "Attempt to take a lock outside of order, potential deadlock";
    }
}

void cascade_mutex::unlock()
{
    size_t this_id = std::hash<std::thread::id>{}(std::this_thread::get_id());
    if (owner == this_id)// Lock belongs to current thread
    {
        MutexID_Guard.lock();
        bool exist = MutexMode_Map.count(MutexID);
        if (exist && MutexID_Map[MutexID] > 0)
            MutexID_Map[MutexID]--;

        if (exist && MutexID_Map[MutexID] == 0)
        {
            owner = 0;
            bool mode = MutexMode_Map[MutexID];
            MutexID_Guard.unlock();
            if(mode)
                shared_mutex::unlock();
            else
                shared_mutex::unlock_shared();
        }
        else
            MutexID_Guard.unlock();
    }
}