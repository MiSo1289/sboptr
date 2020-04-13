#pragma once

#include <cstddef>
#include <memory>
#include <utility>
#include <type_traits>

namespace sboptr {

namespace detail {

template <typename Base, bool enable_move, bool enable_copy, bool enable_heap>
struct sbo_ptr_vtable;

struct sbo_ptr_vtable_heap_base {
    bool on_heap;
};

template <typename Base>
struct sbo_ptr_vtable_move_base {
    Base* (*move)(void*, void*);

    template <typename Derived>
    static constexpr auto create() noexcept -> sbo_ptr_vtable_move_base {
        static_assert(std::is_nothrow_move_constructible_v<Derived>);
        return {
            [](void* from, void* to) -> Base* {
                auto& old_object = *reinterpret_cast<Derived*>(from);
                auto* const new_ptr = new(to) Derived(std::move(old_object));
                old_object.~Derived();
                return new_ptr;
            },
        };
    }
};

template <typename Base>
struct sbo_ptr_vtable_copy_base {
    Base* (*copy)(void const*, void*);

    template <typename Derived>
    static constexpr auto create() noexcept -> sbo_ptr_vtable_copy_base {
        return {
            [](void const* from, void* to) -> Base* {
                auto const& old_object = *reinterpret_cast<Derived const*>(from);
                return new(to) Derived(old_object);
            },
        };
    }
};

template <typename Base>
struct sbo_ptr_vtable_heap_copy_base {
    Base* (*heap_copy)(Base const*);

    template <typename Derived>
    static constexpr auto create() noexcept -> sbo_ptr_vtable_heap_copy_base {
        return {
            [](Base const* from) -> Base* {
                return new Derived{*static_cast<Derived const*>(from)};
            },
        };
    }
};

template <typename Base>
struct sbo_ptr_vtable<Base, true, false, false>
: sbo_ptr_vtable_move_base<Base> {
    template <typename Derived>
    static auto get() noexcept -> sbo_ptr_vtable const* {
        static constexpr auto vtable = sbo_ptr_vtable{
            sbo_ptr_vtable_move_base<Base>::template create<Derived>(),
        };
        return &vtable;
    }
};

template <typename Base>
struct sbo_ptr_vtable<Base, true, true, false>
: sbo_ptr_vtable_copy_base<Base>
, sbo_ptr_vtable_move_base<Base> {
    template <typename Derived>
    static auto get() noexcept -> sbo_ptr_vtable const* {
        static constexpr auto vtable = sbo_ptr_vtable{
            sbo_ptr_vtable_copy_base<Base>::template create<Derived>(),
            sbo_ptr_vtable_move_base<Base>::template create<Derived>(),
        };
        return &vtable;
    }
};

template <typename Base>
struct sbo_ptr_vtable<Base, true, false, true>
: sbo_ptr_vtable_move_base<Base>
, sbo_ptr_vtable_heap_base {
    template <typename Derived, bool on_heap>
    static auto get() noexcept -> sbo_ptr_vtable const* {
        static constexpr auto vtable = sbo_ptr_vtable{
            sbo_ptr_vtable_move_base<Base>::template create<Derived>(),
            {on_heap},
        };
        return &vtable;
    }
};

template <typename Base>
struct sbo_ptr_vtable<Base, true, true, true>
  : sbo_ptr_vtable_copy_base<Base>
  , sbo_ptr_vtable_move_base<Base>
  , sbo_ptr_vtable_heap_base
  , sbo_ptr_vtable_heap_copy_base<Base> {
    template <typename Derived, bool on_heap>
    static auto get() noexcept -> sbo_ptr_vtable const* {
        static constexpr auto vtable = sbo_ptr_vtable{
            sbo_ptr_vtable_copy_base<Base>::template create<Derived>(),
            sbo_ptr_vtable_move_base<Base>::template create<Derived>(),
            {on_heap},
            sbo_ptr_vtable_heap_copy_base<Base>::template create<Derived>(),
        };
        return &vtable;
    }
};

template <typename Base, std::size_t sbo_size, bool enable_move, bool enable_copy, bool enable_heap>
class sbo_ptr_base;

template <typename Base, std::size_t sbo_size>
class sbo_ptr_base<Base, sbo_size, false, false, false> {
    protected:
    template <typename Derived, typename... Args>
    static constexpr bool can_emplace_v
        = std::conjunction_v<
            std::is_convertible<Derived*, Base*>,
            std::is_constructible<Derived, Args&&...>>;

