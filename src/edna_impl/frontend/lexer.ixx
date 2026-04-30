module;

#include <utility>
#include <string_view>
#include <string>
#include <flat_map>

export module edna.frontend.lexer;

export import edna.frontend.lexicals;

namespace Edna::Frontend {
    [[nodiscard]] constexpr bool like_disjoint(char c, std::same_as<char> auto first, std::same_as<char> auto ... rest) noexcept {
        return ((c == first) || ... || (c == rest));
    }

    [[nodiscard]] constexpr bool like_whitespace(char c) noexcept {
        return c == ' ' || c == '\t' || c == '\n' || c == '\r';
    }

    [[nodiscard]] constexpr bool like_alphabetic(char c) noexcept {
        return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || c == '_';
    }

    [[nodiscard]] constexpr bool like_digit(char c) noexcept {
        return c >= '0' && c <= '9';
    }

    [[nodiscard]] constexpr bool like_hexdigit(char c) noexcept {
        return like_digit(c) || (c >= 'A' && c <= 'F');
    }

    [[nodiscard]] constexpr bool like_alphanumeric(char c) noexcept {
        return like_alphabetic(c) || like_digit(c);
    }

    [[nodiscard]] constexpr bool like_numeric(char c) noexcept {
        return like_digit(c) || c == '.';
    }

    [[nodiscard]] constexpr bool like_symbolic(char c) noexcept {
        return like_disjoint(c, '.', '?', '%', '*', '/', '+', '-', '!', '=', '<', '>', '&', '|');
    }


    export class Lexer {
    private:
        //? NOTE: the initially inserted string_view keys MUST have static lifetimes.
        std::flat_map<std::string_view, TokenTag> m_specials;
        int m_pos;
        int m_end;
        int m_line;
        int m_col;

        [[nodiscard]] constexpr bool at_end() const noexcept {
            return m_pos >= m_end;
        }

        [[nodiscard]] constexpr char peek(std::string_view source, int peek_offset = 0) const noexcept {
            return source[m_pos + peek_offset];
        }

        void update_location(char c) noexcept {
            switch (c) {
            case '\n':
                m_line++;
                m_col = 1;
                break;
            default:
                m_col++;
                break;
            }

            ++m_pos;
        }

        [[nodiscard]] Token lex_single(std::string_view source, TokenTag tag) noexcept {
            const auto tk_begin = m_pos;
            const auto tk_line = m_line;
            const auto tk_col = m_col;

            update_location(peek(source));

            return Token {
                tk_begin,
                1,
                tk_line,
                tk_col,
                tag
            };
        }

        [[nodiscard]] Token lex_spaces(std::string_view source) noexcept {
            const auto tk_begin = m_pos;
            const auto tk_line = m_line;
            const auto tk_col = m_col;

            while (!at_end()) {
                if (const char c = peek(source); like_whitespace(c)) {
                    update_location(c);
                } else {
                    break;
                }
            }

            return Token {
                tk_begin,
                m_pos - tk_begin,
                tk_line,
                tk_col,
                TokenTag::spaces
            };
        }

        [[nodiscard]] Token lex_comment(std::string_view source) noexcept {
            m_pos++; // skip leading semicolon...

            const auto tk_begin = m_pos;
            const auto tk_line = m_line;
            const auto tk_col = m_col;

            while (!at_end()) {
                if (const char c = peek(source); c != '\n') {
                    update_location(c);
                } else {
                    break;
                }
            }

            return Token {
                tk_begin,
                m_pos - tk_begin,
                tk_line,
                tk_col,
                TokenTag::comment
            };
        }

        [[nodiscard]] Token lex_number(std::string_view source) noexcept {
            const auto tk_begin = m_pos;
            const auto tk_line = m_line;
            const auto tk_col = m_col;
            auto tk_dots = 0;

            while (!at_end()) {
                if (const char c = peek(source); like_digit(c)) {
                    update_location(c);
                } else if (c == '.') {
                    update_location(c);
                    tk_dots++;
                } else {
                    break;
                }
            }

            const auto deduced_token_tag = ([] (int dots) constexpr noexcept {
                switch (dots) {
                case 0: return TokenTag::literal_int;
                case 1: return TokenTag::literal_real;
                default: return TokenTag::unknown;
                }
            })(tk_dots);

            return Token {
                tk_begin,
                m_pos - tk_begin,
                tk_line,
                tk_col,
                deduced_token_tag
            };
        }

        [[nodiscard]] bool eat_escaped(std::string_view source) noexcept {
            update_location(peek(source)); // skip backslash...

            if (const char peek_0 = peek(source); peek_0 == 'x') {
                update_location(peek_0);
            } else if (like_disjoint(peek_0, 'b', 't', 'r', 'n')) {
                update_location(peek_0);
                return true;
            }

            if (const char peek_1 = peek(source); !like_hexdigit(peek_1)) {
                return false;
            } else {
                update_location(peek_1);
            }

            if (const char peek_2 = peek(source); !like_hexdigit(peek_2)) {
                return false;
            } else {   
                update_location(peek_2);
            }

            return true;
        }

