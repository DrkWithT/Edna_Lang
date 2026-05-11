module;

#include <utility>
#include <type_traits>
#include <memory>
#include <string_view>
#include <array>
#include <vector>
#include <iostream>
#include <format>
#include <print>

export module edna.frontend.parser;

//? Pratt Parser article: https://discord.com/channels/331718482485837825/1358502198321090620/1488040819729498203

import edna.frontend.lexer;
export import edna.frontend.ast;

namespace Edna::Frontend {
    static constexpr std::array<std::string_view, static_cast<std::size_t>(ExprTag::last)> expr_names {
        "atom-expression",
        "lambda-expression",
        "block-expression",
        "cond-expression",
        "lhs-expression",
        "call-expression",
        "unary-expression",
        "binary-expression",
        "assign-expression"
    };

    static constexpr std::array<std::string_view, static_cast<std::size_t>(StmtTag::last)> stmt_names {
        "var-declaration",
        "symbol-definition",
        "expression-statement",
        "top-level"
    };

    struct ParseInfo {
        int feature_begin;
        int feature_end;
        std::uint8_t syntax_id;
        bool id_for_expr;

        friend std::string_view parseinfo_syntax_name(const ParseInfo& info) noexcept {
            if (const auto syntax_id = info.syntax_id; info.id_for_expr) {
                return expr_names[syntax_id];
            } else {
                return stmt_names[syntax_id];
            }
        }
    };

    struct ExprInfoOpt {};
    struct StmtInfoOpt {};

    class ParseGuard {
    private:
        std::vector<ParseInfo>* m_info_stack_p;

    public:
        template <typename Opt> requires (std::is_same_v<Opt, ExprInfoOpt> || std::is_same_v<Opt, StmtInfoOpt>)
        ParseGuard(std::vector<ParseInfo>& syntax_info_stack, int syntax_begin, std::uint8_t syntax_enum_id, [[maybe_unused]] Opt option)
        : m_info_stack_p {&syntax_info_stack} {
            constexpr auto info_for_expr = std::is_same_v<Opt, ExprInfoOpt>;

            m_info_stack_p->emplace_back(ParseInfo {
                .feature_begin = syntax_begin,
                .feature_end = syntax_begin, //? NOTE: this must be updated via `Parser::m_infos.back()` within each `Parser::consume()` call.
                .syntax_id = syntax_enum_id,
                .id_for_expr = info_for_expr
            });
        }

        [[nodiscard]] const ParseInfo& info() const noexcept {
            return m_info_stack_p->back();
        }

        ~ParseGuard() {
            m_info_stack_p->pop_back();
        }
    };



    export class Parser {
    private:
        std::vector<ParseInfo> m_infos;
        Token m_previous;
        Token m_current;
        int m_errors;

        //? NOTE: should use the top-most ParseInfo...
        void raise_parse_error(const Token& culprit, std::string_view source, std::string_view msg) {
            m_errors++;

            throw std::runtime_error {std::format(
                "Parse Error {} in {} at source:[ln {}, col {}]:\n\tnote: {}\n\tculprit: '{}'\n\n",
                m_errors,
                parseinfo_syntax_name(m_infos.back()),
                culprit.line,
                culprit.col,
                msg,
                culprit.as_str_from(source)
            )};
        }

        [[nodiscard]] bool at_eof() const noexcept {
            return m_current.tag == TokenTag::eof;
        }

        [[nodiscard]] constexpr bool match_token(std::same_as<TokenTag> auto first, std::same_as<TokenTag> auto ... rest) const noexcept {
            return ((m_current.tag == first) || ... || (m_current.tag == rest));
        }

        [[nodiscard]] Token advance(Lexer& lexer, std::string_view source) {
            Token temp;

            do {
                temp = lexer(source);

                if (temp.tag == TokenTag::spaces || temp.tag == TokenTag::comment) {
                    continue;
                }

                break;
            } while (temp.tag != TokenTag::eof);

            return temp;
        }

        void consume(Lexer& lexer, std::string_view source) {
            m_previous = m_current;
            m_current = advance(lexer, source);
        }

        void consume(Lexer& lexer, std::string_view source, std::string_view error_msg, std::same_as<TokenTag> auto first, std::same_as<TokenTag> auto ... rest) {
            if (match_token(first, rest...)) {
                m_previous = m_current;
                m_current = advance(lexer, source);
                return;
            }

            raise_parse_error(m_current, source, error_msg);
            std::unreachable();
        }

