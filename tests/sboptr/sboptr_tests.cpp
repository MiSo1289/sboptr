#include <algorithm>
#include <array>
#include <cstddef>
#include <optional>
#include <tuple>

#include <catch/catch.hpp>
#include "sboptr/sboptr.hpp"

namespace {

class interface {
  public:
    virtual ~interface() = default;

    [[nodiscard]] virtual auto foo() const noexcept -> int = 0;
};

template <typename Derived>
class new_base {
  public:
    static constexpr std::size_t max_objects = 12;
    static constexpr std::size_t new_buffer_size = 128;
    static inline std::array<
        std::optional<std::aligned_storage_t<new_buffer_size>>,
        max_objects>
        new_buffers = {};

    static auto find_buff(void const* const ptr) {
        return std::find_if(
            new_buffers.begin(),
            new_buffers.end(),
            [=](auto const& buff) { return buff.has_value() and &*buff == ptr; });
    }

    /** Dynamic allocation */
    static auto operator new(std::size_t const count) -> void* {
        REQUIRE(count <= new_buffer_size);
        auto const iter = std::find_if(
            new_buffers.begin(),
            new_buffers.end(),
            [](auto const& buff) { return not buff.has_value(); });
        REQUIRE(iter != new_buffers.end());
        return &iter->emplace();
    }

    /** Placement new must also be overloaded now */
    static auto operator new([[maybe_unused]] std::size_t const count, void* const buff) -> void* {
        return buff;
    }

    static void operator delete(void* const ptr) {
        auto const iter = find_buff(ptr);
        REQUIRE(iter != new_buffers.end());
        iter->reset();
    }

    [[nodiscard]] auto is_dyn_allocated() const noexcept -> bool {
        return find_buff(static_cast<Derived const*>(this)) != new_buffers.end();
    }
};

class impl_a : public interface, public new_base<impl_a> {
  public:
    using new_base<impl_a>::operator new;
    using new_base<impl_a>::operator delete;

    static constexpr auto foo_constant = 1;

    std::string str;

    explicit impl_a(std::string str) : str{std::move(str)} {}

    friend auto operator==(impl_a const& lhs, impl_a const& rhs) noexcept -> bool {
        return lhs.str == rhs.str;
    }

    auto foo() const noexcept -> int override { return foo_constant; }
};

class impl_b : public interface, public new_base<impl_b> {
  public:
    using new_base<impl_b>::operator new;
    using new_base<impl_b>::operator delete;

    static constexpr auto foo_constant = 2;

    std::string str_a;
    std::string str_b;

    impl_b(std::string str_a, std::string str_b)
        : str_a{std::move(str_a)}, str_b{std::move(str_b)} {}

    friend auto operator==(impl_b const& lhs, impl_b const& rhs) noexcept -> bool {
        return lhs.str_a == rhs.str_a and lhs.str_b == lhs.str_b;
    }

    auto foo() const noexcept -> int override { return foo_constant; }
};

class copy_throw_impl : public interface, public new_base<copy_throw_impl> {
  public:
    using new_base<copy_throw_impl>::operator new;
    using new_base<copy_throw_impl>::operator delete;

    static constexpr auto foo_constant = 3;

    std::string str;

    explicit copy_throw_impl(std::string str)
        : str{std::move(str)} {}

    copy_throw_impl(copy_throw_impl const&) {
        throw std::runtime_error{""};
    }

    copy_throw_impl(copy_throw_impl&&) noexcept = default;

    friend auto operator==(copy_throw_impl const& lhs, copy_throw_impl const& rhs) noexcept -> bool {
        return lhs.str == rhs.str;
    }

