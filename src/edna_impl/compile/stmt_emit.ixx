module;

#include <string>
#include <string_view>
// #include <vector>
// #include <flat_map>

export module edna.compile.stmt_emit;

import edna.frontend.ast;
import edna.runtime.bytecode;
import edna.compile.context;

namespace Edna::Compile {
    export class FunctionDeclEmitter : public StmtEmitterBase {
    private:
        // todo: implement!

    public:
        [[nodiscard]] bool emit(CompileContext& c, const Frontend::StmtNode& stmt, const std::string& source) override {
            const auto& [stmt_data, stmt_line, stmt_trailing, stmt_tag] = stmt;

            c.report_error("Unsupported feature: function declarations not yet supported.", stmt_line);

            return false;
        }
    };

    export class VarsEmitter : public StmtEmitterBase {
    private:
        [[nodiscard]] bool emit_var_decl(CompileContext& c, const Frontend::VarDecl& var_decl, const std::string& source) {
            const auto& [name_token, initializer_expr] = var_decl;
            std::string name_lexeme = name_token.as_string_from(source);

            if (c.needs_prepass) {
                auto local_info = c.record_local_symbol(name_lexeme);

                c.encode_instruction(Runtime::Opcode::push_null);
                c.encode_instruction(Runtime::Opcode::set_local, local_info->id);

                return true;
            }

            {
                FlagGuard<std::string> current_name_guard {&c.current_name, name_lexeme};
                
                if (!c.emit_expr(*initializer_expr, source)) {   
                    return false;
                }
                
                auto initialized_local_info = c.lookup_local_symbol(name_lexeme);
                
                if (!initialized_local_info) {
                    return false;
                }

                c.encode_instruction(Runtime::Opcode::set_local, initialized_local_info->id);
            }

            return true;
        }

    public:
        [[nodiscard]] bool emit(CompileContext& c, const Frontend::StmtNode& stmt, const std::string& source) override {
            const auto& [stmt_data, stmt_line, stmt_trailing, stmt_tag] = stmt;
            const auto& [var_decls, has_mut] = std::get<Frontend::Vars>(stmt_data); // todo: check names assigned to for mut...

            for (const auto& var_decl : var_decls) {
                if (!emit_var_decl(c, var_decl, source)) {
                    return false;
                }
            }

            return true;
        }

    };

    export class SymbolDefEmitter : public StmtEmitterBase {
    private:
        // todo: implement!

    public:
        [[nodiscard]] bool emit(CompileContext& c, const Frontend::StmtNode& stmt, const std::string& source) override {
            const auto& [stmt_data, stmt_line, stmt_trailing, stmt_tag] = stmt;

            c.report_error("Unsupported feature: custom operator declarations not yet supported.", stmt_line);

            return false;
        }
    };

    export class ExprStmtEmitter : public StmtEmitterBase {
    public:
        [[nodiscard]] bool emit(CompileContext& c, const Frontend::StmtNode& stmt, const std::string& source) override {
            const auto& [stmt_data, stmt_line, stmt_trailing, stmt_tag] = stmt;
            const auto& [expr_stmt_inner_expr] = std::get<Frontend::ExprStmt>(stmt_data);

            return c.emit_expr(*expr_stmt_inner_expr, source);
        }
    };
}
