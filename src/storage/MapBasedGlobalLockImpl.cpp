#include "MapBasedGlobalLockImpl.h"

#include <mutex>

namespace Afina {
namespace Backend {

void MapBasedGlobalLockImpl::to_head(entry* entry) const
{
        std::unique_lock<std::recursive_mutex> lock(mutex);
        if(entry->_prev != nullptr)
             entry->_prev->_next = entry->_next;

        if(entry->_next != nullptr)
             entry->_next->_prev = entry->_prev;

        _head->_prev->_next = entry;
        entry -> _prev = _head->_prev;

       _head->_prev = entry;
        entry -> _next = _head;
    }

// See MapBasedGlobalLockImpl.h
bool MapBasedGlobalLockImpl::Put(const std::string &key, const std::string &value)
    {
        std::unique_lock<std::recursive_mutex> lock(mutex);

        size_t new_entry_size = key.size() + value.size();

        if(_backend.count(key))
        {
            entry *entry = _backend[key];
            entry->_data = value;
            to_head(entry);
            return true;
        }

       entry* new_entry = new entry(key, value);
       _backend[key] = new_entry;
        new_entry->it = _backend.find(key);


       if(_size >= _max_size && _tail->_next != nullptr)
       {
           entry* new_tail = _tail->_next -> _next;
           _backend.erase(_tail->_next ->it->first);
           delete _tail->_next;

           new_tail->_prev = _tail;
           _tail->_next = new_tail;

           to_head(new_entry);
           return true;
       }


        to_head(new_entry);
        _size++;

        return true;
    }

// See MapBasedGlobalLockImpl.h
bool MapBasedGlobalLockImpl::PutIfAbsent(const std::string &key, const std::string &value)
    {
        std::unique_lock<std::recursive_mutex> lock(mutex);
        if(_backend.count(key)) {
            return false;
        }

        this->Put(key, value);
    }

// See MapBasedGlobalLockImpl.h
bool MapBasedGlobalLockImpl::Set(const std::string &key, const std::string &value)
    {
        std::unique_lock<std::recursive_mutex> lock(mutex);
        if(_backend.count(key))
        {
            _backend[key]->_data = value;
            to_head(_backend[key]);
            return true;
        }
        return false;
    }

// See MapBasedGlobalLockImpl.h
bool MapBasedGlobalLockImpl::Delete(const std::string &key)
    {
        std::unique_lock<std::recursive_mutex> lock(mutex);
        if(_backend.count(key))
        {
            entry* entry_delete = _backend[key];
            entry_delete->_prev->_next = entry_delete->_next;
            entry_delete->_next->_prev=entry_delete->_prev;

            _backend.erase(entry_delete->it->first);
            _size--;
            return true;
        }
        return false;
    }

// See MapBasedGlobalLockImpl.h
bool MapBasedGlobalLockImpl::Get(const std::string &key, std::string &value) const
    {
        std::unique_lock<std::recursive_mutex> lock(mutex);
        if(_backend.count(key))
        {
            to_head(_backend.at(key));
            value =  _backend.at(key)->_data;
            return true;
        }
        return false;
    }

} // namespace Backend
} // namespace Afina
