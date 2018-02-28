#ifndef AFINA_STORAGE_MAP_BASED_GLOBAL_LOCK_IMPL_H
#define AFINA_STORAGE_MAP_BASED_GLOBAL_LOCK_IMPL_H

#include <map>
#include <mutex>
#include <string>

#include <afina/Storage.h>
#include <unordered_map>

namespace Afina {
    namespace Backend {

/**
 * # Map based implementation with global lock
 *
 *
 */

        struct entry
        {
            entry* _next;
            entry* _prev;
            std::string _data;
            std::string _key;

            entry(std::string key, std::string data, entry* prev = nullptr, entry* next = nullptr)
                    :_key(key),
                     _data(data),
                     _prev(prev),
                     _next(next)
            {

            }

            entry()
                    :_key(""),
                     _data(""),
                     _prev(nullptr),
                     _next(nullptr)
            {}

        };

        class MapBasedGlobalLockImpl : public Afina::Storage {

        public:
            MapBasedGlobalLockImpl(size_t max_size = 1024) :
                    _max_size(max_size),
                    _size(0)
            {
                init_list();
            }
            ~MapBasedGlobalLockImpl()
            {
                // delete only entries not in hash_map
                delete _head;
                delete _tail;
            }

            // Implements Afina::Storage interface
            bool Put(const std::string &key, const std::string &value) override;

            // Implements Afina::Storage interface
            bool PutIfAbsent(const std::string &key, const std::string &value) override;

            // Implements Afina::Storage interface
            bool Set(const std::string &key, const std::string &value) override;

            // Implements Afina::Storage interface
            bool Delete(const std::string &key) override;

            // Implements Afina::Storage interface
            bool Get(const std::string &key, std::string &value) const override;

        private:
            mutable entry* _head;
            mutable entry* _tail;
            mutable size_t _max_size;
            mutable size_t _size;
            std::unordered_map<std::string, entry*> _backend;

            mutable std::recursive_mutex mutex;

            void to_head(entry* entry) const;
            void init_list() const
            {
                _head = new entry();
                _tail = new entry();
                _head -> _prev = _tail;
                _tail -> _next = _head;
            }
        };

    } // namespace Backend
} // namespace Afina

#endif // AFINA_STORAGE_MAP_BASED_GLOBAL_LOCK_IMPL_H