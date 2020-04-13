# SboPtr
C++ smart pointer type with configurable small buffer storage
and value semantics.

## Installation
The library is a single header file, so you can just steal that.
Using conan, clone the repository and run `conan create . sboptr/0.1`.

## Example
```c++
struct interface {
	// Virtual destructor is needed
	virtual ~interface() = default;

	virtual int foo() = 0;
};

struct small_impl : public interface {
	int value;
	small_impl(int value) : value{value} {}
	int foo() override { return value; }
};

struct big_impl : public interface {
	std::array<int, 500> value;
	int foo() override { return value[0]; }
};

using my_pointer_type = sboptr::basic_sbo_ptr<
	interface,
	/* size of small buffer */ sizeof(void*) * 2,
	/* enable move */ true,
	/* enable copy */ true,
	/* enable heap storage */ true>;

// Does not allocate
my_pointer_type a = small_impl{10};
std::cout << a->foo() << "\n"; // prints 10

// Heap allocation
a = big_impl{{50}};

// Copies the pointed-to value
auto b = a;
std::cout << std::boolalpha;
std::cout << (b == a) << "\n"; // prints false
std::cout << (b.get() == a.get()) << "\n"; // prints false

struct no_copy_impl : public interface {
	std::unique_ptr<int> value;
	int foo() override { return *value; }
};

// Compile error; copy-constructible type is required
/* my_pointer_type = no_copy_impl{}; */

// Define another pointer type that can not be copied
// and never allocates;
// Equivalent to basic_sbo_ptr<T, size, true, false, false>
using other_pointer_type = sboptr::unique_no_alloc_sbo_ptr<interface, sizeof(void*) * 2>;

other_pointer_type c = no_copy_impl{std::make_unique<int>(20)};

// Compile error - no copy constructor
/* auto d = c; */

// Compile error - too big to fit into the small buffer and heap allocations
// are disabled
/* other_pointer_type e = big_impl{}; */

c.reset();

// In-place construction
c.emplace<small_impl>(20);
other_pointer_type f{std::in_place_type<small_impl>, 30};
```

## How is this different from available type erasure libraries?
There are many mature type erasure libraries which provide configurable
small buffer storage. The advantage of sboptr is that it is (almost) a drop-in 
replacement for standard smart pointer types, allowing the use of normal
virtual interfaces/base classes. The disadvantage is the larger size:
2 pointers + sbo size. This is because it needs to store a separate vtable
pointer for special member functions (and your implementation classes
will also contain the built-in base class vtable pointer).
