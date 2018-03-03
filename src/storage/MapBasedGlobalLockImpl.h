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

        class MapBasedGlobalLockImpl : public Afina::Storage {

            struct entry
            {
                entry* _next;
                entry* _prev;
                std::string _data;
                std::string _key;
                bool _is_size_binary = false;

                size_t size()
                {
                    if(_is_size_binary && (_data.size() + _key.size() > 0))
                        return 1;

                    return _data.size() + _key.size();
                }

                entry(std::string key, std::string data = "", bool is_size_binary = false,
                      entry* prev = nullptr, entry* next = nullptr):
                        _key(key),
                        _data(data),
                        _prev(prev),
                        _next(next),
                        _is_size_binary(is_size_binary)

                {

                }

                entry():
                        _key(""),
                        _data(""),
                        _prev(nullptr),
                        _next(nullptr)
                {}

            };

        public:
            MapBasedGlobalLockImpl(size_t max_size = 1024, bool is_size_binary = false) :
                    _max_size(max_size),
                    _size(0),
                    _is_size_binary(is_size_binary)

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
            mutable bool _is_size_binary;

            typedef std::reference_wrapper<const std::string> string_ref;
            std::unordered_map<string_ref, entry*,
                    std::hash<std::string>,
                    std::equal_to<std::string>> _backend;

            mutable std::recursive_mutex mutex;

            bool _put(const std::string &key, const std::string &value, bool put_if_abscent_flag);

            bool _fit(size_t new_size);

            void to_head(entry* entry) const;

            bool _set(entry *chng_entry, const std::string &value);

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