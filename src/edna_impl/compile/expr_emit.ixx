module;

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>
#include <flat_map>
#include <format>
// #include <print>

export module edna.compile.expr_emit;

import edna.frontend.ast;
import edna.runtime.bytecode;
import edna.compile.context;
import edna.runtime.callables;

namespace Edna::Compile {
    // TODO: 1: make expr emitters
    // TODO: 2: make stmt emitters
    // TODO: 3: test VM on fibonacci

    export class AtomEmitter : public ExprEmitterBase {
    private:
        [[nodiscard]] bool emit_name(CompileContext& c, const Frontend::Token& atom_token, const std::string& source) {
            std::string atom_lexeme = atom_token.as_string_from(source);

            auto [domain_id, domain_tag, name_is_key, name_is_foreign] = c.lookup_global_symbol(atom_lexeme)
                .or_else([&] () mutable { return c.lookup_local_symbol(atom_lexeme); })
                .or_else([&] constexpr noexcept -> std::optional<SymbolInfo> { return SymbolInfo {
                    .id = 0,
                    .domain = Domain::last,
                    .is_key_str = false,
                    .is_foreign = false
                }; }).value();

            if (domain_tag == Domain::global) {
                c.encode_instruction(Runtime::Opcode::push_global, domain_id);
            } else if (domain_tag == Domain::local) {
                c.encode_instruction(Runtime::Opcode::get_local, domain_id);
            } else if (atom_lexeme == c.current_name && c.within_call) {
                c.encode_instruction(Runtime::Opcode::push_callee);
            } else {
                std::string msg = std::format("Invalid type of name information, likely undeclared, found for '{}' in scope of '{}'", atom_lexeme, c.scopes.back().title);
                c.report_error(msg, atom_token.line);

                return false;
            }

            if (!c.within_assignable) {
                //? Here, this check ensures that "non-assignable" values (basically anything not in a LHS) are deeply cloned for proper computation of RHS expressions. One case of the context.within_assignable flag being set is during evaluating assignments / variable initializers.
                c.encode_instruction(Runtime::Opcode::deref);
            }

            return true;
        }

    public:
        [[nodiscard]] bool emit(CompileContext& c, const Frontend::ExprNode& expr, const std::string& source) override {
            if (c.needs_prepass) {
                return true;
            }

            const auto& [expr_data, expr_line, expr_tag] = expr;
            const auto& [atom_token] = std::get<Frontend::Atom>(expr_data);

            if (atom_token.tag == Frontend::TokenTag::identifier) {
                return emit_name(c, atom_token, source);
            }

            std::string literal_lexeme = atom_token.as_string_from(source);

            switch (atom_token.tag) {
                //? The "null" / boolean symbol will be mapped before actual bytecode compilation begins, so this should work.
                // case Frontend::TokenTag::keyword_self: c.encode_instruction(Runtime::Opcode::push_self); break;
                case Frontend::TokenTag::literal_null: {
                    c.encode_instruction(Runtime::Opcode::push_null);
                } break; case Frontend::TokenTag::literal_true: case Frontend::TokenTag::literal_false: {
                    c.encode_instruction(Runtime::Opcode::push_bool, static_cast<std::uint16_t>(atom_token.tag == Frontend::TokenTag::literal_true));
                } break;
                case Frontend::TokenTag::literal_int: {
                    auto integer_locator = c.lookup_constant_symbol(literal_lexeme)
                        .or_else([&] () mutable {
                            return c.record_constant_symbol(
                                literal_lexeme,
                                false,
                                Runtime::Value::create_from_int(std::stoi(literal_lexeme))
                            );
                        });

                    if (!integer_locator) {
                        return false;
                    }

                    c.encode_instruction(Runtime::Opcode::push_const, integer_locator->id);
                } break;
                case Frontend::TokenTag::literal_real: {
                    auto real_locator = c.lookup_constant_symbol(literal_lexeme).or_else([&] () mutable {
                        return c.record_constant_symbol(literal_lexeme, false, Runtime::Value::create_from_double(std::stod(literal_lexeme)));
                    });

                    if (!real_locator) {
                        return false;
                    }

                    c.encode_instruction(Runtime::Opcode::push_const, real_locator->id);
                } break;
                case Frontend::TokenTag::literal_string: {
                    c.report_error("String literals are not yet supported.", expr_line);
                    return false; // todo
                }
                case Frontend::TokenTag::literal_esc_string: {
                    c.report_error("Escaped string literals are not yet supported.", expr_line);
                    return false; // todo
                }
                case Frontend::TokenTag::identifier:
                    return emit_name(c, atom_token, source);
                default: break;
            }

            return true;
        }
    };

