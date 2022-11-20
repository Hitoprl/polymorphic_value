#include <gtest/gtest.h>

#include "polymorphic_value.h"

#include <cstdlib>
#include <iostream>
#include <new>
#include <string>

using namespace pmv;

static int new_call_counter = 0;
static int delete_call_counter = 0;
static bool enable_allocator_counters = false;

void* operator new(std::size_t count)
{
    if (enable_allocator_counters) {
        ++new_call_counter;
    }
    void* const ret = std::malloc(count);
    if (!ret) {
        throw std::bad_alloc{};
    }
    return ret;
}

void operator delete(void* ptr, std::size_t) noexcept
{
    if (enable_allocator_counters) {
        ++delete_call_counter;
    }
    return std::free(ptr);
}

void operator delete(void* ptr) noexcept
{
    if (enable_allocator_counters) {
        ++delete_call_counter;
    }
    return std::free(ptr);
}

struct Base {
    virtual ~Base() = default;
    virtual int fn() = 0;
};

struct DerivedSmall : public Base {
    int fn() override { return 1; }
};

struct DerivedBig : public Base {
    int fn() override { return 2; }
    char data[sizeof(void*) * 4];
};

template<std::size_t DataSize>
struct DerivedSpecialFunctions : public Base {
    static int default_ctor_counter;
    static int non_default_ctor_counter;
    static int copy_ctor_counter;
    static int move_ctor_counter;
    static int copy_assign_counter;
    static int move_assign_counter;
    static int destructor_ctor_counter;

    static void reset_counters() noexcept
    {
        default_ctor_counter = 0;
        non_default_ctor_counter = 0;
        copy_ctor_counter = 0;
        move_ctor_counter = 0;
        copy_assign_counter = 0;
        move_assign_counter = 0;
        destructor_ctor_counter = 0;
    }

    static void expect_counters(int default_ctor_counter,
                                int non_default_ctor_counter,
                                int copy_ctor_counter,
                                int move_ctor_counter,
                                int copy_assign_counter,
                                int move_assign_counter,
                                int destructor_ctor_counter,
                                int line)
    {
        SCOPED_TRACE("From line " + std::to_string(line));
        EXPECT_EQ(default_ctor_counter, DerivedSpecialFunctions::default_ctor_counter);
        EXPECT_EQ(non_default_ctor_counter, DerivedSpecialFunctions::non_default_ctor_counter);
        EXPECT_EQ(copy_ctor_counter, DerivedSpecialFunctions::copy_ctor_counter);
        EXPECT_EQ(move_ctor_counter, DerivedSpecialFunctions::move_ctor_counter);
        EXPECT_EQ(copy_assign_counter, DerivedSpecialFunctions::copy_assign_counter);
        EXPECT_EQ(move_assign_counter, DerivedSpecialFunctions::move_assign_counter);
        EXPECT_EQ(destructor_ctor_counter, DerivedSpecialFunctions::destructor_ctor_counter);
    };

    int fn() override { return value; }

    explicit DerivedSpecialFunctions() noexcept { ++default_ctor_counter; }

    explicit DerivedSpecialFunctions(int value) noexcept : value{value}
    {
        ++non_default_ctor_counter;
    }

    ~DerivedSpecialFunctions() override { ++destructor_ctor_counter; }

    DerivedSpecialFunctions(DerivedSpecialFunctions const& o) noexcept : value{o.value}
    {
        ++copy_ctor_counter;
    }

    DerivedSpecialFunctions(DerivedSpecialFunctions&& o) noexcept : value{o.value}
    {
        ++move_ctor_counter;
    }

    DerivedSpecialFunctions& operator=(DerivedSpecialFunctions const& o) noexcept
    {
        ++copy_assign_counter;
        value = o.value;
        return *this;
    }

    DerivedSpecialFunctions& operator=(DerivedSpecialFunctions&& o) noexcept
    {
        ++move_assign_counter;
        value = o.value;
        return *this;
    }

    int value = DataSize + 3;
    char data[DataSize];
};

template<std::size_t DataSize>
int DerivedSpecialFunctions<DataSize>::default_ctor_counter = 0;
template<std::size_t DataSize>
int DerivedSpecialFunctions<DataSize>::non_default_ctor_counter = 0;
template<std::size_t DataSize>
int DerivedSpecialFunctions<DataSize>::copy_ctor_counter = 0;
template<std::size_t DataSize>
int DerivedSpecialFunctions<DataSize>::move_ctor_counter = 0;
template<std::size_t DataSize>
int DerivedSpecialFunctions<DataSize>::copy_assign_counter = 0;
template<std::size_t DataSize>
int DerivedSpecialFunctions<DataSize>::move_assign_counter = 0;
template<std::size_t DataSize>
int DerivedSpecialFunctions<DataSize>::destructor_ctor_counter = 0;