        void recover(Lexer& lexer, std::string_view source) {
            while (!at_eof()) {
                if (match_token(TokenTag::keyword_fun)) {
                    break;
                }

                consume(lexer, source);
            }
        }

        [[nodiscard]] ExprPtr parse_atom(Lexer& lexer, std::string_view source) {
            const auto current_tag = m_current.tag;

            const ParseGuard guard {m_infos, m_current.begin, std::to_underlying(ExprTag::atom), ExprInfoOpt {}};

            switch (current_tag) {
            case TokenTag::literal_null: case TokenTag::keyword_self:
            case TokenTag::literal_true: case TokenTag::literal_false:
            case TokenTag::literal_int: case TokenTag::literal_real:
            case TokenTag::literal_string: case TokenTag::literal_esc_string:
            case TokenTag::identifier: {
                consume(lexer, source);

                return std::make_unique<ExprNode>(ExprNode {
                    .data = Atom {
                        .token = m_previous
                    },
                    .line = m_previous.line,
                    .tag = ExprTag::atom
                });
            }
            case TokenTag::left_bracket: return parse_array(lexer, source);
            case TokenTag::left_brace: return parse_block(lexer, source);
            case TokenTag::keyword_cond: return parse_cond(lexer, source);
            case TokenTag::keyword_fun: return parse_lambda(lexer, source);
            case TokenTag::left_paren: {
                consume(lexer, source);
                ExprPtr temp_expr = parse_or(lexer, source);
                consume(lexer, source);

                return temp_expr;
            }
            default:
                break;
            }

            raise_parse_error(m_current, source, "Unexpected token found for literal.");
            std::unreachable();
        }

        [[nodiscard]] ExprPtr parse_array(Lexer& lexer, std::string_view source) {
            const auto array_lexeme_begin = m_current.begin;
            const auto array_line = m_current.line;

            const ParseGuard guard {m_infos, array_lexeme_begin, std::to_underlying(ExprTag::array), ExprInfoOpt {}};

            consume(lexer, source); // eat pre-checked '[' since this is only called by parse_primary()

            std::vector<ExprPtr> temp_items;

            if (!match_token(TokenTag::right_bracket)) {
                temp_items.emplace_back(parse_or(lexer, source));
            }

            while (!at_eof()) {
                if (!match_token(TokenTag::comma)) {
                    break;
                }

                consume(lexer, source);
                temp_items.emplace_back(parse_or(lexer, source));
            }

            consume(lexer, source, "Expected closing bracket ']'.", TokenTag::right_bracket);

            return std::make_unique<ExprNode>(ExprNode {
                .data = ArrayLiteral {
                    .items = std::move(temp_items)
                },
                .line = array_line,
                .tag = ExprTag::array
            });
        }

        [[nodiscard]] CondCase parse_cond_case(Lexer& lexer, std::string_view source) {
            const auto cond_case_lexeme_begin = m_current.begin;
            const auto cond_case_line = m_current.line;

            const ParseGuard guard {m_infos, cond_case_lexeme_begin, std::to_underlying(ExprTag::cond), ExprInfoOpt {}};

            consume(lexer, source, "Expected 'case' or 'else' here.", TokenTag::keyword_case, TokenTag::keyword_else);

            const auto is_final_case = m_previous.tag == TokenTag::keyword_else;
            ExprPtr condition;

            if (!is_final_case) {
                condition = parse_compare(lexer, source);
            }

            consume(lexer, source, "Expected '=>' symbol before a result expression.", TokenTag::arrow);

            ExprPtr result_expr = parse_or(lexer, source);

            return CondCase {
                .check = std::move(condition),
                .result = std::move(result_expr),
                .is_last = is_final_case
            };
        }