    export class CondEmitter : public ExprEmitterBase {
    private:
        struct SkipPosition {
            std::uint16_t exit_jump_pos; //? marks bytecode location of cond-terminating JUMP
            bool last; //? if the case is an 'else': these somewhat resemble default in C++ switch-cases
        };

        [[nodiscard]] std::optional<SkipPosition> emit_cond_case(CompileContext& c, const Frontend::CondCase& clause, const std::string& source) {
            const auto& [case_check, case_result, case_is_last] = clause;

            //? 0: Emit special case: else => <Expr>
            if (case_is_last) {
                if (!c.emit_expr(*case_result, source)) {
                    return {};
                }

                return SkipPosition {0, true}; //? dud value that's not errorneous BUT means an 'else'
            }

            //? 1: Emit check code to evaluate...
            if (!c.emit_expr(*case_check, source)) {
                return {};
            }

            //? 2: Emit JUMP_ELSE, record code position...
            const std::uint16_t skip_jump_ip = c.chunks.back().code.size();

            c.encode_instruction(Runtime::Opcode::jump_else, 0); //? dud jump
            // if (!c.within_assignable) {
            //     c.encode_instruction(Runtime::Opcode::pop, 1); //? the check's boolean is already used, so yeet it
            // }

            //? 3: Emit result code to evaluate, as it's fully evaluated value will remain as the one temporary from the cond.
            if (!c.emit_expr(*case_result, source)) {
                return {};
            }

            //? 4.1: Emit JUMP to skip out of the cond. Only 1 case / else is run.
            const std::uint16_t exit_jump_ip = c.chunks.back().code.size();

            c.encode_instruction(Runtime::Opcode::jump, 0); //? dud jump

            //? 4.2: Emit NOP to mark the end of the result code- It's where the current JUMP_ELSE will go to.
            const std::uint16_t end_case_body_ip = c.chunks.back().code.size();
            c.encode_instruction(Runtime::Opcode::nop);

            //? 4.3: Patch JUMP_ELSE from 2... The skip jump positions are each returned and collected for patching later, once the cond's last bytecode position is known.
            c.chunks.back().code.at(skip_jump_ip).arg = end_case_body_ip - skip_jump_ip;

            return SkipPosition {
                .exit_jump_pos = exit_jump_ip,
                .last = false
            };
        }

    public:
        [[nodiscard]] bool emit(CompileContext& c, const Frontend::ExprNode& expr, const std::string& source) override {
            const auto& [expr_data, expr_line, expr_tag] = expr; // todo: use line info for errors
            const auto& [cond_cases] = std::get<Frontend::Cond>(expr_data); //? Type: const std::vector<CondCase>&

            std::vector<SkipPosition> code_positions;

            for (const auto& cond_case_node : cond_cases) {
                if (auto skip_position_info = emit_cond_case(c, cond_case_node, source); skip_position_info) {
                    if (skip_position_info->last) {
                        ;
                    } else {
                        code_positions.emplace_back(*skip_position_info);
                    }
                } else {
                    return false;
                }
            }

            const std::uint16_t cond_end_ip = c.chunks.back().code.size();
            c.encode_instruction(Runtime::Opcode::nop);

            for (const auto& [skip_jump_ip, is_last_case] : code_positions) {
                if (!is_last_case) {
                    c.chunks.back().code.at(skip_jump_ip).arg = cond_end_ip - skip_jump_ip;
                }
            }

            return true;
        }
    };

    //! NOTE: hoist function / variable decls 1st!!
    export class BlockEmitter : public ExprEmitterBase {
    public:
        [[nodiscard]] bool emit(CompileContext& c, const Frontend::ExprNode& expr, const std::string& source) override {
            const auto& [expr_data, expr_line, expr_tag] = expr; // todo: use line info for errors
            const auto& [block_stmts] = std::get<Frontend::Block>(expr_data);

            FlagGuard<bool> block_is_body_guard {&c.within_func_body, static_cast<int>(c.scopes.size()) == c.function_body_scope_depth};
            const bool is_lexically_scoped_block = c.scopes.size() > c.function_body_scope_depth && !c.within_assignable;

            if (is_lexically_scoped_block) {
                c.scopes.emplace_back(SymbolScope {
                    .locations = c.scopes.back().locations,
                    .title = "(anonymous-block-expr)",
                    .next_local_id = c.scopes.back().next_local_id,
                    .next_const_id = c.scopes.back().next_const_id,
                });
                c.function_body_scope_depth++;
            }

            {
                FlagGuard<bool> block_inside_prepass_guard {&c.needs_prepass, true};

                for (const auto& prepass_stmt : block_stmts) {
                    if (!c.emit_stmt(*prepass_stmt, source)) {
                        c.scopes.pop_back();
                        return false;
                    }
                }
            }

            {
                FlagGuard<bool> block_inside_prepass_guard {&c.needs_prepass, false};

                for (const auto& prepass_stmt : block_stmts) {
                    if (!c.emit_stmt(*prepass_stmt, source)) {
                        c.scopes.pop_back();
                        return false;
                    }
                }
            }

            if (is_lexically_scoped_block) {
                c.function_body_scope_depth--;
                c.scopes.pop_back();
            }

            return true;
        }
    };