    auto foo() const noexcept -> int override { return foo_constant; }
};

template <typename TupleA, typename TupleB>
using tuple_cat_t = decltype(std::tuple_cat(std::declval<TupleA>(), std::declval<TupleB>()));
template <typename Elem, typename Tuple>
using tuple_append_t = tuple_cat_t<Tuple, std::tuple<Elem>>;

using no_alloc_copy_ptrs_small = std::tuple<sboptr::no_alloc_sbo_ptr<interface>>;
using no_alloc_move_ptrs_small = tuple_append_t<
    sboptr::unique_no_alloc_sbo_ptr<interface>,
    no_alloc_copy_ptrs_small>;
using no_alloc_ptrs_small = tuple_append_t<
    sboptr::pinned_no_alloc_sbo_ptr<interface>,
    no_alloc_move_ptrs_small>;

using alloc_copy_ptrs_small = std::tuple<sboptr::sbo_ptr<interface>>;
using alloc_move_ptrs_small = tuple_append_t<
    sboptr::unique_sbo_ptr<interface>,
    alloc_copy_ptrs_small>;
using alloc_ptrs_small = tuple_append_t<
    sboptr::pinned_sbo_ptr<interface>,
    alloc_move_ptrs_small>;

using no_alloc_copy_ptrs_medium = std::tuple<sboptr::no_alloc_sbo_ptr<interface, sizeof(impl_a)>>;
using no_alloc_move_ptrs_medium = tuple_append_t<
    sboptr::unique_no_alloc_sbo_ptr<interface, sizeof(impl_a)>,
    no_alloc_copy_ptrs_medium>;
using no_alloc_ptrs_medium = tuple_append_t<
    sboptr::pinned_no_alloc_sbo_ptr<interface, sizeof(impl_a)>,
    no_alloc_move_ptrs_medium>;

using alloc_copy_ptrs_medium = std::tuple<sboptr::sbo_ptr<interface, sizeof(impl_a)>>;
using alloc_move_ptrs_medium = tuple_append_t<
    sboptr::unique_sbo_ptr<interface, sizeof(impl_a)>,
    alloc_copy_ptrs_medium>;
using alloc_ptrs_medium = tuple_append_t<
    sboptr::pinned_sbo_ptr<interface, sizeof(impl_a)>,
    alloc_move_ptrs_medium>;

using no_alloc_copy_ptrs_big = std::tuple<sboptr::no_alloc_sbo_ptr<interface, sizeof(impl_b)>>;
using no_alloc_move_ptrs_big = tuple_append_t<
    sboptr::unique_no_alloc_sbo_ptr<interface, sizeof(impl_b)>,
    no_alloc_copy_ptrs_big>;
using no_alloc_ptrs_big = tuple_append_t<
    sboptr::pinned_no_alloc_sbo_ptr<interface, sizeof(impl_b)>,
    no_alloc_move_ptrs_big>;

using alloc_copy_ptrs_big = std::tuple<sboptr::sbo_ptr<interface, sizeof(impl_b)>>;
using alloc_move_ptrs_big = tuple_append_t<
    sboptr::unique_sbo_ptr<interface, sizeof(impl_b)>,
    alloc_copy_ptrs_big>;
using alloc_ptrs_big = tuple_append_t<
    sboptr::pinned_sbo_ptr<interface, sizeof(impl_b)>,
    alloc_move_ptrs_big>;

using copy_ptrs_small = tuple_cat_t<
    no_alloc_copy_ptrs_small,
    alloc_copy_ptrs_small>;
using copy_ptrs_medium = tuple_cat_t<
    no_alloc_copy_ptrs_medium,
    alloc_copy_ptrs_medium>;
using copy_ptrs_big = tuple_cat_t<
    no_alloc_copy_ptrs_big,
    alloc_copy_ptrs_big>;

using move_ptrs_small = tuple_cat_t<
    no_alloc_move_ptrs_small,
    alloc_move_ptrs_small>;
using move_ptrs_medium = tuple_cat_t<
    no_alloc_move_ptrs_medium,
    alloc_move_ptrs_medium>;
using move_ptrs_big = tuple_cat_t<
    no_alloc_move_ptrs_big,
    alloc_move_ptrs_big>;

using ptrs_small = tuple_cat_t<
    no_alloc_ptrs_small,
    alloc_ptrs_small>;
using ptrs_medium = tuple_cat_t<
    no_alloc_ptrs_medium,
    alloc_ptrs_medium>;
using ptrs_big = tuple_cat_t<
    no_alloc_ptrs_big,
    alloc_ptrs_big>;

template <typename Tuple, typename Fn>
void tuple_for_each(Tuple&& tuple, Fn&& fn) {
    std::apply(
        [&](auto&&... elems) {
            (std::invoke(std::forward<Fn>(fn), std::forward<decltype(elems)>(elems)), ...);
        },
        std::forward<Tuple>(tuple));
}

template <typename ptr_t>
void check_empty(ptr_t const& ptr) {
    CHECK(ptr == nullptr);
    CHECK_FALSE(ptr != nullptr);
    CHECK(ptr == ptr);
    CHECK_FALSE(ptr != ptr);
    CHECK_FALSE(static_cast<bool>(ptr));
    CHECK(ptr.get() == nullptr);
}

template <typename T, typename ptr_t>
void check_impl_is_constructed(T const& impl, bool const dyn_allocated, ptr_t const& ptr) {
    CHECK(ptr != nullptr);
    CHECK_FALSE(ptr == nullptr);
    CHECK(ptr == ptr);
    CHECK_FALSE(ptr != ptr);
    CHECK(ptr != ptr_t{});
    CHECK_FALSE(ptr == ptr_t{});
    CHECK(static_cast<bool>(ptr));
    REQUIRE(ptr.get() != nullptr);
    CHECK(ptr->foo() == T::foo_constant);

    auto const* derived = dynamic_cast<T const*>(ptr.get());
    REQUIRE(derived != nullptr);
    CHECK(*derived == impl);
    CHECK(derived->is_dyn_allocated() == dyn_allocated);
}

constexpr auto long_string
    = "Lorem ipsum dolor sit amet, consectetur adipiscing elit. "
      "Donec tortor dui, maximus id scelerisque sit amet, interdum a turpis. "
      "Quisque mollis, justo in pharetra varius, urna diam laoreet mauris, "
      "eu convallis mauris mi et justo. Quisque aliquam nisi nec quam porttitor, "
      "ut lacinia enim ultricies. Vivamus sit amet tellus mi. Nulla facilisi.";
constexpr auto long_string_2
    = "Fusce vitae sapien sit amet lorem ultricies consequat vel sed enim. "
      "Curabitur nec dui quis tellus faucibus ornare vitae quis arcu. "
      "Fusce condimentum ligula vitae purus cursus accumsan. "
      "Nunc non diam eget sapien finibus consectetur a eget arcu.";


auto const impl_a = impl_a{long_string};
auto const impl_b = impl_b{long_string, long_string_2};

}