    template <typename Derived, typename... Args>
    static constexpr bool can_nothrow_emplace_v
        = std::is_nothrow_constructible_v<Derived, Args&&...>;

  public:
    sbo_ptr_base() noexcept = default;

    sbo_ptr_base(sbo_ptr_base const&) = delete;
    sbo_ptr_base(sbo_ptr_base&&) = delete;
    sbo_ptr_base& operator=(sbo_ptr_base const&) = delete;
    sbo_ptr_base& operator=(sbo_ptr_base&&) = delete;
    
    ~sbo_ptr_base() noexcept {
        destroy();
    }

  protected:       
    Base* ptr_ = nullptr; 

    template <typename Derived, 
        typename... Args,
        typename = std::enable_if_t<can_emplace_v<Derived, Args&&...>>>
    void construct(Args&&... args) noexcept(can_nothrow_emplace_v<Derived, Args&&...>) {
        static_assert(
            sizeof(Derived) <= sbo_size,
            "Derived class is too big to store. Increase the small buffer size or allow heap allocations.");
        ptr_ = new(&sbo_buffer_) Derived(std::forward<Args>(args)...);
    }

    void destroy() noexcept {
        if (ptr_) {
            std::exchange(ptr_, nullptr)->~Base();
        }
    }

  private:
    std::aligned_storage_t<sbo_size> sbo_buffer_;
};

template <typename Base, std::size_t sbo_size>
class sbo_ptr_base<Base, sbo_size, true, false, false> {
    protected:
    template <typename Derived, typename... Args>
    static constexpr bool can_emplace_v
        = std::conjunction_v<
            std::is_convertible<Derived*, Base*>,
            std::is_constructible<Derived, Args&&...>,
            std::is_nothrow_move_constructible<Derived>>;

    template <typename Derived, typename... Args>
    static constexpr bool can_nothrow_emplace_v
        = std::is_nothrow_constructible_v<Derived, Args&&...>;

  public:
    sbo_ptr_base() noexcept = default;

    sbo_ptr_base(sbo_ptr_base const&) = delete;

    sbo_ptr_base(sbo_ptr_base&& other) noexcept {
        construct_from(std::move(other));
    }

    auto operator=(sbo_ptr_base const&) = delete;

    auto operator=(sbo_ptr_base&& other) noexcept -> sbo_ptr_base& {
        if (this != &other) {
            destroy();
            construct_from(std::move(other));
        }
        return *this;
    }
    
    ~sbo_ptr_base() noexcept {
        destroy();
    }

  protected:       
    Base* ptr_ = nullptr; 
    sbo_ptr_vtable<Base, true, false, false> const* vtable_ = nullptr;

    template <typename Derived, 
        typename... Args,
        typename = std::enable_if_t<can_emplace_v<Derived, Args&&...>>>
    void construct(Args&&... args) noexcept(can_nothrow_emplace_v<Derived, Args&&...>) {
        static_assert(
            sizeof(Derived) <= sbo_size,
            "Derived class is too big to store. Increase the small buffer size or allow heap allocations.");
        ptr_ = new(&sbo_buffer_) Derived(std::forward<Args>(args)...);
        vtable_ = sbo_ptr_vtable<Base, true, false, false>::template get<Derived>();
    }

    void construct_from(sbo_ptr_base&& other) noexcept {
        if (std::exchange(other.ptr_, nullptr)) {
            ptr_ = other.vtable_->move(&other.sbo_buffer_, &sbo_buffer_);
            vtable_ = std::exchange(other.vtable_, nullptr);
        }
    }

    void destroy() noexcept {
        if (ptr_) {
            std::exchange(ptr_, nullptr)->~Base();
        }
    }

  private:
    std::aligned_storage_t<sbo_size> sbo_buffer_;
};

template <typename Base, std::size_t sbo_size>
class sbo_ptr_base<Base, sbo_size, true, true, false> {
    protected:
    template <typename Derived, typename... Args>
    static constexpr bool can_emplace_v
        = std::conjunction_v<
            std::is_convertible<Derived*, Base*>,
            std::is_constructible<Derived, Args&&...>,
            std::is_nothrow_move_constructible<Derived>,
            std::is_copy_constructible<Derived>>;