    export class ArrayLiteralEmitter : public ExprEmitterBase {
    public:
        // todo: implement Table (hybrid vector & map) & 
        [[nodiscard]] bool emit(CompileContext& c, const Frontend::ExprNode& expr, const std::string& source) override {
            if (c.needs_prepass) {
                return true;
            }

            const auto& [expr_data, expr_line, expr_tag] = expr; // todo: use line info for errors
            const auto& [array_items] = std::get<Frontend::ArrayLiteral>(expr_data);

            const std::uint16_t item_count = array_items.size();

            for (const auto& item_expr : array_items) {
                if (!c.emit_expr(*item_expr, source)) {
                    return false;
                }
            }

            c.encode_instruction(Runtime::Opcode::make_array, item_count);

            return true;
        }
    };

    export class LambdaEmitter : public ExprEmitterBase {
    public:
        [[nodiscard]] bool emit(CompileContext& c, const Frontend::ExprNode& expr, const std::string& source) override {
            if (c.needs_prepass) {
                return true;
            }

            const auto& [expr_data, expr_line, expr_tag] = expr; // todo: use line info for errors
            const auto& [fun_params, fun_body] = std::get<Frontend::Lambda>(expr_data);

            FlagGuard<bool> lambda_in_callable_guard {&c.within_func_body, true};
            FlagGuard<int> lambda_body_scope_depth_guard {&c.function_body_scope_depth, static_cast<int>(c.scopes.size())};

            c.chunks.emplace_back(Runtime::Chunk {
                .consts = {},
                .code = {}
            });
            c.scopes.emplace_back(SymbolScope {
                .locations = {},
                .title = c.current_name,
                .next_local_id = 1,
                .next_const_id = 0
            });
            c.function_body_scope_depth++;

            c.scopes.back().locations[c.current_name] = SymbolInfo {
                .id = 0,
                .domain = Domain::self,
                .is_key_str = false,
                .is_foreign = false
            };

            for (const auto& [param_token, param_rest] : fun_params) {
                std::string temp_param_name {param_token.as_string_from(source)};

                c.record_local_symbol(temp_param_name);
            }

            if (!c.emit_expr(*fun_body, source)) {
                return false;
            }

            c.encode_instruction(Runtime::Opcode::ret); //? implicitly return upon end of each block... MAYBE blocks will create their own call-frame-like record, allowing for blocks to be used like mini-functions that capture their parent's names?

            c.scopes.pop_back();
            c.function_body_scope_depth--;
            c.current_name.clear();

            //? Here, construct a callable on the heap, preloaded, before recording the name as a constant... finally generating a push of the lambda as a temporary.
            const auto lambda_heap_id = c.heap.store(std::make_unique<Runtime::Callable>(
                std::move(c.chunks.back()),
                static_cast<std::uint8_t>(fun_params.size()),
                false // TODO: deduce from AST::FunctionDecl::is_ctor... maybe remove ctors and just have factory functions.
            ));

            c.chunks.pop_back();

            Runtime::Value chunk_constant_lambda = Runtime::Value::create_from_id(lambda_heap_id, Runtime::HeapIdOpt {});

            if (auto constant_locator = c.record_constant_symbol(std::string {}, false, chunk_constant_lambda); constant_locator) {
                c.encode_instruction(Runtime::Opcode::push_const, constant_locator->id);
                return true;
            }

            return false;
        }
    };

    export class LhsEmitter : public ExprEmitterBase {
    private:
        std::uint16_t m_key_count;

    public:
        constexpr LhsEmitter() noexcept
        : m_key_count {0} {}

