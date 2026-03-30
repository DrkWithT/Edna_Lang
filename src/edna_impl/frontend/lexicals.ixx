module;

#include <utility>
#include <string_view>
#include <string>

export module edna.frontend.lexicals;

namespace Edna::Frontend {
    export enum class TokenTag : std::uint8_t {
        unknown,
        spaces,
        comment,
        identifier,
        keyword_null,
        keyword_fun,
        keyword_uses,
        keyword_let,
        keyword_mut,
        keyword_cond,
        keyword_case,
        keyword_else,
        keyword_symbol,
        keyword_prec,
        op_neg,
        op_bang,
        op_mod,
        op_mult,
        op_div,
        op_plus,
        op_sub,
        op_equals,
        op_unequal,
        op_lesser,
        op_greater,
        op_lte,
        op_gte,
        op_assign,
        op_and,
        op_or,
        op_other,
        semicolon,
        colon,
        comma,
        arrow,
        dot,
        ellipses,
        left_paren,
        right_paren,
        left_brace,
        right_brace,
        left_bracket,
        right_bracket,
        literal_true,
        literal_false,
        literal_int,
        literal_real,
        literal_string,
        literal_esc_string,
        eof
    };

    export struct Token {
        int begin;
        int length;
        int line;
        int col;
        TokenTag tag;

        constexpr Token() noexcept
        : begin {0}, length {0}, line {1}, col {0}, tag {TokenTag::eof} {}

        constexpr Token(int begin_, int line_, int col_, TokenTag tag_) noexcept
        : begin {begin_}, length {1}, line {line_}, col {col_}, tag {tag_} {}

        constexpr Token(int begin_, int length_, int line_, int col_, TokenTag tag_) noexcept
        : begin {begin_}, length {length_}, line {line_}, col {col_}, tag {tag_} {}

        [[nodiscard]] constexpr std::string_view as_str_from(this auto&& self, std::string_view source) {
            return source.substr(self.begin, self.length);
        }

        [[nodiscard]] std::string as_string_from(this auto&& self, const std::string& source) {
            return source.substr(self.begin, self.length);
        }
    };
}