    template <typename Derived, typename... Args>
    static constexpr bool can_nothrow_emplace_v 
        = std::is_nothrow_constructible_v<Derived, Args&&...>;

  public:
    sbo_ptr_base() noexcept = default;

    sbo_ptr_base(sbo_ptr_base const& other) {
        construct_from(other);
    }
    
    sbo_ptr_base(sbo_ptr_base&& other) noexcept {
        construct_from(std::move(other));
    }

    auto operator=(sbo_ptr_base const& other) -> sbo_ptr_base& {
        if (this != &other) {
            // Strong exception guarantee
            auto copy = other;
            destroy();
            construct_from(std::move(copy));
        }
        return *this;
    }

    auto operator=(sbo_ptr_base&& other) noexcept -> sbo_ptr_base& {
        if (this != &other) {
            destroy();
            construct_from(std::move(other));
        }
        return *this;
    }
    
    ~sbo_ptr_base() noexcept {
        destroy();
    }

  protected:       
    Base* ptr_ = nullptr; 
    sbo_ptr_vtable<Base, true, true, false> const* vtable_ = nullptr;

    template <typename Derived, 
        typename... Args,
        typename = std::enable_if_t<can_emplace_v<Derived, Args&&...>>>
    void construct(Args&&... args) noexcept(can_nothrow_emplace_v<Derived, Args&&...>) {
        static_assert(
            sizeof(Derived) <= sbo_size,
            "Derived class is too big to store. Increase the small buffer size or allow heap allocations.");
        ptr_ = new(&sbo_buffer_) Derived(std::forward<Args>(args)...);
        vtable_= sbo_ptr_vtable<Base, true, true, false>::template get<Derived>();
    }

    void construct_from(sbo_ptr_base&& other) noexcept {
        if (std::exchange(other.ptr_, nullptr)) {
            ptr_ = other.vtable_->move(&other.sbo_buffer_, &sbo_buffer_);
            vtable_ = std::exchange(other.vtable_, nullptr);
        }
    }

    void construct_from(sbo_ptr_base const& other) {
        if (other.ptr_) {
            ptr_ = other.vtable_->copy(&other.sbo_buffer_, &sbo_buffer_);
            vtable_ = other.vtable_;
        }
    }

    void destroy() noexcept {
        if (ptr_) {
            std::exchange(ptr_, nullptr)->~Base();
        }
    }

  private:
    std::aligned_storage_t<sbo_size> sbo_buffer_;
};

template <typename Base, std::size_t sbo_size>
class sbo_ptr_base<Base, sbo_size, false, false, true> {
  protected:
    template <typename Derived, typename... Args>
    static constexpr bool can_emplace_v
        = std::is_convertible_v<Derived*, Base*>
            and std::is_constructible_v<Derived, Args&&...>;

    template <typename Derived, typename... Args>
    static constexpr bool can_nothrow_emplace_v 
        = std::is_nothrow_constructible_v<Derived, Args&&...>
            and (sizeof(Derived) <= sbo_size);

  public:
    sbo_ptr_base() noexcept = default;

    sbo_ptr_base(sbo_ptr_base const&) = delete;
    sbo_ptr_base(sbo_ptr_base&&) noexcept = delete;
    sbo_ptr_base& operator=(sbo_ptr_base const&) = delete;
    sbo_ptr_base& operator=(sbo_ptr_base&&) noexcept = delete;
    
    ~sbo_ptr_base() noexcept {
        destroy();
    }

  protected:       
    Base* ptr_ = nullptr; 

    template <typename Derived, 
        typename... Args,
        typename = std::enable_if_t<can_emplace_v<Derived, Args&&...>>>
    void construct(Args&&... args) noexcept(can_nothrow_emplace_v<Derived, Args&&...>) {
        if constexpr (sizeof(Derived) <= sbo_size) {
            ptr_ = new(&sbo_buffer_) Derived(std::forward<Args>(args)...);
            on_heap_ = false;
        } else {
            ptr_ = new Derived(std::forward<Args>(args)...);
            on_heap_ = true;
        }
    }

