#pragma once

#include <fmt/ranges.h>
#include <fmt/std.h>
#include <oxen/log/format.hpp>
#include <oxen/quic/format.hpp>
#include <oxen/quic/formattable.hpp>

namespace llarp
{
    using namespace std::literals;
    using namespace oxen::log::literals;

}  // namespace llarp

namespace fmt
{
    // Make sure that fmt doesn't interpret our custom formattable types as range formattable, which
    // results in ambiguous overloads:
    template <oxen::quic::ToStringFormattable T>
    struct is_range<T, char>
    {
        static constexpr bool value = false;
    };
}  // namespace fmt

// fmt added optional support in version 10.0.0
#if FMT_VERSION < 100000

#include <optional>

namespace fmt
{
    template <typename T, typename Char>
    struct formatter<std::optional<T>, Char, std::enable_if_t<is_formattable<T, Char>::value>>
    {
      private:
        formatter<T, Char> underlying_;
        static constexpr basic_string_view<Char> optional =
            detail::string_literal<Char, 'o', 'p', 't', 'i', 'o', 'n', 'a', 'l', '('>{};
        static constexpr basic_string_view<Char> none = detail::string_literal<Char, 'n', 'o', 'n', 'e'>{};

        template <class U>
        FMT_CONSTEXPR static auto maybe_set_debug_format(U& u, bool set) -> decltype(u.set_debug_format(set))
        {
            u.set_debug_format(set);
        }

        template <class U>
        FMT_CONSTEXPR static void maybe_set_debug_format(U&, ...)
        {}

      public:
        template <typename ParseContext>
        FMT_CONSTEXPR auto parse(ParseContext& ctx)
        {
            maybe_set_debug_format(underlying_, true);
            return underlying_.parse(ctx);
        }

        template <typename FormatContext>
        auto format(const std::optional<T>& opt, FormatContext& ctx) const -> decltype(ctx.out())
        {
            if (!opt)
                return detail::write<Char>(ctx.out(), none);

            auto out = ctx.out();
            out = detail::write<Char>(out, optional);
            ctx.advance_to(out);
            out = underlying_.format(*opt, ctx);
            return detail::write(out, ')');
        }
    };
}  //  namespace fmt

#endif
