#include <iostream>

#include "sboptr/sboptr.hpp"

namespace
{
    class interface
    {
      public:
        virtual ~interface() noexcept = default;

        virtual void foo() = 0;
    };

    class impl : public interface
    {
      public:
        void foo() override
        {
            std::cout << "Foo!\n";
        }
    };
}  // namespace

auto main() -> int
{
    auto ptr = sboptr::sbo_ptr<interface, sizeof(void*) * 4>{impl{}};
    ptr->foo();

    return 0;
}