    void destroy() noexcept {
        if (ptr_) {
            if (on_heap_) {
                delete std::exchange(ptr_, nullptr);
            } else {
                std::exchange(ptr_, nullptr)->~Base();
            }
        }
    }

  private:
    // We do not require a vtable for this case.
    // The on_heap flag is intentionaly placed last,
    // so if sbo_size is not a multiple of alignment,
    // the flag gets placed in the padding.
    // To allow that, we must not use aligned_storage_t struct
    // directly (otherwise the padding would be forced);
    // we just use it to figure out proper alignment.
    alignas(alignof(std::aligned_storage_t<sbo_size>)) std::byte sbo_buffer_[sbo_size];
    bool on_heap_ = false;
};

template <typename Base, std::size_t sbo_size>
class sbo_ptr_base<Base, sbo_size, true, false, true> {
  protected:
    template <typename Derived, typename... Args>
    static constexpr bool can_emplace_v
        = std::conjunction_v<
            std::is_convertible<Derived*, Base*>,
            std::is_nothrow_move_constructible<Derived>,
            std::is_constructible<Derived, Args&&...>>;

    template <typename Derived, typename... Args>
    static constexpr bool can_nothrow_emplace_v 
        = std::is_nothrow_constructible_v<Derived, Args&&...>
            and (sizeof(Derived) <= sbo_size);

  public:
    sbo_ptr_base() noexcept = default;

    sbo_ptr_base(sbo_ptr_base const&) = delete;
    
    sbo_ptr_base(sbo_ptr_base&& other) noexcept {
        construct_from(std::move(other));
    }

    auto operator=(sbo_ptr_base const&) = delete;

    auto operator=(sbo_ptr_base&& other) noexcept -> sbo_ptr_base& {
        if (this != &other) {
            destroy();
            construct_from(std::move(other));
        }
        return *this;
    }
    
    ~sbo_ptr_base() noexcept {
        destroy();
    }

  protected:       
    Base* ptr_ = nullptr; 
    sbo_ptr_vtable<Base, true, false, true> const* vtable_ = nullptr;

    template <typename Derived, 
        typename... Args,
        typename = std::enable_if_t<can_emplace_v<Derived, Args&&...>>>
    void construct(Args&&... args) noexcept(can_nothrow_emplace_v<Derived, Args&&...>) {
        if constexpr (sizeof(Derived) <= sbo_size) {
            ptr_ = new(&sbo_buffer_) Derived(std::forward<Args>(args)...);
            vtable_ = sbo_ptr_vtable<Base, true, false, true>::template get<Derived, false>();
        } else {
            ptr_ = new Derived(std::forward<Args>(args)...);
            vtable_ = sbo_ptr_vtable<Base, true, false, true>::template get<Derived, true>();
        }
    }

    void construct_from(sbo_ptr_base&& other) noexcept {
        if (other.ptr_) {
            if (other.vtable_->on_heap) {
                ptr_ = std::exchange(other.ptr_, nullptr);
            } else {
                ptr_ = other.vtable_->move(&other.sbo_buffer_, &sbo_buffer_);
                other.ptr_ = nullptr;
            }
            vtable_ = std::exchange(other.vtable_, nullptr);
        }
    }

    void destroy() noexcept {
        if (ptr_) {
            if (vtable_->on_heap) {
                delete std::exchange(ptr_, nullptr);
            } else {
                std::exchange(ptr_, nullptr)->~Base();
            }
        }
    }

  private:
    std::aligned_storage_t<sbo_size> sbo_buffer_;
};

template <typename Base, std::size_t sbo_size>
class sbo_ptr_base<Base, sbo_size, true, true, true> {
  protected:
    template <typename Derived, typename... Args>
    static constexpr bool can_emplace_v
        = std::conjunction_v<
            std::is_convertible<Derived*, Base*>,
            std::is_nothrow_move_constructible<Derived>,
            std::is_copy_constructible<Derived>,
            std::is_constructible<Derived, Args&&...>>;

    template <typename Derived, typename... Args>
    static constexpr bool can_nothrow_emplace_v 
        = std::is_nothrow_constructible_v<Derived, Args&&...>
            and (sizeof(Derived) <= sbo_size);