using DerivedSmallSpecialFunctions = DerivedSpecialFunctions<1>;
using DerivedBigSpecialFunctions = DerivedSpecialFunctions<sizeof(void*) * 4>;

#define EXPECT_SMALL_COUNTERS(a, b, c, d, e, f, g)                                                 \
    DerivedSmallSpecialFunctions::expect_counters(a, b, c, d, e, f, g, __LINE__)

#define EXPECT_BIG_COUNTERS(a, b, c, d, e, f, g)                                                   \
    DerivedBigSpecialFunctions::expect_counters(a, b, c, d, e, f, g, __LINE__)

TEST(polymorphic_value, SmallObject)
{
    new_call_counter = 0;
    delete_call_counter = 0;

    {
        enable_allocator_counters = true;
        polymorphic_value<Base> poly{in_place_type<DerivedSmall>};
        enable_allocator_counters = false;

        EXPECT_EQ(new_call_counter, 0);
        EXPECT_EQ(delete_call_counter, 0);
        EXPECT_EQ(poly->fn(), 1);

        enable_allocator_counters = true;
    }

    enable_allocator_counters = false;

    EXPECT_EQ(new_call_counter, 0);
    EXPECT_EQ(delete_call_counter, 0);
}

TEST(polymorphic_value, BigObject)
{
    new_call_counter = 0;
    delete_call_counter = 0;

    {
        enable_allocator_counters = true;
        polymorphic_value<Base> poly{in_place_type<DerivedBig>};
        enable_allocator_counters = false;

        EXPECT_EQ(new_call_counter, 1);
        EXPECT_EQ(delete_call_counter, 0);
        EXPECT_EQ(poly->fn(), 2);

        enable_allocator_counters = true;
    }

    enable_allocator_counters = false;

    EXPECT_EQ(new_call_counter, 1);
    EXPECT_EQ(delete_call_counter, 1);
}

TEST(polymorphic_value, SmallObjectDefaultConstructor)
{
    DerivedSmallSpecialFunctions::reset_counters();
    {
        polymorphic_value<Base> poly{in_place_type<DerivedSmallSpecialFunctions>};
        EXPECT_SMALL_COUNTERS(1, 0, 0, 0, 0, 0, 0);
        EXPECT_EQ(poly->fn(), 4);
    }
    EXPECT_SMALL_COUNTERS(1, 0, 0, 0, 0, 0, 1);
}

TEST(polymorphic_value, SmallObjectNonDefaultConstructor)
{
    DerivedSmallSpecialFunctions::reset_counters();
    {
        polymorphic_value<Base> poly{in_place_type<DerivedSmallSpecialFunctions>, 7};
        EXPECT_SMALL_COUNTERS(0, 1, 0, 0, 0, 0, 0);
        EXPECT_EQ(poly->fn(), 7);
    }
    EXPECT_SMALL_COUNTERS(0, 1, 0, 0, 0, 0, 1);
}

TEST(polymorphic_value, SmallObjectCopyConstructor)
{
    DerivedSmallSpecialFunctions::reset_counters();
    {
        polymorphic_value<Base> poly1{in_place_type<DerivedSmallSpecialFunctions>, 7};
        EXPECT_SMALL_COUNTERS(0, 1, 0, 0, 0, 0, 0);
        EXPECT_EQ(poly1->fn(), 7);
        polymorphic_value<Base> poly2{poly1};
        EXPECT_SMALL_COUNTERS(0, 1, 1, 0, 0, 0, 0);
        EXPECT_EQ(poly2->fn(), 7);
    }
    EXPECT_SMALL_COUNTERS(0, 1, 1, 0, 0, 0, 2);
}

TEST(polymorphic_value, SmallObjectMoveConstructor)
{
    DerivedSmallSpecialFunctions::reset_counters();
    {
        polymorphic_value<Base> poly1{in_place_type<DerivedSmallSpecialFunctions>, 7};
        EXPECT_SMALL_COUNTERS(0, 1, 0, 0, 0, 0, 0);
        EXPECT_EQ(poly1->fn(), 7);
        polymorphic_value<Base> poly2{std::move(poly1)};
        EXPECT_SMALL_COUNTERS(0, 1, 0, 1, 0, 0, 0);
        EXPECT_EQ(poly2->fn(), 7);
    }
    EXPECT_SMALL_COUNTERS(0, 1, 0, 1, 0, 0, 2);
}

