#ifndef POLYMORPHIC_VALUE_INCLUDE_H
#define POLYMORPHIC_VALUE_INCLUDE_H

#include <array>
#include <cstddef>
#include <iostream>
#include <new>
#include <type_traits>
#include <utility>

#ifdef __cpp_rtti
#include <stdexcept>
#define POLYMORPHIC_VALUE_RTTI_SUPPORTED true
#else
#define POLYMORPHIC_VALUE_RTTI_SUPPORTED false
#endif

namespace pmv {
namespace detail {

// All these function pointers assume that the source and destination objects
// are the same type and that their storage type is the same.
struct polymorphic_value_vtable {
    // Call destructor and free storage
    void (*destroy)(void* ptr);
    // Copy from another storage into an empty storage
    void (*copy)(void const* src, void* dst);
    // Copy from a non managed object into an empty storage
    void (*copy_raw)(void const* src, void* dst);
    // Move from another storage into an empty storage
    void (*move)(void* src, void* dst) noexcept;
    // Move from a non managed object into an empty storage
    void (*move_raw)(void* src, void* dst) noexcept;
    // Copy from another storage into a storage containing an object
    void (*copy_assign)(void const* src, void* dst);
    // Copy from a non managed object into a storage containing an object
    void (*copy_assign_raw)(void const* src, void* dst);
    // Move from another storage into a storage containing an object
    void (*move_assign)(void* src, void* dst) noexcept;
    // Move from a non managed objet into a storage containing an object
    void (*move_assign_raw)(void* src, void* dst) noexcept;
};

template<typename Derived, std::size_t SboSize, std::size_t SboAlignment>
constexpr static auto store_in_heap
    = !std::is_nothrow_move_constructible<std::decay_t<Derived>>::value || sizeof(Derived) > SboSize
    || alignof(Derived) > SboAlignment;

template<std::size_t SboSize, std::size_t SboAlignment>
struct sbo_storage {
    constexpr static auto sbo_size = SboSize;
    constexpr static auto sbo_alignment = SboAlignment;

    template<typename Derived, typename... Args>
    void build(Args&&... args) noexcept(!store_in_heap<Derived, sbo_size, sbo_alignment>&& noexcept(
        Derived{std::forward<Args>(args)...}))
    {
        if (store_in_heap<Derived, sbo_size, sbo_alignment>) {
            this->heap_buffer = new Derived{std::forward<Args>(args)...};
        } else {
            new (this->local_buffer.data()) Derived{std::forward<Args>(args)...};
        }
    }