  public:
    sbo_ptr_base() noexcept = default;

    sbo_ptr_base(sbo_ptr_base const& other) {
        construct_from(other);
    }
    
    sbo_ptr_base(sbo_ptr_base&& other) noexcept {
        construct_from(std::move(other));
    }

    auto operator=(sbo_ptr_base const& other) -> sbo_ptr_base& {
        if (this != &other) {
            // Strong exception guarantee
            auto copy = other;
            destroy();
            construct_from(std::move(copy));
        }
        return *this;
    }

    auto operator=(sbo_ptr_base&& other) noexcept -> sbo_ptr_base& {
        if (this != &other) {
            destroy();
            construct_from(std::move(other));
        }
        return *this;
    }
    
    ~sbo_ptr_base() noexcept {
        destroy();
    }

  protected:       
    Base* ptr_ = nullptr; 
    sbo_ptr_vtable<Base, true, true, true> const* vtable_ = nullptr;

    template <typename Derived, 
        typename... Args,
        typename = std::enable_if_t<can_emplace_v<Derived, Args&&...>>>
    void construct(Args&&... args) noexcept(can_nothrow_emplace_v<Derived, Args&&...>) {
        if constexpr (sizeof(Derived) <= sbo_size) {
            ptr_ = new(&sbo_buffer_) Derived(std::forward<Args>(args)...);
            vtable_ = sbo_ptr_vtable<Base, true, true, true>::template get<Derived, false>();
        } else {
            ptr_ = new Derived(std::forward<Args>(args)...);
            vtable_ = sbo_ptr_vtable<Base, true, true, true>::template get<Derived, true>();
        }
    }

    void construct_from(sbo_ptr_base const& other) {
        if (other.ptr_) {
            if (other.vtable_->on_heap) {
                ptr_ = other.vtable_->heap_copy(other.ptr_);
            } else {
                ptr_ = other.vtable_->copy(&other.sbo_buffer_, &sbo_buffer_);
            }
            vtable_ = other.vtable_;
        }
    }

    void construct_from(sbo_ptr_base&& other) noexcept {
        if (other.ptr_) {
            if (other.vtable_->on_heap) {
                ptr_ = std::exchange(other.ptr_, nullptr);
            } else {
                ptr_ = other.vtable_->move(&other.sbo_buffer_, &sbo_buffer_);
                other.ptr_ = nullptr;
            }
            vtable_ = std::exchange(other.vtable_, nullptr);
        }
    }

    void destroy() noexcept {
        if (ptr_) {
            if (vtable_->on_heap) {
                delete std::exchange(ptr_, nullptr);
            } else {
                std::exchange(ptr_, nullptr)->~Base();
            }
        }
    }

  private:
    std::aligned_storage_t<sbo_size> sbo_buffer_;
};

}

template <typename T, std::size_t sbo_size, bool enable_move, bool enable_copy, bool enable_heap>
class basic_sbo_ptr : private detail::sbo_ptr_base<T, sbo_size, enable_move, enable_copy, enable_heap> {
  private:
    using Base = detail::sbo_ptr_base<T, sbo_size, enable_move, enable_copy, enable_heap>;

  public:
    using pointer = T*;
    using element_type = T;

    basic_sbo_ptr() noexcept = default;

    basic_sbo_ptr(std::nullptr_t) noexcept {}

    using Base::Base;

    template <typename U,
        typename... Args,
        typename = std::enable_if_t<Base::template can_emplace_v<U, Args&&...>>>
    explicit basic_sbo_ptr(std::in_place_type_t<U>, Args&&... args)
        noexcept(Base::template can_nothrow_emplace_v<U, Args&&...>) {
        Base::template construct<U>(std::forward<Args>(args)...);
    }

    template <typename U,
        typename = std::enable_if_t<
            not std::is_same_v<std::decay_t<U>, basic_sbo_ptr> and Base::template can_emplace_v<std::decay_t<U>, U&&>>>
    basic_sbo_ptr(U&& value) noexcept(Base::template can_nothrow_emplace_v<std::decay_t<U>, U&&>)
      : basic_sbo_ptr{std::in_place_type<std::decay_t<U>>, std::forward<U>(value)} {
    }