TEST(polymorphic_value, SmallObjectCopyAssignment)
{
    DerivedSmallSpecialFunctions::reset_counters();
    {
        polymorphic_value<Base> poly1{in_place_type<DerivedSmallSpecialFunctions>, 7};
        EXPECT_SMALL_COUNTERS(0, 1, 0, 0, 0, 0, 0);
        EXPECT_EQ(poly1->fn(), 7);
        polymorphic_value<Base> poly2{in_place_type<DerivedSmallSpecialFunctions>, 8};
        EXPECT_SMALL_COUNTERS(0, 2, 0, 0, 0, 0, 0);
        EXPECT_EQ(poly2->fn(), 8);
        poly1 = poly2;
        EXPECT_SMALL_COUNTERS(0, 2, 0, 0, 1, 0, 0);
        EXPECT_EQ(poly1->fn(), 8);
        EXPECT_EQ(poly2->fn(), 8);
    }
    EXPECT_SMALL_COUNTERS(0, 2, 0, 0, 1, 0, 2);
}

TEST(polymorphic_value, SmallObjectMoveAssignment)
{
    DerivedSmallSpecialFunctions::reset_counters();
    {
        polymorphic_value<Base> poly1{in_place_type<DerivedSmallSpecialFunctions>, 7};
        EXPECT_SMALL_COUNTERS(0, 1, 0, 0, 0, 0, 0);
        EXPECT_EQ(poly1->fn(), 7);
        polymorphic_value<Base> poly2{in_place_type<DerivedSmallSpecialFunctions>, 8};
        EXPECT_SMALL_COUNTERS(0, 2, 0, 0, 0, 0, 0);
        EXPECT_EQ(poly2->fn(), 8);
        poly1 = std::move(poly2);
        EXPECT_SMALL_COUNTERS(0, 2, 0, 0, 0, 1, 0);
        EXPECT_EQ(poly1->fn(), 8);
    }
    EXPECT_SMALL_COUNTERS(0, 2, 0, 0, 0, 1, 2);
}

TEST(polymorphic_value, SmallObjectCopyConstructorFromExternalObject)
{
    DerivedSmallSpecialFunctions::reset_counters();
    {
        DerivedSmallSpecialFunctions obj;
        EXPECT_SMALL_COUNTERS(1, 0, 0, 0, 0, 0, 0);
        obj.value = 8;

        polymorphic_value<Base> poly{obj};
        EXPECT_SMALL_COUNTERS(1, 0, 1, 0, 0, 0, 0);
        EXPECT_EQ(poly->fn(), 8);
    }
    EXPECT_SMALL_COUNTERS(1, 0, 1, 0, 0, 0, 2);
}

TEST(polymorphic_value, SmallObjectMoveConstructorFromExternalObject)
{
    DerivedSmallSpecialFunctions::reset_counters();
    {
        DerivedSmallSpecialFunctions obj;
        EXPECT_SMALL_COUNTERS(1, 0, 0, 0, 0, 0, 0);
        obj.value = 8;

        polymorphic_value<Base> poly{std::move(obj)};
        EXPECT_SMALL_COUNTERS(1, 0, 0, 1, 0, 0, 0);
        EXPECT_EQ(poly->fn(), 8);
    }
    EXPECT_SMALL_COUNTERS(1, 0, 0, 1, 0, 0, 2);
}

TEST(polymorphic_value, SmallObjectCopyAssignmentFromExternalObject)
{
    DerivedSmallSpecialFunctions::reset_counters();
    {
        DerivedSmallSpecialFunctions obj;
        EXPECT_SMALL_COUNTERS(1, 0, 0, 0, 0, 0, 0);
        obj.value = 8;

        polymorphic_value<Base> poly{in_place_type<DerivedSmallSpecialFunctions>};
        EXPECT_SMALL_COUNTERS(2, 0, 0, 0, 0, 0, 0);
        EXPECT_EQ(poly->fn(), 4);

        poly = obj;
        EXPECT_SMALL_COUNTERS(2, 0, 0, 0, 1, 0, 0);
        EXPECT_EQ(poly->fn(), 8);
    }
    EXPECT_SMALL_COUNTERS(2, 0, 0, 0, 1, 0, 2);
}