TEST_CASE("Empty pointer") {
    auto default_inited_ptrs = ptrs_small{};

    tuple_for_each(default_inited_ptrs, [](auto const& ptr) { check_empty(ptr); });

    auto null_inited_ptrs = std::apply(
        [](auto const& ... ptrs) {
            // Using comma operator on ptrs to create a pack of nullptr
            return ptrs_small{(static_cast<void>(ptrs), nullptr)...};
        },
        default_inited_ptrs);

    tuple_for_each(null_inited_ptrs, [](auto const& ptr) { check_empty(ptr); });

    SECTION("Reset keeps it in empty state") {
        tuple_for_each(default_inited_ptrs, [](auto& ptr) {
            ptr.reset();

            check_empty(ptr);
        });
    }

    SECTION("= nullptr keeps it in empty state") {
        tuple_for_each(default_inited_ptrs, [](auto& ptr) {
            ptr = nullptr;

            check_empty(ptr);
        });
    }
}

TEST_CASE("Constructing objects in small buffer") {
    SECTION("Move construction from impls") {
        tuple_for_each(ptrs_medium{}, [](auto const& ptr) {
            using ptr_t = std::decay_t<decltype(ptr)>;
            auto ptr_holding_a = ptr_t{impl_a};
            check_impl_is_constructed(impl_a, false, ptr_holding_a);
        });

        tuple_for_each(ptrs_big{}, [](auto const& ptr) {
            using ptr_t = std::decay_t<decltype(ptr)>;
            auto ptr_holding_a = ptr_t{impl_a};
            check_impl_is_constructed(impl_a, false, ptr_holding_a);

            auto ptr_holding_b = ptr_t{impl_b};
            check_impl_is_constructed(impl_b, false, ptr_holding_b);
        });
    }

    SECTION("In place construction") {
        tuple_for_each(ptrs_medium{}, [](auto const& ptr) {
            using ptr_t = std::decay_t<decltype(ptr)>;
            auto ptr_holding_a = ptr_t{std::in_place_type<impl_a>, long_string};
            check_impl_is_constructed(impl_a, false, ptr_holding_a);
        });

        tuple_for_each(ptrs_big{}, [](auto const& ptr) {
            using ptr_t = std::decay_t<decltype(ptr)>;
            auto ptr_holding_a = ptr_t{std::in_place_type<impl_a>, long_string};
            check_impl_is_constructed(impl_a, false, ptr_holding_a);

            auto ptr_holding_b = ptr_t{
                std::in_place_type<impl_b>,
                long_string,
                long_string_2,
            };
            check_impl_is_constructed(impl_b, false, ptr_holding_b);
        });
    }

    SECTION("Emplace and assign") {
        auto medium_ptrs = ptrs_medium{};

        tuple_for_each(medium_ptrs, [](auto& ptr) {
            ptr.template emplace<impl_a>(long_string);
            check_impl_is_constructed(impl_a, false, ptr);
            ptr = nullptr;
            check_empty(ptr);
            ptr = impl_a;
            check_impl_is_constructed(impl_a, false, ptr);
            ptr.reset();
            check_empty(ptr);
        });

        auto big_ptrs = ptrs_big{};

        tuple_for_each(big_ptrs, [](auto& ptr) {
            ptr.template emplace<impl_a>(long_string);
            check_impl_is_constructed(impl_a, false, ptr);
            ptr = nullptr;
            check_empty(ptr);
            ptr = impl_a;
            check_impl_is_constructed(impl_a, false, ptr);
            ptr.reset();
            check_empty(ptr);

            ptr.template emplace<impl_b>(long_string, long_string_2);
            check_impl_is_constructed(impl_b, false, ptr);
            ptr = nullptr;
            check_empty(ptr);
            ptr = impl_b;
            check_impl_is_constructed(impl_b, false, ptr);
            ptr.reset();
            check_empty(ptr);
        });
    }
}

