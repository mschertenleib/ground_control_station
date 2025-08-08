#ifndef UNIQUE_RESOURCE_HPP
#define UNIQUE_RESOURCE_HPP

#include <concepts>
#include <type_traits>
#include <utility>

template <typename R>
inline constexpr R null_resource {};

// FIXME: generally fix copy/move semantics. If we really need the resource type
// to be copyable, that's not too much of a problem since non-copyable resources
// are often RAII objects themselves.
// TODO: requirements on R and D
template <typename R, typename D>
class Unique_resource
{
public:
    // TODO: noexcept based on type traits
    constexpr Unique_resource() : m_resource {null_resource<R>}, m_deleter {}
    {
    }

    // TODO: noexcept based on type traits
    template <typename RR, typename DD = D>
        requires std::constructible_from<R, RR &&> &&
                     std::constructible_from<D, DD &&> &&
                     (!std::same_as<std::remove_cvref_t<RR>, Unique_resource>)
    explicit constexpr Unique_resource(RR &&resource, DD &&deleter = DD())
        : m_resource {std::forward<RR>(resource)},
          m_deleter {std::forward<DD>(deleter)}
    {
    }

    constexpr Unique_resource(Unique_resource &&rhs) noexcept
        : m_resource {std::move(rhs.m_resource)},
          m_deleter {std::move(rhs.m_deleter)}
    {
        rhs.m_resource = null_resource<R>;
    }

    constexpr Unique_resource &operator=(Unique_resource &&rhs) noexcept
    {
        Unique_resource tmp(std::move(rhs));
        swap(tmp);
        return *this;
    }

    Unique_resource(const Unique_resource &) = delete;
    Unique_resource &operator=(const Unique_resource &) = delete;

    constexpr ~Unique_resource() noexcept
    {
        if (m_resource != null_resource<R>)
        {
            m_deleter(m_resource);
        }
    }

    [[nodiscard]] constexpr const R &get() const noexcept
    {
        return m_resource;
    }

    template <typename RR>
        requires std::constructible_from<R, RR &&>
    constexpr void reset(RR &&resource) noexcept
    {
        if (m_resource != null_resource<R>)
        {
            m_deleter(m_resource);
        }
        m_resource = std::forward<RR>(resource);
    }

    [[nodiscard]] constexpr R release() noexcept
    {
        auto resource = m_resource;
        m_resource = null_resource<R>;
        return resource;
    }

    // TODO: noexcept based on type traits?
    constexpr void swap(Unique_resource &rhs) noexcept
    {
        using std::swap;
        swap(m_resource, rhs.m_resource);
        swap(m_deleter, rhs.m_deleter);
    }

private:
    R m_resource;
    [[no_unique_address]] D m_deleter;
};

template <class RR, class DD>
Unique_resource(RR &&, DD &&)
    -> Unique_resource<std::remove_cvref_t<RR>, std::remove_cvref_t<DD>>;

#endif