        [[nodiscard]] bool emit(CompileContext& c, const Frontend::ExprNode& expr, const std::string& source) override {
            if (c.needs_prepass && !c.within_assignable) {
                return true;
            }

            const auto& [expr_data, expr_line, expr_tag] = expr;
            const auto& [access_lhs, access_rhs] = std::get<Frontend::Lhs>(expr_data);

            int saved_key_count = 0;

            {
                FlagGuard<bool> access_guard {&c.within_access, true};
                FlagGuard<int> key_count_guard {&c.key_count, c.key_count};
                
                c.access_depth++;
                
                //? 1. Emit code for target object reference for property access of.
                if (!c.emit_expr(*access_lhs, source)) {
                    return false;
                }
                
                //? 2. Emit code for item / property accessing value.
                if (!c.emit_expr(*access_rhs, source)) {
                    return false;
                }
                
                key_count_guard.current()++;
                c.access_depth--;
                saved_key_count = key_count_guard.current();
                
                if (c.access_depth == 0 && !c.within_assignable) {
                    c.encode_instruction(Runtime::Opcode::get_prop, key_count_guard.current());

                    if (!c.within_assignable) {
                        c.encode_instruction(Runtime::Opcode::deref);
                    }
                }
            }

            //? Pass key count out of this emitter in case the member access is an assignment LHS requiring SET_PROP.
            c.key_count = saved_key_count;

            return true;
        }
    };

    export class CallEmitter : public ExprEmitterBase {
    public:
        [[nodiscard]] bool emit(CompileContext& c, const Frontend::ExprNode& expr, const std::string& source) override {
            if (c.needs_prepass) {
                return true;
            }

            const auto& [expr_data, expr_line, expr_tag] = expr;
            const auto& [call_args, call_fun] = std::get<Frontend::Call>(expr_data);

            FlagGuard<int> key_count_guard {&c.key_count, 0};

            //? Emit callee code with optionally defaulted null for 'self'.
            {
                FlagGuard<bool> call_guard {&c.within_call, true};

                //? Emit placeholder selfArg for non-method calls, placing at least 1 value below the callee reference.
                if (call_fun->tag != Frontend::ExprTag::lhs) {
                    c.encode_instruction(Runtime::Opcode::push_null);
                }

                if (!c.emit_expr(*call_fun, source)) {
                    return false;
                }
            }

            for (const auto& arg_expr : call_args) {
                if (!c.emit_expr(*arg_expr, source)) {
                    return false;
                }
            }

            c.encode_instruction(Runtime::Opcode::call_fun, static_cast<std::uint16_t>(call_args.size()));

            return true;
        }
    };

    export class UnaryEmitter : public ExprEmitterBase {
    public:
        [[nodiscard]] bool emit(CompileContext& c, const Frontend::ExprNode& expr, const std::string& source) override {
            if (c.needs_prepass && !c.within_assignable) {
                return true;
            }

            const auto& [expr_data, expr_line, expr_tag] = expr;
            const auto& [unary_inner, unary_op] = std::get<Frontend::Unary>(expr_data);

            FlagGuard<int> key_count_guard {&c.key_count, 0};

            {
                FlagGuard<bool> call_guard {&c.within_call, false};

                auto op_info = ([] (Frontend::AstOp op) constexpr noexcept -> std::optional<Runtime::Opcode> {
                    switch (op) {
                        case Frontend::AstOp::ast_neg: return Runtime::Opcode::negate_num;
                        case Frontend::AstOp::ast_bang: return Runtime::Opcode::negate_bool;
                        default: return {};
                    }
                })(unary_op);

                if (!op_info) {
                    return false;
                }

                if (!c.emit_expr(*unary_inner, source)) {
                    return false;
                }

                c.encode_instruction(*op_info);
            }

            return true;
        }
    };

    export class BinaryEmitter : public ExprEmitterBase {
    private:
        struct OpInfo {
            Runtime::Opcode op;
            bool has_lhs_first; //? is the opcode for left-associated evaluations?
        };

        [[nodiscard]] bool emit_logical_and(CompileContext& c, const Frontend::ExprNode& lhs, const Frontend::ExprNode& rhs, const std::string& source) {
            FlagGuard<int> key_count_guard {&c.key_count, 0};

            return false; // todo: implement!
        }

        [[nodiscard]] bool emit_logical_or(CompileContext& c, const Frontend::ExprNode& lhs, const Frontend::ExprNode& rhs, const std::string& source) {
            FlagGuard<int> key_count_guard {&c.key_count, 0};

            return false; // todo: implement!
        }