TEST_CASE("Constructing objects in dynamic storage") {
    SECTION("Move construction from impls") {
        tuple_for_each(alloc_ptrs_medium{}, [](auto const& ptr) {
            using ptr_t = std::decay_t<decltype(ptr)>;
            auto ptr_holding_b = ptr_t{impl_b};
            check_impl_is_constructed(impl_b, true, ptr_holding_b);
        });

        tuple_for_each(alloc_ptrs_small{}, [](auto const& ptr) {
            using ptr_t = std::decay_t<decltype(ptr)>;
            auto ptr_holding_a = ptr_t{impl_a};
            check_impl_is_constructed(impl_a, true, ptr_holding_a);

            auto ptr_holding_b = ptr_t{impl_b};
            check_impl_is_constructed(impl_b, true, ptr_holding_b);
        });
    }

    SECTION("In place construction") {
        tuple_for_each(alloc_ptrs_medium{}, [](auto const& ptr) {
            using ptr_t = std::decay_t<decltype(ptr)>;
            auto ptr_holding_b = ptr_t{
                std::in_place_type<impl_b>,
                long_string,
                long_string_2,
            };
            check_impl_is_constructed(impl_b, true, ptr_holding_b);
        });

        tuple_for_each(alloc_ptrs_small{}, [](auto const& ptr) {
            using ptr_t = std::decay_t<decltype(ptr)>;
            auto ptr_holding_a = ptr_t{std::in_place_type<impl_a>, long_string};
            check_impl_is_constructed(impl_a, true, ptr_holding_a);

            auto ptr_holding_b = ptr_t{
                std::in_place_type<impl_b>,
                long_string,
                long_string_2,
            };
            check_impl_is_constructed(impl_b, true, ptr_holding_b);
        });
    }

    SECTION("Emplace and assign") {
        auto medium_ptrs = alloc_ptrs_medium{};

        tuple_for_each(medium_ptrs, [](auto& ptr) {
            ptr.template emplace<impl_a>(long_string);
            check_impl_is_constructed(impl_a, false, ptr);
            ptr = nullptr;
            check_empty(ptr);
            ptr = impl_a;
            check_impl_is_constructed(impl_a, false, ptr);
            ptr.reset();
            check_empty(ptr);

            ptr.template emplace<impl_b>(long_string, long_string_2);
            check_impl_is_constructed(impl_b, true, ptr);
            ptr = nullptr;
            check_empty(ptr);
            ptr = impl_b;
            check_impl_is_constructed(impl_b, true, ptr);
            ptr.reset();
            check_empty(ptr);
        });

        auto small_ptrs = alloc_ptrs_small{};

        tuple_for_each(small_ptrs, [](auto& ptr) {
            ptr.template emplace<impl_a>(long_string);
            check_impl_is_constructed(impl_a, true, ptr);
            ptr = nullptr;
            check_empty(ptr);
            ptr = impl_a;
            check_impl_is_constructed(impl_a, true, ptr);
            ptr.reset();
            check_empty(ptr);

            ptr.template emplace<impl_b>(long_string, long_string_2);
            check_impl_is_constructed(impl_b, true, ptr);
            ptr = nullptr;
            check_empty(ptr);
            ptr = impl_b;
            check_impl_is_constructed(impl_b, true, ptr);
            ptr.reset();
            check_empty(ptr);
        });
    }
}

