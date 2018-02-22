#pragma once

#include <memory>

template <typename T>
struct ZeroAfterFreeAllocator : public std::allocator<T>
{
    // MSVC8 default copy constructor is broken
    typedef std::allocator<T> base;
    typedef typename base::size_type size_type;
    typedef typename base::difference_type difference_type;
    typedef typename base::pointer pointer;
    typedef typename base::const_pointer const_pointer;
    typedef typename base::reference reference;
    typedef typename base::const_reference const_reference;
    typedef typename base::value_type value_type;

    ///@brief constructors and destructor
    ZeroAfterFreeAllocator() noexcept
    {
    }
    ZeroAfterFreeAllocator(const ZeroAfterFreeAllocator& a) noexcept
        : base(a)
    {
    }
    template <typename U>
    ZeroAfterFreeAllocator(const ZeroAfterFreeAllocator<U>& a) noexcept
        : base(a)
    {
    }
    ~ZeroAfterFreeAllocator() noexcept
    {
    }
    template <typename _Other>
    struct rebind
    {
        typedef ZeroAfterFreeAllocator<_Other> other;
    };

    void deallocate(T* p, std::size_t n)
    {
        std::allocator<T>::deallocate(p, n);
    }
};