    union {
        void* heap_buffer;
        alignas(SboAlignment) std::array<char, SboSize> local_buffer;
    };
};

namespace local_storage {

// Local storage doesn't need different "raw_" prefixed functions.

template<typename Derived>
inline void destroy(void* ptr)
{
    static_cast<Derived*>(ptr)->~Derived();
}

template<typename Derived>
inline void copy(void const* src, void* dst)
{
    new (dst) Derived{*static_cast<Derived const*>(src)};
}

template<typename Derived>
inline void move(void* src, void* dst) noexcept
{
    new (dst) Derived{std::move(*static_cast<Derived*>(src))};
}

template<typename Derived>
inline void copy_assign(void const* src, void* dst)
{
    *static_cast<Derived*>(dst) = *static_cast<Derived const*>(src);
}

template<typename Derived>
inline void move_assign(void* src, void* dst) noexcept
{
    *static_cast<Derived*>(dst) = std::move(*static_cast<Derived*>(src));
}

} // namespace local_storage

namespace heap_storage {

template<typename Derived>
inline void destroy(void* ptr)
{
    delete *static_cast<Derived**>(ptr);
}

template<typename Derived>
inline void copy(void const* src, void* dst)
{
    *static_cast<Derived**>(dst) = new Derived{**static_cast<Derived const* const*>(src)};
}

template<typename Derived>
inline void copy_raw(void const* src, void* dst)
{
    *static_cast<Derived**>(dst) = new Derived{*static_cast<Derived const*>(src)};
}

inline void move(void* src, void* dst) noexcept
{
    *static_cast<void**>(dst) = *static_cast<void**>(src);
    *static_cast<void**>(src) = nullptr;
}

template<typename Derived>
inline void move_raw(void* src, void* dst) noexcept
{
    *static_cast<Derived**>(dst) = new Derived{std::move(*static_cast<Derived*>(src))};
}

template<typename Derived>
inline void copy_assign(void const* src, void* dst)
{
    **static_cast<Derived**>(dst) = **static_cast<Derived const* const*>(src);
}

template<typename Derived>
inline void copy_assign_raw(void const* src, void* dst)
{
    **static_cast<Derived**>(dst) = *static_cast<Derived const*>(src);
}

inline void move_assign(void* src, void* dst) noexcept
{
    std::swap(*static_cast<void**>(src), *static_cast<void**>(dst));
}

template<typename Derived>
inline void move_assign_raw(void* src, void* dst) noexcept
{
    **static_cast<Derived**>(dst) = std::move(*static_cast<Derived*>(src));
}

} // namespace heap_storage

template<typename Derived,
         typename Storage,
         bool InHeap = store_in_heap<Derived, Storage::sbo_size, Storage::sbo_alignment>>
struct get_vtable;

// vtable for local (SBO) storage
template<typename Derived, typename Storage>
struct get_vtable<Derived, Storage, false> {
    static polymorphic_value_vtable const* get() noexcept
    {
        alignas(alignof(void*) * 2) static const polymorphic_value_vtable static_vtable{
            local_storage::destroy<Derived>,
            local_storage::copy<Derived>,
            local_storage::copy<Derived>,
            local_storage::move<Derived>,
            local_storage::move<Derived>,
            local_storage::copy_assign<Derived>,
            local_storage::copy_assign<Derived>,
            local_storage::move_assign<Derived>,
            local_storage::move_assign<Derived>,
        };

        return &static_vtable;
    }
};

// vtable for heap storage
template<typename Derived, typename Storage>
struct get_vtable<Derived, Storage, true> {
    static polymorphic_value_vtable const* get() noexcept
    {
        // Struct to align the vtable itself to odd alignof(void*) addresses. This
        // is accomplished by aligning the whole structure to (alignof(void*) * 2)
        // and putting a dummy void* in front of it.
        struct odd_aligned_vtable {
            void* dummy;
            polymorphic_value_vtable vtable;
        };

        alignas(alignof(void*) * 2) static const odd_aligned_vtable static_vtable{
            {},
            {
                heap_storage::destroy<Derived>,
                heap_storage::copy<Derived>,
                heap_storage::copy_raw<Derived>,
                heap_storage::move,
                heap_storage::move_raw<Derived>,
                heap_storage::copy_assign<Derived>,
                heap_storage::copy_assign_raw<Derived>,
                heap_storage::move_assign,
                heap_storage::move_assign_raw<Derived>,
            }};

        // Ensure that the address of the returned object is aligned to odd
        // alignof(void*) addresses, as we rely on that to signal a "heap stored"
        // value.
        static_assert(offsetof(odd_aligned_vtable, vtable) == alignof(void*),
                      "Bug: misaligned vtable");

        return &static_vtable.vtable;
    }
};

} // namespace detail

#if __cplusplus < 201703L

template<typename T>
struct in_place_type_t {
    explicit in_place_type_t() = default;
};

template<typename T>
constexpr in_place_type_t<T> in_place_type{};

#else

template<typename T>
using in_place_type_t = std::in_place_type_t<T>;

template<typename T>
constexpr std::in_place_type_t<T> in_place_type{};

#endif

#if POLYMORPHIC_VALUE_RTTI_SUPPORTED
struct bad_polymorphic_value : public std::logic_error {
    using logic_error::logic_error;
};
#endif

namespace detail {

template<typename Base, bool AllowAllocations, std::size_t SboSize, std::size_t SboAlignment>
class polymorphic_value_impl {
    static_assert(std::is_polymorphic<Base>::value, "Class is not polymorphic");

    using storage_t = detail::sbo_storage<SboSize, SboAlignment>;

public:
    template<typename Derived,
             std::enable_if_t<std::is_base_of<Base, std::decay_t<Derived>>::value, bool> = true>
    polymorphic_value_impl(Derived&& d) noexcept(!POLYMORPHIC_VALUE_RTTI_SUPPORTED&& noexcept(
        m_storage.template build<std::decay_t<Derived>>(std::forward<Derived>(d))))
    {
        static_assert(AllowAllocations || !detail::store_in_heap<Derived, SboSize, SboAlignment>,
                      "Allocations are not allowed");
#if POLYMORPHIC_VALUE_RTTI_SUPPORTED
        if (typeid(std::decay_t<Derived>) != typeid(d)) {
            throw bad_polymorphic_value{"Value would slice"};
        }
#endif
        m_vtable = detail::get_vtable<std::decay_t<Derived>, storage_t>::get();
        m_storage.template build<std::decay_t<Derived>>(std::forward<Derived>(d));
    }

    template<typename Derived,
             typename... Args,
             std::enable_if_t<std::is_base_of<Base, std::decay_t<Derived>>::value, bool> = true>
    explicit polymorphic_value_impl(in_place_type_t<Derived>, Args&&... args) noexcept(
        noexcept(m_storage.template build<std::decay_t<Derived>>(std::forward<Args>(args)...)))
    {
        static_assert(AllowAllocations || !detail::store_in_heap<Derived, SboSize, SboAlignment>,
                      "Allocations are not allowed");
        m_vtable = detail::get_vtable<std::decay_t<Derived>, storage_t>::get();
        m_storage.template build<std::decay_t<Derived>>(std::forward<Args>(args)...);
    }

    polymorphic_value_impl(polymorphic_value_impl const& src) : m_vtable{src.m_vtable}
    {
        m_vtable->copy(&src.m_storage, &m_storage);
    }

    polymorphic_value_impl(polymorphic_value_impl&& src) noexcept : m_vtable{src.m_vtable}
    {
        m_vtable->move(&src.m_storage, &m_storage);
    }