TEST_CASE("Move pointers with active small buffer storage") {
    tuple_for_each(move_ptrs_medium{}, [](auto const& ptr) {
        using ptr_t = std::decay_t<decltype(ptr)>;
        auto ptr1 = ptr_t{impl_a};
        auto ptr2 = std::move(ptr1);
        check_empty(ptr1);
        check_impl_is_constructed(impl_a, false, ptr2);

        ptr1 = std::move(ptr2);
        check_empty(ptr2);
        check_impl_is_constructed(impl_a, false, ptr1);

        std::swap(ptr1, ptr2);
        check_empty(ptr1);
        check_impl_is_constructed(impl_a, false, ptr2);

        ptr2.reset();
        check_empty(ptr2);

        ptr1 = std::move(ptr2);
        check_empty(ptr1);
        check_empty(ptr2);
    });

    tuple_for_each(move_ptrs_big{}, [](auto const& ptr) {
        using ptr_t = std::decay_t<decltype(ptr)>;
        auto ptr1 = ptr_t{impl_a};
        auto ptr2 = std::move(ptr1);
        check_empty(ptr1);
        check_impl_is_constructed(impl_a, false, ptr2);

        auto ptr3 = ptr_t{impl_b};
        ptr1 = std::move(ptr3);
        check_empty(ptr3);
        check_impl_is_constructed(impl_b, false, ptr1);

        std::swap(ptr1, ptr2);
        check_impl_is_constructed(impl_a, false, ptr1);
        check_impl_is_constructed(impl_b, false, ptr2);

        ptr1 = std::move(ptr2);
        check_empty(ptr2);
        check_impl_is_constructed(impl_b, false, ptr1);
    });

}