        [[nodiscard]] ExprPtr parse_cond(Lexer& lexer, std::string_view source) {
            const auto cond_lexeme_begin = m_current.begin;
            const auto cond_line = m_current.line;

            const ParseGuard guard {m_infos, cond_lexeme_begin, std::to_underlying(ExprTag::cond), ExprInfoOpt {}};

            consume(lexer, source, "Expected 'cond' here.", TokenTag::keyword_cond);

            std::vector<CondCase> cond_cases;

            consume(lexer, source, "Expected opening '{' of 'cond' body.", TokenTag::left_brace);

            if (!match_token(TokenTag::right_brace)) {
                cond_cases.emplace_back(parse_cond_case(lexer, source));
            }

            while (!at_eof()) {
                if (!match_token(TokenTag::comma)) {
                    break;
                }

                consume(lexer, source, "Expected ',' separator here.", TokenTag::comma);

                cond_cases.emplace_back(parse_cond_case(lexer, source));
            }

            consume(lexer, source, "Expected closing '}' of 'cond' body.", TokenTag::right_brace);

            return std::make_unique<ExprNode>(ExprNode {
                .data = Cond {
                    .cases = std::move(cond_cases)
                },
                .line = cond_line,
                .tag = ExprTag::cond
            });
        }

        [[nodiscard]] ExprPtr parse_block(Lexer& lexer, std::string_view source) {
            const auto block_lexeme_begin = m_current.begin;
            const auto block_line = m_current.line;

            const ParseGuard guard {m_infos, block_lexeme_begin, std::to_underlying(ExprTag::block), ExprInfoOpt {}};

            consume(lexer, source, "Expected opening '{' for block body.", TokenTag::left_brace);
            
            std::vector<StmtPtr> block_stmts;

            while (!at_eof()) {
                if (match_token(TokenTag::right_brace)) {
                    consume(lexer, source);
                    break;
                }

                block_stmts.emplace_back(parse_stmt(lexer, source));
            }

            if (!block_stmts.empty()) {
                block_stmts.back().get()->trailing = true;
            }

            return std::make_unique<ExprNode>(ExprNode {
                .data = Block {
                    .stmts = std::move(block_stmts)
                },
                .line = block_line,
                .tag = ExprTag::block
            });
        }

        [[nodiscard]] Param parse_lambda_param(Lexer& lexer, std::string_view source) {
            const auto lambda_param_lexeme_begin = m_current.begin;
            const auto lambda_param_line = m_current.line;
            auto lambda_param_is_pack = false;

            const ParseGuard guard {m_infos, lambda_param_lexeme_begin, std::to_underlying(ExprTag::cond), ExprInfoOpt {}};

            if (match_token(TokenTag::ellipses)) {
                lambda_param_is_pack = true;
                consume(lexer, source);
            }

            consume(lexer, source, "Expected identifier in parameter.", TokenTag::identifier);

            return Param {
                .token = m_previous,
                .is_pack = lambda_param_is_pack
            };
        }

        [[nodiscard]] ExprPtr parse_lambda(Lexer& lexer, std::string_view source) {
            const auto lambda_lexeme_begin = m_current.begin;
            const auto lambda_line = m_current.line;

            const ParseGuard guard {m_infos, lambda_lexeme_begin, std::to_underlying(ExprTag::lambda), ExprInfoOpt {}};

            std::vector<Param> lambda_params;

            consume(lexer, source, "Expected beginning 'fun' for lambda.", TokenTag::keyword_fun);
            consume(lexer, source, "Expected opening '(' of lambda parameters.", TokenTag::left_paren);

            if (!match_token(TokenTag::right_paren)) {
                lambda_params.emplace_back(parse_lambda_param(lexer, source));
            }

            while (!at_eof()) {
                if (!match_token(TokenTag::comma)) {
                    break;
                }

                consume(lexer, source);
                lambda_params.emplace_back(parse_lambda_param(lexer, source));
            }

            consume(lexer, source, "Expected closing '(' of lambda parameters.", TokenTag::right_paren);

            consume(lexer, source, "Expected '=>' before function body / expression.", TokenTag::arrow);

            ExprPtr lambda_body = parse_or(lexer, source);

            return std::make_unique<ExprNode>(ExprNode {
                .data = Lambda {
                    .params = std::move(lambda_params),
                    .body = std::move(lambda_body)
                },
                .line = lambda_line,
                .tag = ExprTag::lambda
            });
        }