TEST(polymorphic_value, SmallObjectMoveAssignmentFromExternalObject)
{
    DerivedSmallSpecialFunctions::reset_counters();
    {
        DerivedSmallSpecialFunctions obj;
        EXPECT_SMALL_COUNTERS(1, 0, 0, 0, 0, 0, 0);
        obj.value = 8;

        polymorphic_value<Base> poly{in_place_type<DerivedSmallSpecialFunctions>};
        EXPECT_SMALL_COUNTERS(2, 0, 0, 0, 0, 0, 0);
        EXPECT_EQ(poly->fn(), 4);

        poly = std::move(obj);
        EXPECT_SMALL_COUNTERS(2, 0, 0, 0, 0, 1, 0);
        EXPECT_EQ(poly->fn(), 8);
    }
    EXPECT_SMALL_COUNTERS(2, 0, 0, 0, 0, 1, 2);
}

TEST(polymorphic_value, BigObjectDefaultConstructor)
{
    DerivedBigSpecialFunctions::reset_counters();
    {
        polymorphic_value<Base> poly{in_place_type<DerivedBigSpecialFunctions>};
        EXPECT_BIG_COUNTERS(1, 0, 0, 0, 0, 0, 0);
        EXPECT_EQ(poly->fn(), 35);
    }
    EXPECT_BIG_COUNTERS(1, 0, 0, 0, 0, 0, 1);
}

TEST(polymorphic_value, BigObjectNonDefaultConstructor)
{
    DerivedBigSpecialFunctions::reset_counters();
    {
        polymorphic_value<Base> poly{in_place_type<DerivedBigSpecialFunctions>, 7};
        EXPECT_BIG_COUNTERS(0, 1, 0, 0, 0, 0, 0);
        EXPECT_EQ(poly->fn(), 7);
    }
    EXPECT_BIG_COUNTERS(0, 1, 0, 0, 0, 0, 1);
}

TEST(polymorphic_value, BigObjectCopyConstructor)
{
    DerivedBigSpecialFunctions::reset_counters();
    {
        polymorphic_value<Base> poly1{in_place_type<DerivedBigSpecialFunctions>, 7};
        EXPECT_BIG_COUNTERS(0, 1, 0, 0, 0, 0, 0);
        EXPECT_EQ(poly1->fn(), 7);
        polymorphic_value<Base> poly2{poly1};
        EXPECT_BIG_COUNTERS(0, 1, 1, 0, 0, 0, 0);
        EXPECT_EQ(poly2->fn(), 7);
    }
    EXPECT_BIG_COUNTERS(0, 1, 1, 0, 0, 0, 2);
}

TEST(polymorphic_value, BigObjectMoveConstructor)
{
    DerivedBigSpecialFunctions::reset_counters();
    {
        polymorphic_value<Base> poly1{in_place_type<DerivedBigSpecialFunctions>, 7};
        EXPECT_BIG_COUNTERS(0, 1, 0, 0, 0, 0, 0);
        EXPECT_EQ(poly1->fn(), 7);
        polymorphic_value<Base> poly2{std::move(poly1)};
        // Move constructor for heap allocated objects is ellided.
        EXPECT_BIG_COUNTERS(0, 1, 0, 0, 0, 0, 0);
        EXPECT_EQ(poly2->fn(), 7);
    }
    EXPECT_BIG_COUNTERS(0, 1, 0, 0, 0, 0, 1);
}

TEST(polymorphic_value, BigObjectCopyAssignment)
{
    DerivedBigSpecialFunctions::reset_counters();
    {
        polymorphic_value<Base> poly1{in_place_type<DerivedBigSpecialFunctions>, 7};
        EXPECT_BIG_COUNTERS(0, 1, 0, 0, 0, 0, 0);
        EXPECT_EQ(poly1->fn(), 7);
        polymorphic_value<Base> poly2{in_place_type<DerivedBigSpecialFunctions>, 8};
        EXPECT_BIG_COUNTERS(0, 2, 0, 0, 0, 0, 0);
        EXPECT_EQ(poly2->fn(), 8);
        poly1 = poly2;
        EXPECT_BIG_COUNTERS(0, 2, 0, 0, 1, 0, 0);
        EXPECT_EQ(poly1->fn(), 8);
        EXPECT_EQ(poly2->fn(), 8);
    }
    EXPECT_BIG_COUNTERS(0, 2, 0, 0, 1, 0, 2);
}