TEST_CASE("Move pointers with active dynamic storage") {
    tuple_for_each(alloc_move_ptrs_medium{}, [](auto const& ptr) {
        using ptr_t = std::decay_t<decltype(ptr)>;
        auto ptr1 = ptr_t{impl_b};
        auto ptr2 = std::move(ptr1);
        check_empty(ptr1);
        check_impl_is_constructed(impl_b, true, ptr2);

        ptr1 = std::move(ptr2);
        check_empty(ptr2);
        check_impl_is_constructed(impl_b, true, ptr1);

        std::swap(ptr1, ptr2);
        check_empty(ptr1);
        check_impl_is_constructed(impl_b, true, ptr2);

        ptr2.reset();
        check_empty(ptr2);

        ptr1 = std::move(ptr2);
        check_empty(ptr1);
        check_empty(ptr2);
    });

    tuple_for_each(alloc_move_ptrs_medium{}, [](auto const& ptr) {
        using ptr_t = std::decay_t<decltype(ptr)>;
        auto ptr1 = ptr_t{impl_a};
        auto ptr2 = std::move(ptr1);
        check_empty(ptr1);
        check_impl_is_constructed(impl_a, false, ptr2);

        auto ptr3 = ptr_t{impl_b};
        ptr1 = std::move(ptr3);
        check_empty(ptr3);
        check_impl_is_constructed(impl_b, true, ptr1);

        std::swap(ptr1, ptr2);
        check_impl_is_constructed(impl_a, false, ptr1);
        check_impl_is_constructed(impl_b, true, ptr2);

        ptr1 = std::move(ptr2);
        check_empty(ptr2);
        check_impl_is_constructed(impl_b, true, ptr1);
    });

    tuple_for_each(alloc_move_ptrs_small{}, [](auto const& ptr) {
        using ptr_t = std::decay_t<decltype(ptr)>;
        auto ptr1 = ptr_t{impl_a};
        auto ptr2 = std::move(ptr1);
        check_empty(ptr1);
        check_impl_is_constructed(impl_a, true, ptr2);

        auto ptr3 = ptr_t{impl_b};
        ptr1 = std::move(ptr3);
        check_empty(ptr3);
        check_impl_is_constructed(impl_b, true, ptr1);

        std::swap(ptr1, ptr2);
        check_impl_is_constructed(impl_a, true, ptr1);
        check_impl_is_constructed(impl_b, true, ptr2);

        ptr1 = std::move(ptr2);
        check_empty(ptr2);
        check_impl_is_constructed(impl_b, true, ptr1);
    });
}

TEST_CASE("Copy pointers with active small buffer storage") {
    tuple_for_each(copy_ptrs_medium{}, [](auto const& ptr) {
        using ptr_t = std::decay_t<decltype(ptr)>;
        auto ptr1 = ptr_t{impl_a};
        auto ptr2 = ptr1;
        check_impl_is_constructed(impl_a, false, ptr1);
        check_impl_is_constructed(impl_a, false, ptr2);

        ptr1.reset();
        check_empty(ptr1);
        ptr1 = ptr2;
        check_impl_is_constructed(impl_a, false, ptr1);
        check_impl_is_constructed(impl_a, false, ptr2);
    });

    tuple_for_each(copy_ptrs_big{}, [](auto const& ptr) {
        using ptr_t = std::decay_t<decltype(ptr)>;
        auto ptr1 = ptr_t{impl_a};
        auto ptr2 = ptr1;
        check_impl_is_constructed(impl_a, false, ptr1);
        check_impl_is_constructed(impl_a, false, ptr2);

        ptr1 = impl_b;
        check_impl_is_constructed(impl_b, false, ptr1);

        ptr2 = ptr1;
        check_impl_is_constructed(impl_b, false, ptr1);
        check_impl_is_constructed(impl_b, false, ptr2);
    });

    SECTION("Strong exception guarantee") {
        tuple_for_each(copy_ptrs_medium{}, [](auto const& ptr) {
            using ptr_t = std::decay_t<decltype(ptr)>;
            auto const impl = copy_throw_impl{long_string};
            auto ptr1 = ptr_t{copy_throw_impl{long_string}};
            check_impl_is_constructed(impl, false, ptr1);
            auto ptr2 = ptr_t{};

            CHECK_THROWS(ptr2 = ptr1);
            check_empty(ptr2);
            check_impl_is_constructed(impl, false, ptr1);

            CHECK_THROWS(ptr2 = impl);
            check_empty(ptr2);

            ptr2 = ptr_t{impl_a};
            check_impl_is_constructed(impl_a, false, ptr2);

            CHECK_THROWS(ptr2 = ptr1);
            check_impl_is_constructed(impl, false, ptr1);
            check_impl_is_constructed(impl_a, false, ptr2);

            CHECK_THROWS(ptr2 = impl);
            check_impl_is_constructed(impl, false, ptr1);
            check_impl_is_constructed(impl_a, false, ptr2);
        });
    }
}