    public:
        [[nodiscard]] bool emit(CompileContext& c, const Frontend::ExprNode& expr, const std::string& source) override {
            if (c.needs_prepass && !c.within_assignable) {
                return true;
            }

            const auto& [expr_data, expr_line, expr_tag] = expr;
            const auto& [binary_lhs, binary_rhs, binary_op] = std::get<Frontend::Binary>(expr_data);

            FlagGuard<int> key_count_guard {&c.key_count, 0};

            {
                FlagGuard<bool> call_guard {&c.within_call, false};

                if (binary_op == Frontend::AstOp::ast_and) {
                    return emit_logical_and(c, *binary_lhs, *binary_rhs, source);
                } else if (binary_op == Frontend::AstOp::ast_or) {
                    return emit_logical_or(c, *binary_lhs, *binary_rhs, source);
                }

                auto op_info = ([] (Frontend::AstOp op) constexpr noexcept -> std::optional<OpInfo> {
                    switch (op) {
                        case Frontend::AstOp::ast_mod: return OpInfo {
                            .op = Runtime::Opcode::mod,
                            .has_lhs_first = true
                        };
                        case Frontend::AstOp::ast_mult: return OpInfo {
                            .op = Runtime::Opcode::mul,
                            .has_lhs_first = true
                        };
                        case Frontend::AstOp::ast_div: return OpInfo {
                            .op = Runtime::Opcode::div,
                            .has_lhs_first = true
                        };
                        case Frontend::AstOp::ast_plus: return OpInfo {
                            .op = Runtime::Opcode::add,
                            .has_lhs_first = true
                        };
                        case Frontend::AstOp::ast_sub: return OpInfo {
                            .op = Runtime::Opcode::sub,
                            .has_lhs_first = true
                        };
                        case Frontend::AstOp::ast_equals: return OpInfo {
                            .op = Runtime::Opcode::compare_eq,
                            .has_lhs_first = true
                        };
                        case Frontend::AstOp::ast_unequal: return OpInfo {
                            .op = Runtime::Opcode::compare_ne,
                            .has_lhs_first = true
                        };
                        case Frontend::AstOp::ast_lesser: return OpInfo {
                            .op = Runtime::Opcode::compare_lt,
                            .has_lhs_first = true
                        };
                        case Frontend::AstOp::ast_greater: return OpInfo {
                            .op = Runtime::Opcode::compare_gt,
                            .has_lhs_first = true
                        };
                        case Frontend::AstOp::ast_lte: return OpInfo {
                            .op = Runtime::Opcode::compare_lte,
                            .has_lhs_first = true
                        };
                        case Frontend::AstOp::ast_gte: return OpInfo {
                            .op = Runtime::Opcode::compare_gte,
                            .has_lhs_first = true
                        };
                        default: return {};
                    }
                })(binary_op);

                if (!op_info) {
                    return false;
                }

                auto binary_generate_ok = false;

                if (const auto [temp_opcode, opcode_is_ltr] = *op_info; opcode_is_ltr) {
                    binary_generate_ok = c.emit_expr(*binary_lhs, source) && c.emit_expr(*binary_rhs, source);
                    c.encode_instruction(temp_opcode);
                } else {
                    binary_generate_ok = c.emit_expr(*binary_rhs, source) && c.emit_expr(*binary_lhs, source);
                    c.encode_instruction(temp_opcode);
                }

                return binary_generate_ok;
            }
        }
    };

    export class AssignEmitter : public ExprEmitterBase {
    public:
        [[nodiscard]] bool emit(CompileContext& c, const Frontend::ExprNode& expr, const std::string& source) override {
            if (c.needs_prepass) {
                return true;
            }

            const auto& [expr_data, expr_line, expr_tag] = expr;
            const auto& [dest_expr, source_expr] = std::get<Frontend::Assign>(expr_data);

            {
                FlagGuard<bool> call_guard {&c.within_call, false};
                FlagGuard<bool> assign_guard {&c.within_assignable, true};

                if (!c.emit_expr(*dest_expr, source)) {
                    return false;
                }

                if (!c.emit_expr(*source_expr, source)) {
                    return false;
                }

                if (dest_expr->tag == Frontend::ExprTag::atom) {
                    c.encode_instruction(Runtime::Opcode::set_local);
                } else if (dest_expr->tag == Frontend::ExprTag::lhs) {
                    c.encode_instruction(Runtime::Opcode::set_prop, c.key_count);
                } else {
                    c.report_error("Found invalid assignment LHS- only identifier atoms or key / property accesses are valid.", expr_line);

                    return false;
                }

                c.key_count = 0;
            }

            return true;
        }
    };
}