TEST(polymorphic_value, BigObjectMoveAssignment)
{
    DerivedBigSpecialFunctions::reset_counters();
    {
        polymorphic_value<Base> poly1{in_place_type<DerivedBigSpecialFunctions>, 7};
        EXPECT_BIG_COUNTERS(0, 1, 0, 0, 0, 0, 0);
        EXPECT_EQ(poly1->fn(), 7);
        polymorphic_value<Base> poly2{in_place_type<DerivedBigSpecialFunctions>, 8};
        EXPECT_BIG_COUNTERS(0, 2, 0, 0, 0, 0, 0);
        EXPECT_EQ(poly2->fn(), 8);
        poly1 = std::move(poly2);
        // Move assignment operator for heap allocated objects is ellided.
        EXPECT_BIG_COUNTERS(0, 2, 0, 0, 0, 0, 0);
        EXPECT_EQ(poly1->fn(), 8);
    }
    EXPECT_BIG_COUNTERS(0, 2, 0, 0, 0, 0, 2);
}
TEST(polymorphic_value, BigObjectCopyConstructorFromExternalObject)
{
    DerivedBigSpecialFunctions::reset_counters();
    {
        DerivedBigSpecialFunctions obj;
        EXPECT_BIG_COUNTERS(1, 0, 0, 0, 0, 0, 0);
        obj.value = 7;

        polymorphic_value<Base> poly{obj};
        EXPECT_BIG_COUNTERS(1, 0, 1, 0, 0, 0, 0);
        EXPECT_EQ(poly->fn(), 7);
    }
    EXPECT_BIG_COUNTERS(1, 0, 1, 0, 0, 0, 2);
}

TEST(polymorphic_value, BigObjectMoveConstructorFromExternalObject)
{
    DerivedBigSpecialFunctions::reset_counters();
    {
        DerivedBigSpecialFunctions obj;
        EXPECT_BIG_COUNTERS(1, 0, 0, 0, 0, 0, 0);
        obj.value = 7;

        polymorphic_value<Base> poly{std::move(obj)};
        EXPECT_BIG_COUNTERS(1, 0, 0, 1, 0, 0, 0);
        EXPECT_EQ(poly->fn(), 7);
    }
    EXPECT_BIG_COUNTERS(1, 0, 0, 1, 0, 0, 2);
}

TEST(polymorphic_value, BigObjectCopyAssignmentFromExternalObject)
{
    DerivedBigSpecialFunctions::reset_counters();
    {
        DerivedBigSpecialFunctions obj;
        EXPECT_BIG_COUNTERS(1, 0, 0, 0, 0, 0, 0);
        obj.value = 7;

        polymorphic_value<Base> poly{in_place_type<DerivedBigSpecialFunctions>};
        EXPECT_BIG_COUNTERS(2, 0, 0, 0, 0, 0, 0);
        EXPECT_EQ(poly->fn(), 35);

        poly = obj;
        EXPECT_BIG_COUNTERS(2, 0, 0, 0, 1, 0, 0);
        EXPECT_EQ(poly->fn(), 7);
    }
    EXPECT_BIG_COUNTERS(2, 0, 0, 0, 1, 0, 2);
}

TEST(polymorphic_value, BigObjectMoveAssignmentFromExternalObject)
{
    DerivedBigSpecialFunctions::reset_counters();
    {
        DerivedBigSpecialFunctions obj;
        EXPECT_BIG_COUNTERS(1, 0, 0, 0, 0, 0, 0);
        obj.value = 7;

        polymorphic_value<Base> poly{in_place_type<DerivedBigSpecialFunctions>};
        EXPECT_BIG_COUNTERS(2, 0, 0, 0, 0, 0, 0);
        EXPECT_EQ(poly->fn(), 35);

        poly = std::move(obj);
        EXPECT_BIG_COUNTERS(2, 0, 0, 0, 0, 1, 0);
        EXPECT_EQ(poly->fn(), 7);
    }
    EXPECT_BIG_COUNTERS(2, 0, 0, 0, 0, 1, 2);
}

TEST(polymorphic_value, CopyAssignmentFromSmallToBig)
{
    new_call_counter = 0;
    delete_call_counter = 0;
    DerivedSmallSpecialFunctions::reset_counters();
    DerivedBigSpecialFunctions::reset_counters();
    {
        enable_allocator_counters = true;
        polymorphic_value<Base> poly1{in_place_type<DerivedSmallSpecialFunctions>};
        enable_allocator_counters = false;
        EXPECT_SMALL_COUNTERS(1, 0, 0, 0, 0, 0, 0);
        EXPECT_EQ(new_call_counter, 0);
        EXPECT_EQ(delete_call_counter, 0);
        EXPECT_EQ(poly1->fn(), 4);

        enable_allocator_counters = true;
        polymorphic_value<Base> poly2{in_place_type<DerivedBigSpecialFunctions>};
        enable_allocator_counters = false;
        EXPECT_BIG_COUNTERS(1, 0, 0, 0, 0, 0, 0);
        EXPECT_EQ(new_call_counter, 1);
        EXPECT_EQ(delete_call_counter, 0);
        EXPECT_EQ(poly2->fn(), 35);

        enable_allocator_counters = true;
        poly2 = poly1;
        enable_allocator_counters = false;
        EXPECT_SMALL_COUNTERS(1, 0, 1, 0, 0, 0, 0);
        EXPECT_BIG_COUNTERS(1, 0, 0, 0, 0, 0, 1);
        EXPECT_EQ(new_call_counter, 1);
        EXPECT_EQ(delete_call_counter, 1);
        EXPECT_EQ(poly1->fn(), 4);
        EXPECT_EQ(poly2->fn(), 4);

        enable_allocator_counters = true;
    }
    enable_allocator_counters = false;
    EXPECT_SMALL_COUNTERS(1, 0, 1, 0, 0, 0, 2);
    EXPECT_BIG_COUNTERS(1, 0, 0, 0, 0, 0, 1);
    EXPECT_EQ(new_call_counter, 1);
    EXPECT_EQ(delete_call_counter, 1);
}