TEST_CASE("Copy pointers with active dynamic storage") {
    tuple_for_each(alloc_copy_ptrs_medium{}, [](auto const& ptr) {
        using ptr_t = std::decay_t<decltype(ptr)>;
        auto ptr1 = ptr_t{impl_a};
        auto ptr2 = ptr1;
        check_impl_is_constructed(impl_a, false, ptr1);
        check_impl_is_constructed(impl_a, false, ptr2);

        ptr1 = impl_b;
        check_impl_is_constructed(impl_b, true, ptr1);

        ptr2 = ptr1;
        check_impl_is_constructed(impl_b, true, ptr1);
        check_impl_is_constructed(impl_b, true, ptr2);
    });

    tuple_for_each(alloc_copy_ptrs_small{}, [](auto const& ptr) {
        using ptr_t = std::decay_t<decltype(ptr)>;
        auto ptr1 = ptr_t{impl_a};
        auto ptr2 = ptr1;
        check_impl_is_constructed(impl_a, true, ptr1);
        check_impl_is_constructed(impl_a, true, ptr2);

        ptr1 = impl_b;
        check_impl_is_constructed(impl_b, true, ptr1);

        ptr2 = ptr1;
        check_impl_is_constructed(impl_b, true, ptr1);
        check_impl_is_constructed(impl_b, true, ptr2);
    });

    SECTION("Strong exception guarantee") {
        tuple_for_each(alloc_copy_ptrs_small{}, [](auto const& ptr) {
            using ptr_t = std::decay_t<decltype(ptr)>;
            auto const impl = copy_throw_impl{long_string};
            auto ptr1 = ptr_t{copy_throw_impl{long_string}};
            check_impl_is_constructed(impl, true, ptr1);
            auto ptr2 = ptr_t{};
            CHECK_THROWS(ptr2 = ptr1);
            check_empty(ptr2);
            check_impl_is_constructed(impl, true, ptr1);

            CHECK_THROWS(ptr2 = impl);
            check_empty(ptr2);

            ptr2 = ptr_t{impl_a};
            check_impl_is_constructed(impl_a, true, ptr2);

            CHECK_THROWS(ptr2 = ptr1);
            check_impl_is_constructed(impl, true, ptr1);
            check_impl_is_constructed(impl_a, true, ptr2);

            CHECK_THROWS(ptr2 = impl);
            check_impl_is_constructed(impl, true, ptr1);
            check_impl_is_constructed(impl_a, true, ptr2);
        });

        tuple_for_each(alloc_copy_ptrs_medium{}, [](auto const& ptr) {
            using ptr_t = std::decay_t<decltype(ptr)>;
            auto const impl = copy_throw_impl{long_string};
            auto ptr1 = ptr_t{copy_throw_impl{long_string}};
            check_impl_is_constructed(impl, false, ptr1);
            auto ptr2 = ptr_t{};
            CHECK_THROWS(ptr2 = ptr1);
            check_empty(ptr2);
            check_impl_is_constructed(impl, false, ptr1);

            CHECK_THROWS(ptr2 = impl);
            check_empty(ptr2);

            ptr2 = ptr_t{impl_b};
            check_impl_is_constructed(impl_b, true, ptr2);

            CHECK_THROWS(ptr2 = ptr1);
            check_impl_is_constructed(impl, false, ptr1);
            check_impl_is_constructed(impl_b, true, ptr2);

            CHECK_THROWS(ptr2 = impl);
            check_impl_is_constructed(impl, false, ptr1);
            check_impl_is_constructed(impl_b, true, ptr2);
        });
    }
}