    polymorphic_value_impl& operator=(polymorphic_value_impl const& src)
    {
        if (&src == this) {
            return *this;
        }

        if (m_vtable == src.m_vtable) {
            m_vtable->copy_assign(&src.m_storage, &m_storage);
        } else {
            m_vtable->destroy(&m_storage);
            m_vtable = src.m_vtable;
            m_vtable->copy(&src.m_storage, &m_storage);
        }

        return *this;
    }

    polymorphic_value_impl& operator=(polymorphic_value_impl&& src) noexcept
    {
        if (&src == this) {
            return *this;
        }

        if (m_vtable == src.m_vtable) {
            m_vtable->move_assign(&src.m_storage, &m_storage);
        } else {
            m_vtable->destroy(&m_storage);
            m_vtable = src.m_vtable;
            m_vtable->move(&src.m_storage, &m_storage);
        }

        return *this;
    }

    template<typename Derived, std::enable_if_t<std::is_base_of<Base, Derived>::value, bool> = true>
    polymorphic_value_impl& operator=(Derived const& src)
    {
        static_assert(AllowAllocations || !detail::store_in_heap<Derived, SboSize, SboAlignment>,
                      "Allocations are not allowed");
#if POLYMORPHIC_VALUE_RTTI_SUPPORTED
        if (typeid(std::decay_t<Derived>) != typeid(src)) {
            throw bad_polymorphic_value{"Value would slice"};
        }
#endif
        auto* new_vtable = detail::get_vtable<Derived, storage_t>::get();

        if (m_vtable == new_vtable) {
            m_vtable->copy_assign_raw(&src, &m_storage);
        } else {
            m_vtable->destroy(&m_storage);
            m_vtable = new_vtable;
            m_vtable->copy_raw(&src, &m_storage);
        }

        return *this;
    }

    template<typename Derived,
             std::enable_if_t<std::is_base_of<Base, Derived>::value
                                  && !std::is_lvalue_reference<Derived>::value,
                              bool> = true>
    polymorphic_value_impl& operator=(Derived&& src)
    {
        static_assert(AllowAllocations || !detail::store_in_heap<Derived, SboSize, SboAlignment>,
                      "Allocations are not allowed");
#if POLYMORPHIC_VALUE_RTTI_SUPPORTED
        if (typeid(std::decay_t<Derived>) != typeid(src)) {
            throw bad_polymorphic_value{"Value would slice"};
        }
#endif
        auto* new_vtable = detail::get_vtable<Derived, storage_t>::get();

        if (m_vtable == new_vtable) {
            m_vtable->move_assign_raw(&src, &m_storage);
        } else {
            m_vtable->destroy(&m_storage);
            m_vtable = new_vtable;
            m_vtable->move_raw(&src, &m_storage);
        }

        return *this;
    }

    template<typename Derived, typename... Args>
    std::enable_if_t<std::is_base_of<Base, std::decay_t<Derived>>::value>
    emplace(Args&&... args) noexcept(
        noexcept(m_storage.template build<std::decay_t<Derived>>(std::forward<Args>(args)...)))
    {
        static_assert(AllowAllocations || !detail::store_in_heap<Derived, SboSize, SboAlignment>,
                      "Allocations are not allowed");
        m_vtable->destroy(&m_storage);
        m_vtable = detail::get_vtable<std::decay_t<Derived>, storage_t>::get();
        m_storage.template build<std::decay_t<Derived>>(std::forward<Args>(args)...);
    }

    ~polymorphic_value_impl() { m_vtable->destroy(&m_storage); }

    Base* operator->() noexcept { return get(); }
    Base const* operator->() const noexcept { return get(); }
    Base& operator*() noexcept { return *get(); }
    Base const& operator*() const noexcept { return *get(); }

private:
    bool storage_is_local() const noexcept
    {
        // Trick: local storage vtables are aligned to even alignof(void*)
        // addresses, while heap storage vtables are aligned to odd alignof(void*)
        // addresses.
        return reinterpret_cast<std::uintptr_t>(m_vtable) % (alignof(void*) * 2) == 0;
    }

    Base* get() noexcept
    {
        if (storage_is_local()) {
            return reinterpret_cast<Base*>(&m_storage.local_buffer);
        } else {
            return static_cast<Base*>(m_storage.heap_buffer);
        }
    }

    Base const* get() const noexcept
    {
        if (storage_is_local()) {
            return reinterpret_cast<Base const*>(&m_storage.local_buffer);
        } else {
            return static_cast<Base const*>(m_storage.heap_buffer);
        }
    }

    storage_t m_storage;
    detail::polymorphic_value_vtable const* m_vtable;
};

} // detail

template<typename Base,
         bool AllowAllocations = true,
         std::size_t SboSize = sizeof(void*) * 3,
         std::size_t SboAlignment = alignof(void*)>
using polymorphic_value = detail::polymorphic_value_impl<Base,
                                                         AllowAllocations,
                                                         std::max(SboSize, sizeof(void*)),
                                                         std::max(SboAlignment, alignof(void*))>;

} // pmv

#endif // POLYMORPHIC_VALUE_INCLUDE_H
