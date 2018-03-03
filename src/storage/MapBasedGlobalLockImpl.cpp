#include "MapBasedGlobalLockImpl.h"

namespace Afina {
    namespace Backend {

        bool MapBasedGlobalLockImpl::_fit(size_t diff)
        {
            std::unique_lock<std::recursive_mutex> lock(mutex);
            if( diff > _max_size)
                return false;

            while (_size > _max_size - diff)
            {
                entry* new_tail = _tail->_next->_next;

                if( new_tail == _head)
                {
                    return false;
                }

                _size -= _tail->_next->size();
                _backend.erase(_tail->_next->_key);

                new_tail->_prev = _tail;
                _tail->_next = new_tail;
            }

            return true;
        }

        bool MapBasedGlobalLockImpl::_set(entry *entry_chng, const std::string &value)
        {
            std::unique_lock<std::recursive_mutex> lock(mutex);

            size_t old_val_size = entry_chng->_data.size();
            size_t new_val_size = value.size();

            if (entry_chng->_key.size() > _max_size - new_val_size) {
                return false;
            }

            while (_size - old_val_size  > _max_size - new_val_size)
            {
                entry* new_tail = _tail->_next->_next;

                if( new_tail == _head)
                {
                    return false;
                }

                _size -= _tail->_next->size();
                _backend.erase(_tail->_next->_key);

                new_tail->_prev = _tail;
                _tail->_next = new_tail;
            }


            entry_chng->_data = value;
            _size = _size - old_val_size + new_val_size;
            return true;
        }


        bool MapBasedGlobalLockImpl::_put(const std::string &key, const std::string &value, bool put_if_abscent_flag) {
            std::unique_lock<std::recursive_mutex> lock(mutex);

            if(key.size() + value.size()> _max_size)
            {
                throw std::exception();
            }

            auto entry_iter = _backend.find(key);

            if (entry_iter != _backend.end())
            {
                if (put_if_abscent_flag) {
                    return false;
                }

                entry *cur_entry = entry_iter->second;

                if(!_set(cur_entry, value))
                {
                    return false;
                }

                to_head(cur_entry);
                return true;
            }


            entry* new_entry = new entry(key, value, this->_is_size_binary);
            if(!_fit( new_entry->size()))
            {
                delete new_entry;
                return false;
            }

            _backend[new_entry->_key] = new_entry;
            _size += new_entry->size();
            to_head(new_entry);

            return true;
        }


        void MapBasedGlobalLockImpl::to_head(entry *entry) const {
            std::unique_lock<std::recursive_mutex> lock(mutex);
            if (entry->_prev != nullptr)
                entry->_prev->_next = entry->_next;

            if (entry->_next != nullptr)
                entry->_next->_prev = entry->_prev;

            _head->_prev->_next = entry;
            entry->_prev = _head->_prev;

            _head->_prev = entry;
            entry->_next = _head;
        }

// See MapBasedGlobalLockImpl.h
        bool MapBasedGlobalLockImpl::Put(const std::string &key, const std::string &value) {
            std::unique_lock<std::recursive_mutex> lock(mutex);
            return _put(key, value, false);
        }

// See MapBasedGlobalLockImpl.h
        bool MapBasedGlobalLockImpl::PutIfAbsent(const std::string &key, const std::string &value) {
            std::unique_lock<std::recursive_mutex> lock(mutex);
            return _put(key, value, true);
        }

// See MapBasedGlobalLockImpl.h
        bool MapBasedGlobalLockImpl::Set(const std::string &key, const std::string &value){
            std::unique_lock<std::recursive_mutex> lock(mutex);

            auto entry_it = _backend.find(key);

            if(entry_it != _backend.end())
            {
                if(_set(entry_it->second, value))
                {
                    to_head(entry_it->second);
                    return true;
                }
            }

            return false;
        }

// See MapBasedGlobalLockImpl.h
        bool MapBasedGlobalLockImpl::Delete(const std::string &key) {
            std::unique_lock<std::recursive_mutex> lock(mutex);

            auto entry_it = _backend.find(key);

            if (entry_it != _backend.end())
            {
                entry *entry_delete = entry_it->second;
                entry_delete->_prev->_next = entry_delete->_next;
                entry_delete->_next->_prev = entry_delete->_prev;

                _size-= entry_delete->size();
                _backend.erase(entry_delete->_key);

                return true;
            }
            return false;
        }

// See MapBasedGlobalLockImpl.h
        bool MapBasedGlobalLockImpl::Get(const std::string &key, std::string &value) const {
            std::unique_lock<std::recursive_mutex> lock(mutex);
            if (_backend.find(key) != _backend.end()) {
                to_head(_backend.at(key));
                value = _backend.at(key)->_data;
                return true;
            }
            return false;
        }



} // namespace Backend
} // namespace Afina