        [[nodiscard]] ExprPtr parse_lhs(Lexer& lexer, std::string_view source) {
            const auto lhs_lexeme_begin = m_current.begin;
            const auto lhs_line = m_current.line;

            const ParseGuard guard {m_infos, lhs_lexeme_begin, std::to_underlying(ExprTag::lhs), ExprInfoOpt {}};

            ExprPtr lhs = parse_atom(lexer, source);

            while (!at_eof()) {
                if (match_token(TokenTag::locusor)) {
                    consume(lexer, source);
                    consume(lexer, source, "Expected identifier or integer for '.' access of member.", TokenTag::identifier, TokenTag::literal_int);

                    lhs = std::make_unique<ExprNode>(ExprNode {
                        .data = Lhs {
                            .lhs = std::move(lhs),
                            .rhs = std::make_unique<ExprNode>(ExprNode {
                                .data = Atom {
                                    .token = m_previous
                                },
                                .line = m_previous.line,
                                .tag = ExprTag::atom
                            })
                        },
                        .line = m_previous.line,
                        .tag = ExprTag::lhs
                    });
                } else {
                    break;
                }
            }

            return lhs;
        }

        [[nodiscard]] ExprPtr parse_call(Lexer& lexer, std::string_view source) {
            const auto call_lexeme_begin = m_current.begin;
            const auto call_line = m_current.line;

            const ParseGuard guard {m_infos, call_lexeme_begin, std::to_underlying(ExprTag::call), ExprInfoOpt {}};

            ExprPtr callee_expr = parse_lhs(lexer, source);

            if (match_token(TokenTag::left_paren)) {
                consume(lexer, source);
            } else {
                return callee_expr;
            }

            std::vector<ExprPtr> callee_args;

            if (!match_token(TokenTag::right_paren)) {
                callee_args.emplace_back(parse_or(lexer, source));
            }

            while (!at_eof()) {
                if (!match_token(TokenTag::comma)) {
                    break;
                }

                consume(lexer, source);
                callee_args.emplace_back(parse_or(lexer, source));
            }

            consume(lexer, source, "Expected ')' closing call arguments here.", TokenTag::right_paren);

            return std::make_unique<ExprNode>(ExprNode {
                .data = Call {
                    .args = std::move(callee_args),
                    .callee = std::move(callee_expr),
                },
                .line = call_line,
                .tag = ExprTag::call
            });
        }

        [[nodiscard]] ExprPtr parse_unary(Lexer& lexer, std::string_view source) {
            const auto unary_lexeme_begin = m_current.begin;
            const auto unary_line = m_current.line;

            const ParseGuard guard {m_infos, unary_lexeme_begin, std::to_underlying(ExprTag::unary), ExprInfoOpt {}};

            const auto unary_op_tag = ([] (TokenTag tag) noexcept {
                switch (tag) {
                    case TokenTag::op_neg: return AstOp::ast_neg;
                    case TokenTag::op_bang: return AstOp::ast_bang;
                    default: return AstOp::ast_noop;
                }
            })(m_current.tag);

            if (unary_op_tag != AstOp::ast_noop) {
                consume(lexer, source);

                ExprPtr inner_expr = parse_call(lexer, source);

                return std::make_unique<ExprNode>(ExprNode {
                    .data = Unary {
                        .inner = std::move(inner_expr),
                        .op = unary_op_tag
                    },
                    .line = unary_line,
                    .tag = ExprTag::unary
                });
            } else {
                return parse_call(lexer, source);
            }
        }

        [[nodiscard]] ExprPtr parse_factor(Lexer& lexer, std::string_view source) {
            const auto factor_lexeme_begin = m_current.begin;
            const auto factor_line = m_current.line;

            const ParseGuard guard {m_infos, factor_lexeme_begin, std::to_underlying(ExprTag::binary), ExprInfoOpt {}};
            ExprPtr lhs = parse_unary(lexer, source);

            while (!at_eof()) {
                if (!match_token(TokenTag::op_mod, TokenTag::op_mult, TokenTag::op_div)) {
                    break;
                }

                consume(lexer, source);

                const auto factor_op_tag = ([] (TokenTag tag) noexcept {
                    switch (tag) {
                    case TokenTag::op_mod: return AstOp::ast_mod;
                    case TokenTag::op_mult: return AstOp::ast_mult;
                    case TokenTag::op_div: default: return AstOp::ast_div;
                    }
                })(m_previous.tag);

                ExprPtr rhs = parse_unary(lexer, source);

                lhs = std::make_unique<ExprNode>(ExprNode {
                    .data = Binary {
                        .lhs = std::move(lhs),
                        .rhs = std::move(rhs),
                        .op = factor_op_tag
                    },
                    .line = factor_line,
                    .tag = ExprTag::binary
                });
            }

            return lhs;
        }

