module;

#include <cstdint>
#include <type_traits>
#include <utility>
#include <string>
#include <string_view>
#include <vector>
#include <flat_map>
#include <iostream>
#include <print>

export module edna.compile.context;

import edna.frontend.ast;
import edna.runtime.bytecode;

namespace Edna::Compile {
    export enum class Domain : std::uint8_t {
        immediate,
        self,
        global, //? built-in name (constant Value / ObjectBase-ID)
        constant, //? chunk constant ID
        local, //? current local value ID per stack frame (0 - 255)... Can map to a constant-ref to a heap object!
        // heap,
        last
    };

    export struct SymbolInfo {
        std::uint16_t id;
        Domain domain;
        bool is_key_str; //? symbol is a name if false... otherwise, it's an interned string literal
        bool is_foreign; //? denotes a non-local symbol, so dynamic lookup in environment objects is needed...
    };

    export struct SymbolScope {
        std::flat_map<std::string, SymbolInfo> locations;
        std::string title;
        int next_local_id;
        int next_const_id;
        bool is_of_func_body;
    };

    export template <typename T>
    class FlagGuard {
    private:
        T* m_ptr;
        T m_old;

    public:
        explicit constexpr FlagGuard(T* ptr_, T temp_value) noexcept
        : m_ptr {ptr_}, m_old {} {
            if constexpr (std::is_integral_v<T> || std::is_aggregate_v<T>) {
                *m_ptr = temp_value;
            } else {
                m_old = std::move(*m_ptr);
                *m_ptr = std::move(temp_value);
            }
        }

        [[nodiscard]] constexpr T& current() const noexcept {
            return *m_ptr;
        }

        [[nodiscard]] constexpr T old() const noexcept {
            return m_old;
        }

        ~FlagGuard() noexcept {
            if constexpr (std::is_scalar_v<T> || std::is_aggregate_v<T>) {
                *m_ptr = m_old;
            } else {
                *m_ptr = std::move(m_old);
            }
        }
    };

    export struct CompileContext;

    export class ExprEmitterBase {
    public:
        virtual ~ExprEmitterBase() = default;

        virtual bool emit(CompileContext& c, const Frontend::ExprNode& expr, const std::string& source) = 0;
    };

    export class StmtEmitterBase {
    public:
        virtual ~StmtEmitterBase() = default;

        virtual bool emit(CompileContext& c, const Frontend::StmtNode& expr, const std::string& source) = 0;
    };

    struct CompileContext {
        std::array<std::unique_ptr<ExprEmitterBase>, static_cast<std::size_t>(Frontend::ExprTag::last)> expr_emitters;
        std::array<std::unique_ptr<StmtEmitterBase>, static_cast<std::size_t>(Frontend::StmtTag::last)> stmt_emitters;

        Runtime::ObjectHeap<Runtime::Value> heap;
        std::vector<Runtime::Value> globals;
        std::vector<SymbolScope> scopes;
        std::vector<Runtime::Chunk> chunks;

        std::string current_name;

        int function_body_scope_depth;
        int access_depth;
        int key_count;
        int error_count;

        bool needs_prepass;
        bool within_func_body;
        bool within_access;
        bool within_assignable;
        bool within_call;

        CompileContext(/* ObjectHeap preloaded_heap */)
        : expr_emitters {}, stmt_emitters {}, heap {}, globals {}, scopes {}, chunks {}, current_name {}, function_body_scope_depth {0}, access_depth {0}, key_count {0}, error_count {0}, needs_prepass {false}, within_func_body {false}, within_access {false}, within_assignable {false}, within_call {false} {
            //? 1. Establish top-level scoping & codegen data for correctness. Nested scopes will make nested mappings as such.
            scopes.emplace_back(SymbolScope {
                .locations = {},
                .title = "(global-body)",
                .next_local_id = 0,
                .next_const_id = 0,
                .is_of_func_body = true
            });
            function_body_scope_depth++;

            chunks.emplace_back(Runtime::Chunk {
                .consts = {},
                .code = {}
            });

            //? 2. Establish pre-made mappings of built-in objects injected into the Edna interpreter.
            // todo
        }