TEST(polymorphic_value, CopyAssignmentFromBigToSmall)
{
    new_call_counter = 0;
    delete_call_counter = 0;
    DerivedSmallSpecialFunctions::reset_counters();
    DerivedBigSpecialFunctions::reset_counters();
    {
        enable_allocator_counters = true;
        polymorphic_value<Base> poly1{in_place_type<DerivedSmallSpecialFunctions>};
        enable_allocator_counters = false;
        EXPECT_SMALL_COUNTERS(1, 0, 0, 0, 0, 0, 0);
        EXPECT_EQ(new_call_counter, 0);
        EXPECT_EQ(delete_call_counter, 0);
        EXPECT_EQ(poly1->fn(), 4);

        enable_allocator_counters = true;
        polymorphic_value<Base> poly2{in_place_type<DerivedBigSpecialFunctions>};
        enable_allocator_counters = false;
        EXPECT_BIG_COUNTERS(1, 0, 0, 0, 0, 0, 0);
        EXPECT_EQ(new_call_counter, 1);
        EXPECT_EQ(delete_call_counter, 0);
        EXPECT_EQ(poly2->fn(), 35);

        enable_allocator_counters = true;
        poly1 = poly2;
        enable_allocator_counters = false;
        EXPECT_SMALL_COUNTERS(1, 0, 0, 0, 0, 0, 1);
        EXPECT_BIG_COUNTERS(1, 0, 1, 0, 0, 0, 0);
        EXPECT_EQ(new_call_counter, 2);
        EXPECT_EQ(delete_call_counter, 0);
        EXPECT_EQ(poly1->fn(), 35);
        EXPECT_EQ(poly2->fn(), 35);

        enable_allocator_counters = true;
    }
    enable_allocator_counters = false;
    EXPECT_SMALL_COUNTERS(1, 0, 0, 0, 0, 0, 1);
    EXPECT_BIG_COUNTERS(1, 0, 1, 0, 0, 0, 2);
    EXPECT_EQ(new_call_counter, 2);
    EXPECT_EQ(delete_call_counter, 2);
}

TEST(polymorphic_value, MoveAssignmentFromSmallToBig)
{
    new_call_counter = 0;
    delete_call_counter = 0;
    DerivedSmallSpecialFunctions::reset_counters();
    DerivedBigSpecialFunctions::reset_counters();
    {
        enable_allocator_counters = true;
        polymorphic_value<Base> poly1{in_place_type<DerivedSmallSpecialFunctions>};
        enable_allocator_counters = false;
        EXPECT_SMALL_COUNTERS(1, 0, 0, 0, 0, 0, 0);
        EXPECT_EQ(new_call_counter, 0);
        EXPECT_EQ(delete_call_counter, 0);
        EXPECT_EQ(poly1->fn(), 4);

        enable_allocator_counters = true;
        polymorphic_value<Base> poly2{in_place_type<DerivedBigSpecialFunctions>};
        enable_allocator_counters = false;
        EXPECT_BIG_COUNTERS(1, 0, 0, 0, 0, 0, 0);
        EXPECT_EQ(new_call_counter, 1);
        EXPECT_EQ(delete_call_counter, 0);
        EXPECT_EQ(poly2->fn(), 35);

        enable_allocator_counters = true;
        poly2 = std::move(poly1);
        enable_allocator_counters = false;
        EXPECT_SMALL_COUNTERS(1, 0, 0, 1, 0, 0, 0);
        EXPECT_BIG_COUNTERS(1, 0, 0, 0, 0, 0, 1);
        EXPECT_EQ(new_call_counter, 1);
        EXPECT_EQ(delete_call_counter, 1);
        EXPECT_EQ(poly2->fn(), 4);

        enable_allocator_counters = true;
    }
    enable_allocator_counters = false;
    EXPECT_SMALL_COUNTERS(1, 0, 0, 1, 0, 0, 2);
    EXPECT_BIG_COUNTERS(1, 0, 0, 0, 0, 0, 1);
    EXPECT_EQ(new_call_counter, 1);
    EXPECT_EQ(delete_call_counter, 1);
}