        [[nodiscard]] ExprPtr parse_term(Lexer& lexer, std::string_view source) {
            const auto term_lexeme_begin = m_current.begin;
            const auto term_line = m_current.line;

            const ParseGuard guard {m_infos, term_lexeme_begin, std::to_underlying(ExprTag::binary), ExprInfoOpt {}};
            ExprPtr lhs = parse_factor(lexer, source);

            while (!at_eof()) {
                if (!match_token(TokenTag::op_plus, TokenTag::op_sub)) {
                    break;
                }

                consume(lexer, source);

                const auto factor_op_tag = ([] (TokenTag tag) noexcept {
                    switch (tag) {
                    case TokenTag::op_plus: return AstOp::ast_plus;
                    case TokenTag::op_sub: default: return AstOp::ast_sub;
                    }
                })(m_previous.tag);

                ExprPtr rhs = parse_factor(lexer, source);

                lhs = std::make_unique<ExprNode>(ExprNode {
                    .data = Binary {
                        .lhs = std::move(lhs),
                        .rhs = std::move(rhs),
                        .op = factor_op_tag
                    },
                    .line = term_line,
                    .tag = ExprTag::binary
                });
            }

            return lhs;
        }

        [[nodiscard]] ExprPtr parse_equality(Lexer& lexer, std::string_view source) {
            const auto equality_lexeme_begin = m_current.begin;
            const auto equality_line = m_current.line;

            const ParseGuard guard {m_infos, equality_lexeme_begin, std::to_underlying(ExprTag::binary), ExprInfoOpt {}};
            ExprPtr lhs = parse_term(lexer, source);

            while (!at_eof()) {
                if (!match_token(TokenTag::op_equals, TokenTag::op_unequal)) {
                    break;
                }

                consume(lexer, source);

                const auto equality_op_tag = ([] (TokenTag tag) noexcept {
                    switch (tag) {
                    case TokenTag::op_equals: return AstOp::ast_equals;
                    case TokenTag::op_unequal: default: return AstOp::ast_unequal;
                    }
                })(m_previous.tag);

                ExprPtr rhs = parse_term(lexer, source);

                lhs = std::make_unique<ExprNode>(ExprNode {
                    .data = Binary {
                        .lhs = std::move(lhs),
                        .rhs = std::move(rhs),
                        .op = equality_op_tag
                    },
                    .line = equality_line,
                    .tag = ExprTag::binary
                });
            }

            return lhs;
        }

        [[nodiscard]] ExprPtr parse_compare(Lexer& lexer, std::string_view source) {
            const auto compare_lexeme_begin = m_current.begin;
            const auto compare_line = m_current.line;

            const ParseGuard guard {m_infos, compare_lexeme_begin, std::to_underlying(ExprTag::binary), ExprInfoOpt {}};
            ExprPtr lhs = parse_equality(lexer, source);

            while (!at_eof()) {
                if (!match_token(TokenTag::op_lesser, TokenTag::op_greater, TokenTag::op_lte, TokenTag::op_gte)) {
                    break;
                }

                consume(lexer, source);

                const auto compare_op_tag = ([] (TokenTag tag) noexcept {
                    switch (tag) {
                    case TokenTag::op_lesser: return AstOp::ast_lesser;
                    case TokenTag::op_greater: return AstOp::ast_greater;
                    case TokenTag::op_lte: return AstOp::ast_lte;
                    case TokenTag::op_gte: default: return AstOp::ast_gte;
                    }
                })(m_previous.tag);

                ExprPtr rhs = parse_equality(lexer, source);

                lhs = std::make_unique<ExprNode>(ExprNode {
                    .data = Binary {
                        .lhs = std::move(lhs),
                        .rhs = std::move(rhs),
                        .op = compare_op_tag
                    },
                    .line = compare_line,
                    .tag = ExprTag::binary
                });
            }

            return lhs;
        }

        [[nodiscard]] ExprPtr parse_and(Lexer& lexer, std::string_view source) {
            const auto logical_and_lexeme_begin = m_current.begin;
            const auto logical_and_line = m_current.line;

            const ParseGuard guard {m_infos, logical_and_lexeme_begin, std::to_underlying(ExprTag::binary), ExprInfoOpt {}};
            ExprPtr lhs = parse_compare(lexer, source);

            while (!at_eof()) {
                if (!match_token(TokenTag::op_and)) {
                    break;
                }

                consume(lexer, source);

                ExprPtr rhs = parse_compare(lexer, source);

                lhs = std::make_unique<ExprNode>(ExprNode {
                    .data = Binary {
                        .lhs = std::move(lhs),
                        .rhs = std::move(rhs),
                        .op = AstOp::ast_and
                    },
                    .line = logical_and_line,
                    .tag = ExprTag::binary
                });
            }

            return lhs;
        }