        [[nodiscard]] Token lex_string(std::string_view source) noexcept {
            const auto tk_begin = m_pos;
            const char delim = peek(source);

            update_location(delim);

            const auto tk_line = m_line;
            const auto tk_col = m_col;
            bool closed = false;
            bool valid = true;
            bool escaped = false;

            while (!at_end() && valid) {
                if (const char c = peek(source); c == '\\') {
                    escaped = true;
                    valid = eat_escaped(source);
                    update_location(c);
                } else if (c == '\n') {
                    closed = true;
                    valid = false;
                    break;
                } else {
                    update_location(c);

                    if (c == delim) {
                        closed = true;
                        break;
                    }
                }
            }

            const auto deduced_token_tag = ([] (bool is_closed, bool has_valid_contents, bool is_escaped) constexpr noexcept {
                if (!has_valid_contents || !is_closed) {
                    return TokenTag::unknown;
                } else if (is_escaped) {
                    return TokenTag::literal_esc_string;
                } else {
                    return TokenTag::literal_string;
                }
            })(closed, valid, escaped);

            return Token {
                tk_begin,
                m_pos - tk_begin + 1,
                tk_line,
                tk_col,
                deduced_token_tag
            };
        }

        [[nodiscard]] Token lex_word(std::string_view source) {
            const auto tk_begin = m_pos;
            const auto tk_line = m_line;
            const auto tk_col = m_col;

            while (!at_end()) {
                if (const char c = peek(source); like_alphanumeric(c)) {
                    update_location(c);
                } else {
                    break;
                }
            }

            const std::string_view lexeme = source.substr(tk_begin, m_pos - tk_begin);

            const auto deduced_token_tag = ([] (const std::flat_map<std::string_view, TokenTag>& special_lexicals, std::string_view current_lexeme) constexpr noexcept {
                if (auto token_tag_it = special_lexicals.find(current_lexeme); token_tag_it != special_lexicals.end()) {
                    return token_tag_it->second;
                } else {
                    return TokenTag::identifier;
                }
            })(m_specials, lexeme);

            return Token {
                tk_begin,
                m_pos - tk_begin,
                tk_line,
                tk_col,
                deduced_token_tag
            };
        }

        [[nodiscard]] Token lex_symbolic(std::string_view source) {
            const auto tk_begin = m_pos;
            const auto tk_line = m_line;
            const auto tk_col = m_col;

            while (!at_end()) {
                if (const char c = peek(source); like_symbolic(c)) {
                    update_location(c);
                } else {
                    break;
                }
            }

            const std::string_view lexeme = source.substr(tk_begin, m_pos - tk_begin);

            const auto deduced_token_tag = ([] (const std::flat_map<std::string_view, TokenTag>& special_lexicals, std::string_view current_lexeme) constexpr noexcept {
                if (auto token_tag_it = special_lexicals.find(current_lexeme); token_tag_it != special_lexicals.end()) {
                    return token_tag_it->second;
                } else {
                    return TokenTag::op_other;
                }
            })(m_specials, lexeme);

            return Token {
                tk_begin,
                m_pos - tk_begin,
                tk_line,
                tk_col,
                deduced_token_tag
            };
        }

    public:
        explicit Lexer() noexcept
        : m_specials {}, m_pos {}, m_end {}, m_line {1}, m_col {1} {}

        void add_edna_lexical(std::string_view c_string_static, TokenTag tag) {
            m_specials[c_string_static] = tag;
        }

        void use_source(std::string_view source) noexcept {
            m_pos = 0;
            m_end = static_cast<int>(source.length());
        }

        [[nodiscard]] Token operator()(std::string_view source) {
            if (at_end()) {
                return Token {};
            }

            const auto peeked = peek(source);

            if (like_whitespace(peeked)) {
                return lex_spaces(source);
            }

            switch (peeked) {
            case '#': return lex_comment(source);
            case ';': return lex_single(source, TokenTag::semicolon);
            case ':': return lex_single(source, TokenTag::colon);
            case ',': return lex_single(source, TokenTag::comma);
            case '(': return lex_single(source, TokenTag::left_paren);
            case ')': return lex_single(source, TokenTag::right_paren);
            case '{': return lex_single(source, TokenTag::left_brace);
            case '}': return lex_single(source, TokenTag::right_brace);
            case '[': return lex_single(source, TokenTag::left_bracket);
            case ']': return lex_single(source, TokenTag::right_bracket);
            case '\'': case '\"': return lex_string(source);
            default: break;
            }

            if (like_digit(peeked)) {
                return lex_number(source);
            } else if (like_symbolic(peeked)) {
                return lex_symbolic(source);
            } else if (like_alphabetic(peeked)) {
                return lex_word(source);
            } else {
                return lex_single(source, TokenTag::unknown);
            }
        }
    };
}