TEST(polymorphic_value, MoveAssignmentFromBigToSmall)
{
    new_call_counter = 0;
    delete_call_counter = 0;
    DerivedSmallSpecialFunctions::reset_counters();
    DerivedBigSpecialFunctions::reset_counters();
    {
        enable_allocator_counters = true;
        polymorphic_value<Base> poly1{in_place_type<DerivedSmallSpecialFunctions>};
        enable_allocator_counters = false;
        EXPECT_SMALL_COUNTERS(1, 0, 0, 0, 0, 0, 0);
        EXPECT_EQ(new_call_counter, 0);
        EXPECT_EQ(delete_call_counter, 0);
        EXPECT_EQ(poly1->fn(), 4);

        enable_allocator_counters = true;
        polymorphic_value<Base> poly2{in_place_type<DerivedBigSpecialFunctions>};
        enable_allocator_counters = false;
        EXPECT_BIG_COUNTERS(1, 0, 0, 0, 0, 0, 0);
        EXPECT_EQ(new_call_counter, 1);
        EXPECT_EQ(delete_call_counter, 0);
        EXPECT_EQ(poly2->fn(), 35);

        enable_allocator_counters = true;
        poly1 = std::move(poly2);
        enable_allocator_counters = false;
        EXPECT_SMALL_COUNTERS(1, 0, 0, 0, 0, 0, 1);
        // Move constructor ellided
        EXPECT_BIG_COUNTERS(1, 0, 0, 0, 0, 0, 0);
        EXPECT_EQ(new_call_counter, 1);
        EXPECT_EQ(delete_call_counter, 0);
        EXPECT_EQ(poly1->fn(), 35);

        enable_allocator_counters = true;
    }
    enable_allocator_counters = false;
    EXPECT_SMALL_COUNTERS(1, 0, 0, 0, 0, 0, 1);
    EXPECT_BIG_COUNTERS(1, 0, 0, 0, 0, 0, 1);
    EXPECT_EQ(new_call_counter, 1);
    EXPECT_EQ(delete_call_counter, 1);
}

TEST(polymorphic_value, CopyAssignmentFromExternalSmallToBig)
{
    new_call_counter = 0;
    delete_call_counter = 0;
    DerivedSmallSpecialFunctions::reset_counters();
    DerivedBigSpecialFunctions::reset_counters();
    {
        DerivedSmallSpecialFunctions obj;
        EXPECT_SMALL_COUNTERS(1, 0, 0, 0, 0, 0, 0);

        enable_allocator_counters = true;
        polymorphic_value<Base> poly{in_place_type<DerivedBigSpecialFunctions>};
        enable_allocator_counters = false;
        EXPECT_BIG_COUNTERS(1, 0, 0, 0, 0, 0, 0);
        EXPECT_EQ(new_call_counter, 1);
        EXPECT_EQ(delete_call_counter, 0);
        EXPECT_EQ(poly->fn(), 35);

        enable_allocator_counters = true;
        poly = obj;
        enable_allocator_counters = false;
        EXPECT_SMALL_COUNTERS(1, 0, 1, 0, 0, 0, 0);
        EXPECT_BIG_COUNTERS(1, 0, 0, 0, 0, 0, 1);
        EXPECT_EQ(new_call_counter, 1);
        EXPECT_EQ(delete_call_counter, 1);
        EXPECT_EQ(poly->fn(), 4);

        enable_allocator_counters = true;
    }
    enable_allocator_counters = false;
    EXPECT_SMALL_COUNTERS(1, 0, 1, 0, 0, 0, 2);
    EXPECT_BIG_COUNTERS(1, 0, 0, 0, 0, 0, 1);
    EXPECT_EQ(new_call_counter, 1);
    EXPECT_EQ(delete_call_counter, 1);
}