        [[nodiscard]] ExprPtr parse_or(Lexer& lexer, std::string_view source) {
            const auto logical_or_lexeme_begin = m_current.begin;
            const auto logical_or_line = m_current.line;

            const ParseGuard guard {m_infos, logical_or_lexeme_begin, std::to_underlying(ExprTag::binary), ExprInfoOpt {}};
            ExprPtr lhs = parse_and(lexer, source);

            while (!at_eof()) {
                if (!match_token(TokenTag::op_or)) {
                    break;
                }

                consume(lexer, source);

                ExprPtr rhs = parse_and(lexer, source);

                lhs = std::make_unique<ExprNode>(ExprNode {
                    .data = Binary {
                        .lhs = std::move(lhs),
                        .rhs = std::move(rhs),
                        .op = AstOp::ast_or
                    },
                    .line = logical_or_line,
                    .tag = ExprTag::binary
                });
            }

            return lhs;
        }

        [[nodiscard]] VarDecl parse_var_decl(Lexer& lexer, std::string_view source) {
            const auto var_decl_lexeme_begin = m_current.begin;
            const auto var_decl_line = m_current.line;

            const ParseGuard guard {m_infos, var_decl_lexeme_begin, std::to_underlying(StmtTag::var_decl), ExprInfoOpt {}};

            consume(lexer, source, "Expected identifier before initializer here.", TokenTag::identifier);
            
            const auto name_token = m_previous;

            consume(lexer, source, "Expected '=' before initializer here.", TokenTag::op_assign);

            ExprPtr initializer_expr = parse_or(lexer, source);

            return VarDecl {
                .name = name_token,
                .initializer = std::move(initializer_expr)
            };
        }

        [[nodiscard]] StmtPtr parse_vars(Lexer& lexer, std::string_view source) {
            const auto vars_lexeme_begin = m_current.begin;
            const auto vars_line = m_current.line;

            const ParseGuard guard {m_infos, vars_lexeme_begin, std::to_underlying(StmtTag::var_decl), ExprInfoOpt {}};

            consume(lexer, source, "Expected beginning 'mut' or 'let' for variable declaration(s).", TokenTag::keyword_let, TokenTag::keyword_mut);

            auto vars_are_mutable = m_previous.tag == TokenTag::keyword_mut;
            std::vector<VarDecl> variable_decls;

            variable_decls.emplace_back(parse_var_decl(lexer, source));

            while (!at_eof()) {
                if (!match_token(TokenTag::comma)) {
                    break;
                }

                consume(lexer, source);
                variable_decls.emplace_back(parse_var_decl(lexer, source));
            }

            consume(lexer, source, "Expected ';' after variables declaration.", TokenTag::semicolon);

            return std::make_unique<StmtNode>(StmtNode {
                .data = Vars {
                    .decls = std::move(variable_decls),
                    .has_mut = vars_are_mutable
                },
                .line = vars_line,
                .trailing = false,
                .tag = StmtTag::vars
            });
        }

        [[nodiscard]] StmtPtr parse_symbol_def(Lexer& lexer, std::string_view source) {
            return {};
        }