        void add_expr_emitter(Frontend::ExprTag expr_tag, std::unique_ptr<ExprEmitterBase> emitter) noexcept {
            expr_emitters[std::to_underlying(expr_tag)] = std::move(emitter);
        }

        void add_stmt_emitter(Frontend::StmtTag stmt_tag, std::unique_ptr<StmtEmitterBase> emitter) noexcept {
            stmt_emitters[std::to_underlying(stmt_tag)] = std::move(emitter);
        }

        [[nodiscard]] std::optional<SymbolInfo> lookup_symbol(const std::string& symbol) {
            return lookup_global_symbol(symbol)
                .or_else([&symbol, this] () { return lookup_constant_symbol(symbol); })
                .or_else([&symbol, this] () { return lookup_local_symbol(symbol); });
        }

        [[nodiscard]] std::optional<SymbolInfo> lookup_global_symbol(const std::string& symbol) {
            const auto& global_scope = scopes.front();

            if (auto global_locus_it = global_scope.locations.find(symbol); global_locus_it != global_scope.locations.end() && global_locus_it->second.domain == Domain::global) {
                return global_locus_it->second;
            }

            return {};
        }

        [[nodiscard]] std::optional<SymbolInfo> lookup_local_symbol(const std::string& symbol) {
            const auto& current_scope = scopes.back();

            if (auto local_locus_it = current_scope.locations.find(symbol); local_locus_it != current_scope.locations.end() && local_locus_it->second.domain == Domain::local) {
                return local_locus_it->second;
            }

            return {};
        }

        [[nodiscard]] std::optional<SymbolInfo> lookup_constant_symbol(const std::string& symbol) {
            const auto& current_scope = scopes.back();

            if (auto chunk_constant_locus_it = current_scope.locations.find(symbol); chunk_constant_locus_it != current_scope.locations.end() && chunk_constant_locus_it->second.domain == Domain::constant) {
                return chunk_constant_locus_it->second;
            }

            return {};
        }

        template <typename Item>
        [[maybe_unused]] std::optional<SymbolInfo> record_global_symbol(const std::string& symbol, Item item_arg) {
            using item_type = std::remove_cvref_t<Item>;

            SymbolInfo temp_locus {};
            const int global_id = globals.size();

            if constexpr (std::is_same_v<item_type, Runtime::Value>) {
                globals.emplace_back(item_arg);

                temp_locus = SymbolInfo {
                    .id = static_cast<std::uint16_t>(global_id),
                    .domain = Domain::global,
                    .is_key_str = false,
                    .is_foreign = false
                };

                scopes.front().locations[symbol] = temp_locus;

                return temp_locus;
            } else if constexpr (std::is_same_v<item_type, std::unique_ptr<Runtime::ObjectBase<Runtime::Value>>>) {
                // todo: register object base to heap & do globals.emplace of global Value with heap_id: xxx...
                const int object_id = heap.store(std::move(item_arg));

                if (object_id == Runtime::ObjectHeap<Runtime::Value>::dud_id) {
                    return {};
                }

                Runtime::Value object_proxy_value {
                    Runtime::ValueScalarOpt {},
                    Runtime::ValueScalarHint::heap_id,
                    object_id
                };

                globals.emplace_back(object_proxy_value);

                temp_locus = SymbolInfo {
                    .id = static_cast<std::uint16_t>(global_id),
                    .domain = Domain::global,
                    .is_key_str = false,
                    .is_foreign = false
                };

                scopes.front().locations[symbol] = temp_locus;

                return temp_locus;
            } else {
                return {};
            }
        }