TEST(polymorphic_value, CopyAssignmentFromExternalBigToSmall)
{
    new_call_counter = 0;
    delete_call_counter = 0;
    DerivedSmallSpecialFunctions::reset_counters();
    DerivedBigSpecialFunctions::reset_counters();
    {
        enable_allocator_counters = true;
        polymorphic_value<Base> poly{in_place_type<DerivedSmallSpecialFunctions>};
        enable_allocator_counters = false;
        EXPECT_SMALL_COUNTERS(1, 0, 0, 0, 0, 0, 0);
        EXPECT_EQ(new_call_counter, 0);
        EXPECT_EQ(delete_call_counter, 0);
        EXPECT_EQ(poly->fn(), 4);

        DerivedBigSpecialFunctions obj;
        EXPECT_BIG_COUNTERS(1, 0, 0, 0, 0, 0, 0);

        enable_allocator_counters = true;
        poly = obj;
        enable_allocator_counters = false;
        EXPECT_SMALL_COUNTERS(1, 0, 0, 0, 0, 0, 1);
        EXPECT_BIG_COUNTERS(1, 0, 1, 0, 0, 0, 0);
        EXPECT_EQ(new_call_counter, 1);
        EXPECT_EQ(delete_call_counter, 0);
        EXPECT_EQ(poly->fn(), 35);

        enable_allocator_counters = true;
    }
    enable_allocator_counters = false;
    EXPECT_SMALL_COUNTERS(1, 0, 0, 0, 0, 0, 1);
    EXPECT_BIG_COUNTERS(1, 0, 1, 0, 0, 0, 2);
    EXPECT_EQ(new_call_counter, 1);
    EXPECT_EQ(delete_call_counter, 1);
}

TEST(polymorphic_value, MoveAssignmentFromExternalSmallToBig)
{
    new_call_counter = 0;
    delete_call_counter = 0;
    DerivedSmallSpecialFunctions::reset_counters();
    DerivedBigSpecialFunctions::reset_counters();
    {
        DerivedSmallSpecialFunctions obj;
        EXPECT_SMALL_COUNTERS(1, 0, 0, 0, 0, 0, 0);

        enable_allocator_counters = true;
        polymorphic_value<Base> poly{in_place_type<DerivedBigSpecialFunctions>};
        enable_allocator_counters = false;
        EXPECT_BIG_COUNTERS(1, 0, 0, 0, 0, 0, 0);
        EXPECT_EQ(new_call_counter, 1);
        EXPECT_EQ(delete_call_counter, 0);
        EXPECT_EQ(poly->fn(), 35);

        enable_allocator_counters = true;
        poly = std::move(obj);
        enable_allocator_counters = false;
        EXPECT_SMALL_COUNTERS(1, 0, 0, 1, 0, 0, 0);
        EXPECT_BIG_COUNTERS(1, 0, 0, 0, 0, 0, 1);
        EXPECT_EQ(new_call_counter, 1);
        EXPECT_EQ(delete_call_counter, 1);
        EXPECT_EQ(poly->fn(), 4);

        enable_allocator_counters = true;
    }
    enable_allocator_counters = false;
    EXPECT_SMALL_COUNTERS(1, 0, 0, 1, 0, 0, 2);
    EXPECT_BIG_COUNTERS(1, 0, 0, 0, 0, 0, 1);
    EXPECT_EQ(new_call_counter, 1);
    EXPECT_EQ(delete_call_counter, 1);
}

TEST(polymorphic_value, MoveAssignmentFromExternalBigToSmall)
{
    new_call_counter = 0;
    delete_call_counter = 0;
    DerivedSmallSpecialFunctions::reset_counters();
    DerivedBigSpecialFunctions::reset_counters();
    {
        enable_allocator_counters = true;
        polymorphic_value<Base> poly{in_place_type<DerivedSmallSpecialFunctions>};
        enable_allocator_counters = false;
        EXPECT_SMALL_COUNTERS(1, 0, 0, 0, 0, 0, 0);
        EXPECT_EQ(new_call_counter, 0);
        EXPECT_EQ(delete_call_counter, 0);
        EXPECT_EQ(poly->fn(), 4);

        DerivedBigSpecialFunctions obj;
        EXPECT_BIG_COUNTERS(1, 0, 0, 0, 0, 0, 0);

        enable_allocator_counters = true;
        poly = std::move(obj);
        enable_allocator_counters = false;
        EXPECT_SMALL_COUNTERS(1, 0, 0, 0, 0, 0, 1);
        EXPECT_BIG_COUNTERS(1, 0, 0, 1, 0, 0, 0);
        EXPECT_EQ(new_call_counter, 1);
        EXPECT_EQ(delete_call_counter, 0);
        EXPECT_EQ(poly->fn(), 35);

        enable_allocator_counters = true;
    }
    enable_allocator_counters = false;
    EXPECT_SMALL_COUNTERS(1, 0, 0, 0, 0, 0, 1);
    EXPECT_BIG_COUNTERS(1, 0, 0, 1, 0, 0, 2);
    EXPECT_EQ(new_call_counter, 1);
    EXPECT_EQ(delete_call_counter, 1);
}