        [[nodiscard]] StmtPtr parse_expr_stmt(Lexer& lexer, std::string_view source) {
            const auto expr_stmt_lexeme_begin = m_current.begin;
            const auto expr_stmt_line = m_current.line;

            const ParseGuard guard {m_infos, expr_stmt_lexeme_begin, std::to_underlying(StmtTag::expr_stmt), ExprInfoOpt {}};

            ExprPtr lhs_expr = parse_or(lexer, source);

            const auto assign_operation = ([] (TokenTag tag) constexpr noexcept {
                switch (tag) {
                    case TokenTag::op_assign: return AstOp::ast_assign;
                    case TokenTag::op_mult_assign: return AstOp::ast_multiply_assign;
                    case TokenTag::op_div_assign: return AstOp::ast_divide_assign;
                    case TokenTag::op_plus_assign: return AstOp::ast_add_assign;
                    case TokenTag::op_minus_assign: return AstOp::ast_sub_assign;
                    default: return AstOp::ast_noop;
                }
            })(m_current.tag);

            if (assign_operation == AstOp::ast_noop) {
                consume(lexer, source, "Expected ';' after expression statement.", TokenTag::semicolon);

                return std::make_unique<StmtNode>(StmtNode {
                    .data = ExprStmt {
                        .inner = std::move(lhs_expr)
                    },
                    .line = expr_stmt_line,
                    .trailing = false,
                    .tag = StmtTag::expr_stmt
                });
            }

            consume(lexer, source);

            ExprPtr rhs_expr = parse_or(lexer, source);
            consume(lexer, source, "Expected ';' after expression statement.", TokenTag::semicolon);

            return std::make_unique<StmtNode>(StmtNode {
                .data = ExprStmt {
                    .inner = std::make_unique<ExprNode>(ExprNode {
                        .data = Assign {
                            .dest = std::move(lhs_expr),
                            .src = std::move(rhs_expr),
                            .op = assign_operation,
                        },
                        .line = expr_stmt_line,
                        .tag = ExprTag::assign
                    })
                },
                .line = expr_stmt_line,
                .trailing = false,
                .tag = StmtTag::expr_stmt
            });
        }

        //! NOTE: parses 'fun' decls as let <name> = <lambda>!
        [[nodiscard]] StmtPtr parse_function(Lexer& lexer, std::string_view source) {
            const auto function_decl_lexeme_begin = m_current.begin;
            const auto function_decl_line = m_current.line;

            const ParseGuard guard {m_infos, function_decl_lexeme_begin, std::to_underlying(StmtTag::function_decl), ExprInfoOpt {}};

            consume(lexer, source, "Expected 'fun' beginning this function definition.", TokenTag::keyword_fun);
            consume(lexer, source, "Expected identifier here.", TokenTag::identifier);

            const auto function_name_token = m_previous;

            consume(lexer, source, "Expected '(' opening parameter list here.", TokenTag::left_paren);

            std::vector<Param> function_parameters;

            if (!match_token(TokenTag::right_paren)) {
                function_parameters.emplace_back(parse_lambda_param(lexer, source));
            }

            while (!at_eof()) {
                if (!match_token(TokenTag::comma)) {
                    break;
                }

                consume(lexer, source);
                function_parameters.emplace_back(parse_lambda_param(lexer, source));
            }

            consume(lexer, source, "Expected ')' closing parameter list here.", TokenTag::right_paren);
            consume(lexer, source, "Expected '=>' before function body here.", TokenTag::arrow);

            ExprPtr function_body = parse_or(lexer, source);

            return std::make_unique<StmtNode>(StmtNode {
                .data = FunctionDecl {
                    .params = std::move(function_parameters),
                    .name_token = function_name_token,
                    .body = std::move(function_body)
                },
                .line = function_decl_line,
                .trailing = false
            });
        }

        [[nodiscard]] StmtPtr parse_stmt(Lexer& lexer, std::string_view source) {
            if (const auto prefix_token_tag = m_current.tag; prefix_token_tag == TokenTag::keyword_fun) {
                return parse_function(lexer, source);
            } else if (prefix_token_tag == TokenTag::keyword_let || prefix_token_tag == TokenTag::keyword_mut) {
                return parse_vars(lexer, source);
            } else { // todo: add custom operator symbol decl.
                return parse_expr_stmt(lexer, source);
            }
        }


    public:
        Parser(Lexer& lexer, std::string_view source)
        : m_infos {}, m_previous {}, m_current {}, m_errors {0} {
            consume(lexer, source);
        }

        [[nodiscard]] AllDecls operator()(Lexer& lexer, std::string_view source) {
            const ParseGuard guard {m_infos, 0, std::to_underlying(StmtTag::top_level), ExprInfoOpt {}};

            AllDecls decl_vec;

            while (!at_eof()) {
                try {
                    decl_vec.emplace_back(parse_stmt(lexer, source));
                } catch (const std::runtime_error& parse_exception) {
                    std::println(std::cerr, "{}", parse_exception.what());
                    recover(lexer, source);
                }
            }

            if (m_errors > 0) {
                decl_vec.clear();
            }

            return decl_vec;
        }
    };
}