        template <typename Item>
        [[nodiscard]] std::optional<SymbolInfo> record_constant_symbol(const std::string& symbol, bool has_key_str_constant, Item&& item) {
            using item_type = std::remove_cvref_t<Item>;

            auto& current_scope = scopes.back();
            SymbolInfo temp_locus {};
            const int constant_id = current_scope.next_const_id;

            if constexpr (std::is_same_v<item_type, Runtime::Value>) {
                current_scope.next_const_id++;

                temp_locus = SymbolInfo {
                    .id = static_cast<std::uint16_t>(constant_id),
                    .domain = Domain::constant,
                    .is_key_str = has_key_str_constant,
                    .is_foreign = false
                };

                if (!symbol.empty()) {
                    current_scope.locations[symbol] = temp_locus;
                }

                chunks.back().consts.emplace_back(std::forward<Item>(item));

                return temp_locus;
            }

            return {};
        }

        [[maybe_unused]] std::optional<SymbolInfo> record_local_symbol(const std::string& symbol) {
            auto& current_scope = scopes.back();
            SymbolInfo temp_locus {};
            const int local_id = current_scope.next_local_id;

            temp_locus = SymbolInfo {
                .id = static_cast<std::uint16_t>(local_id),
                .domain = Domain::local,
                .is_key_str = false,
                .is_foreign = false
            };

            current_scope.locations[symbol] = temp_locus;
            current_scope.next_local_id++;

            return temp_locus;
        }

        //? Emits a no-argument bytecode instruction e.g NOOP, DUP...
        void encode_instruction(Runtime::Opcode opcode) {
            chunks.back().code.emplace_back(opcode, 0);
        }

        void encode_instruction(Runtime::Opcode opcode, std::uint16_t arg_0) {
            chunks.back().code.emplace_back(opcode, arg_0);
        }

        [[nodiscard]] bool emit_expr(const Frontend::ExprNode& expr, const std::string& source) {
            const auto& [expr_data, expr_line, expr_tag] = expr;

            if (auto& expr_emitter = expr_emitters.at(static_cast<std::size_t>(expr_tag)); expr_emitter) {
                return expr_emitter->emit(*this, expr, source); //? each emitter uses EmitterBase::emit(CompileContext& c, const NodeType& node, const std::string& source)
            }

            return false;
        }

        [[nodiscard]] bool emit_stmt(const Frontend::StmtNode& stmt, const std::string& source) {
            const auto& [stmt_data, stmt_line, stmt_trailing, stmt_tag] = stmt;

            if (auto& stmt_emitter = stmt_emitters.at(static_cast<std::size_t>(stmt_tag)); stmt_emitter) {
                return stmt_emitter->emit(*this, stmt, source); //? each emitter uses EmitterBase::emit(CompileContext& c, const NodeType& node, const std::string& source)
            }

            return false;
        }

        void report_error(std::string_view msg, int line) {
            error_count++;

            std::println(
                std::cerr,
                "Compile Error {}, [ln {}]:\nNote: {}:",
                error_count,
                line,
                msg
            );
        }
    };

    export [[nodiscard]] std::optional<Runtime::Program> compile_all(CompileContext& c, const Frontend::AllDecls& decls, const std::string& source) {
        {
            FlagGuard<bool> top_level_prepass_guard {&c.needs_prepass, true};

            for (auto decl_position = 0; const auto& decl : decls) {
                decl_position++;

                if (!c.emit_stmt(*decl, source)) {
                    std::println(std::cerr, "\n\tNote (name prepass): see declaration {} from the top.\n", decl_position);
                }
            }

            if (c.error_count > 0) {
                return {};
            }
        }

        FlagGuard<bool> top_level_prepass_guard {&c.needs_prepass, false};

        for (auto revisit_pos = 0; const auto& revisit_decl : decls) {
            revisit_pos++;

            // TODO: find out why the rest of the top-level chunk fails to generate AFTER EXECUTABLE REBUILD.
            if (!c.emit_stmt(*revisit_decl, source)) {
                std::println(std::cerr, "\n\tNote (bytecode generation pass): see declaration {} from the top.\n", revisit_pos);
            }
        }

        if (c.error_count > 0) {
            return {};
        }

        return Runtime::Program {
            .pre_heap = std::move(c.heap),
            .globals = std::move(c.globals),
            .chunks = std::move(c.chunks),
            .entry_chunk_id = 0
        };
    }
}