    template <typename U,
              typename = std::enable_if_t<
                  std::is_same_v<U, std::decay_t<U>> and not std::is_same_v<U, basic_sbo_ptr> and Base::template can_emplace_v<std::decay_t<U>, U&&>>>
    auto operator=(U&& other) noexcept -> basic_sbo_ptr& {
        Base::destroy();
        Base::template construct<U>(std::forward<U>(other));
        return *this;
    }

    template <typename U,
               typename = std::enable_if_t<
                   not std::is_same_v<U, basic_sbo_ptr> and Base::template can_emplace_v<U, U const&>>>
    auto operator=(U const& other) noexcept(Base::template can_nothrow_emplace_v<U, U const&>) -> basic_sbo_ptr& {
        // Strong exception guarantee
        return (*this = U{other});
    }

    auto operator=(std::nullptr_t) noexcept -> basic_sbo_ptr& {
        Base::destroy();
        return *this;
    }

    template <typename U,
              typename... Args,
              typename = std::enable_if_t<Base::template can_emplace_v<U, Args&&...>>>
    void emplace(Args&&... args) noexcept(Base::template can_nothrow_emplace_v<U, Args&&...>) {
        // Emplace cannot provide strong exception guarantee
        Base::destroy();
        Base::template construct<U>(std::forward<Args>(args)...);
    }

    void reset() noexcept {
        Base::destroy();
    }

    [[nodiscard]] auto get() noexcept -> T* {
        return Base::ptr_;
    }

    [[nodiscard]] auto get() const noexcept -> T const* {
        return Base::ptr_;
    }

    [[nodiscard]] auto operator*() noexcept -> T& {
        return *get();
    }

    [[nodiscard]] auto operator*() const noexcept -> T const& {
        return *get();
    }

    [[nodiscard]] auto operator->() noexcept -> T* {
        return get();
    }

    [[nodiscard]] auto operator->() const noexcept -> T const* {
        return get();
    }

    [[nodiscard]] explicit operator bool() const noexcept {
        return *this != nullptr;
    }

    [[nodiscard]] friend auto operator==(basic_sbo_ptr const& lhs, basic_sbo_ptr const& rhs) noexcept -> bool {
        return lhs.get() == rhs.get();
    }

    [[nodiscard]] friend auto operator==(basic_sbo_ptr const& lhs, std::nullptr_t) noexcept -> bool {
        return lhs.ptr_ == nullptr;
    }

    [[nodiscard]] friend auto operator==(std::nullptr_t, basic_sbo_ptr const& rhs) noexcept -> bool {
        return rhs.ptr_ == nullptr;
    }

    [[nodiscard]] friend auto operator!=(basic_sbo_ptr const& lhs, basic_sbo_ptr const& rhs) noexcept -> bool {
        return not (lhs == rhs);
    }

    [[nodiscard]] friend auto operator!=(basic_sbo_ptr const& lhs, std::nullptr_t) noexcept -> bool {
        return not (lhs == nullptr);
    }

    [[nodiscard]] friend auto operator!=(std::nullptr_t, basic_sbo_ptr const& rhs) noexcept -> bool {
        return not (nullptr == rhs);
    }
};

template <typename T, std::size_t sbo_size = sizeof(T)>
using pinned_sbo_ptr = basic_sbo_ptr<T, sbo_size, false, false, true>;

template <typename T, std::size_t sbo_size = sizeof(T)>
using pinned_no_alloc_sbo_ptr = basic_sbo_ptr<T, sbo_size, false, false, false>;

template <typename T, std::size_t sbo_size = sizeof(T)>
using unique_sbo_ptr = basic_sbo_ptr<T, sbo_size, true, false, true>;

template <typename T, std::size_t sbo_size = sizeof(T)>
using unique_no_alloc_sbo_ptr = basic_sbo_ptr<T, sbo_size, true, false, false>;

template <typename T, std::size_t sbo_size = sizeof(T)>
using sbo_ptr = basic_sbo_ptr<T, sbo_size, true, true, true>;

template <typename T, std::size_t sbo_size = sizeof(T)>
using no_alloc_sbo_ptr = basic_sbo_ptr<T, sbo_size, true, true, false>;

}
