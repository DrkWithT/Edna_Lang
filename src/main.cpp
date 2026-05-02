#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <stdexcept>
#include <sstream>
#include <fstream>
#include <chrono>
#include <format>
#include <print>

import edna_impl;

constexpr std::string_view edna_ascii_art = " ___       _            \n"
                                            "(  _ \\    ( )            \n"
                                            "| (_(_)  _| | ___    _ _ \n"
                                            "|  _)_ / _  |  _  \\/ _  )\n"
                                            "| (_( ) (_| | ( ) | (_| |\n"
                                            "(____/ \\__ _)_) (_)\\__ _)\n";

namespace Edna {
    struct Configs {
        std::string_view name;
        std::string_view author;
        std::size_t heap_populate_capacity;
        std::size_t string_populate_capacity;
        std::size_t local_capacity;
        std::uint16_t v_major;
        std::uint16_t v_minor;
        std::uint16_t v_patch;
    };

    class EditObjectProxy {
    private:
        Compile::CompileContext* m_compiler;        // ? contains preloaded heap, name-info tables, etc.
        Runtime::ObjectBase* m_object;    // ? first refers to an empty object to build / edit

    public:
        explicit constexpr EditObjectProxy(Compile::CompileContext* compiler_p, Runtime::ObjectBase* object_p) noexcept
        : m_compiler {compiler_p}, m_object {object_p} {}

        template <typename Fn> requires (
            std::is_base_of_v<Runtime::ObjectBase, Fn>
            && (
                std::is_same_v<Fn, Runtime::NativeCallable>
                || std::is_same_v<Fn, Runtime::Callable>
            )
        )
        [[nodiscard]] auto add_method(std::string name, std::unique_ptr<Fn> item) -> decltype(this) {
            if (m_compiler == nullptr || m_object == nullptr) {
                throw std::runtime_error {
                    std::format("Cannot use finished EditObjectProxy at address {} !", reinterpret_cast<void*>(this))
                };
            }

            std::unique_ptr<Runtime::ObjectBase> item_box = std::move(item);

            const auto method_key = Runtime::Value::create_from_id(
                static_cast<int>(m_compiler->record_str_symbol(std::move(name))->id),
                Runtime::StrIdOpt {}
            );
            const auto method_handle_v = Runtime::Value::create_from_id(
                static_cast<int>(m_compiler->record_global_symbol("", std::move(item_box))->id),
                Runtime::HeapIdOpt {}
            );

            m_object->set_property(nullptr, method_key, method_handle_v, false);

            return this;
        }

        constexpr void finish() noexcept {
            m_compiler = nullptr;
            m_object = nullptr;
        }
    };

    class Driver {
    private:
        Frontend::Lexer m_lexer;
        Compile::CompileContext m_compiler;
        Compile::Optimizer m_optimizer;
        Configs m_info;
        bool m_allow_bytecode_dump;

        [[nodiscard]] std::string read_source(const std::string& source_path) {
            std::ifstream reader {source_path};

            if (!reader.is_open()) {
                return {};
            }

            std::ostringstream sout;
            std::string temp_line;

            while (std::getline(reader, temp_line)) {
                sout << temp_line << '\n';
                temp_line.clear();
            }

            return sout.str();
        }

        [[nodiscard]] std::optional<Runtime::Program> compile_sources(const std::string& first_source_path) {
            const std::string source = read_source(first_source_path);
            const std::string_view source_view {source};

            m_lexer.use_source(source_view);

            Frontend::Parser parser {m_lexer, source_view};

            auto ast_decls = parser(m_lexer, source_view);

            if (ast_decls.empty()) {
                return {};
            }

            return Compile::compile_all(m_compiler, ast_decls, source);
        }

    public:
        constexpr Driver(Configs cfg)
        : m_lexer {}, m_compiler {cfg.heap_populate_capacity, cfg.string_populate_capacity}, m_optimizer {}, m_info {cfg} {}

        [[nodiscard]] constexpr const Configs& get_info() const noexcept {
            return m_info;
        }

        constexpr void allow_bytecode_dump(bool b) noexcept {
            m_allow_bytecode_dump = b;
        }

        void map_lexical(std::string_view lexeme, Frontend::TokenTag tag) {
            m_lexer.add_edna_lexical(lexeme, tag);
        }

        template <typename Emitter> requires (
            std::is_base_of_v<Compile::ExprEmitterBase, Emitter>
            && std::is_default_constructible_v<Emitter>
        )
        void add_expr_emitter(Frontend::ExprTag tag) noexcept {
            m_compiler.add_expr_emitter(tag, std::make_unique<Emitter>());
        }

        template <typename Emitter> requires (
            std::is_base_of_v<Compile::StmtEmitterBase, Emitter>
            && std::is_default_constructible_v<Emitter>
        )
        void add_stmt_emitter(Frontend::StmtTag tag) noexcept {
            m_compiler.add_stmt_emitter(tag, std::make_unique<Emitter>());
        }

        template <typename OptPass> requires (
            std::is_base_of_v<Compile::PassBase, OptPass>
            && std::is_default_constructible_v<OptPass>
        )
        void add_optimizer_pass(Compile::PassTag tag) noexcept {
            m_optimizer.add_pass(tag, std::make_unique<OptPass>());
        }

        void add_native_object(std::string name, std::unique_ptr<Runtime::ObjectBase> object) noexcept {
            m_compiler.record_global_symbol(name, std::move(object));
        }

        template <typename Object> requires (std::is_base_of_v<Runtime::ObjectBase, Object> && std::is_default_constructible_v<Object>)
        [[nodiscard]] EditObjectProxy begin_object(std::string name) noexcept {
            std::unique_ptr<Runtime::ObjectBase> opaque_box = std::make_unique<Object>();
            auto proxy_object_ptr = opaque_box.get();

            add_native_object(name, std::move(opaque_box));

            return EditObjectProxy {std::addressof(m_compiler), proxy_object_ptr};
        }

        Runtime::EvalStatus execute_program(const std::string& main_file_path) {
            auto program_option = compile_sources(main_file_path);

            if (!program_option) {
                return Runtime::EvalStatus::build_failure;
            }

            for (auto& chunk : program_option->chunks) {
                m_optimizer.apply(chunk.code);
            }

            for (auto& object_ptr : program_option->pre_heap.cells()) {
                if (!object_ptr) {
                    break;
                }

                if (auto object_chunk_ptr = reinterpret_cast<Runtime::Chunk*>(object_ptr->get_code_data()); object_chunk_ptr != nullptr) {
                    m_optimizer.apply(object_chunk_ptr->code);
                }
            }

            if (m_allow_bytecode_dump) {
                Runtime::disassemble_program(*program_option);
                return Runtime::EvalStatus::ok;
            }

            Runtime::VM vm {
                Runtime::EvalContext {
                    program_option.value(),
                    m_info.local_capacity
                }
            };

            auto run_begin = std::chrono::steady_clock::now();
            auto status = vm.template run<Runtime::Handlers>();
            auto running_time = std::chrono::steady_clock::now() - run_begin;

            std::println("Runtime: \x1b[1;33m{}\x1b[0m ms", std::chrono::duration_cast<std::chrono::milliseconds>(running_time));

            Runtime::display_value(vm.context().heap, vm.result());

            return vm.context().status;
        }
    };
}


[[nodiscard]] Edna::Runtime::EvalStatus native_print(void* opaque, std::uint8_t argc) {
    auto context = reinterpret_cast<Edna::Runtime::EvalContext*>(opaque);
    const std::uint32_t callee_bp = context->sp - argc;

    const std::span<Edna::Runtime::Value> arguments {
        context->stack.get() + callee_bp + 1,
        static_cast<std::uint32_t>(argc)
    };

    for (const auto& value_ref : arguments) {
        Edna::Runtime::display_value(context->heap, value_ref);
        std::print(" ");
    }
    std::println("");

    context->sp = callee_bp - 1;
    context->stack[context->sp] = Edna::Runtime::Value::create_from_dud();

    return Edna::Runtime::EvalStatus::pending;
}

// todo: refactor all setup + interpreter logic into a driver class later.
int main(int argc, char* argv[]) {
    using namespace Edna;

    if (argc < 2 || argc > 3) {
        std::println("Usage:\nedna [info | dump | run] <args...>\n\tinfo: print info\n\tdump: print bytecode dump only\n\trun: run without bytecode dump\n");
        return 1;
    }

    Driver driver {
        Configs {
            .name = edna_ascii_art,
            .author = "DrkWithT",
            .heap_populate_capacity = 512,
            .string_populate_capacity = 1024,
            .local_capacity = 4096,
            .v_major = 0,
            .v_minor = 1,
            .v_patch = 0,
        }
    };

    const std::string_view arg_1 = argv[1];

    if (arg_1 == "info") {
        const auto& [name, author, dud_0, dud_1, dud_2, v_major, v_minor, v_patch] = driver.get_info();

        std::println("\x1b[1;36m{}\x1b[0m\nBy: {}\nVersion: {}.{}.{}\n\nUsage:\nedna [info | dump | run] <args...>\n\tinfo: print info\n\tdump: print bytecode dump only\n\trun: run without bytecode dump\n", name, author, v_major, v_minor, v_patch);

        return 0;
    }

    if ((arg_1 != "run" && arg_1 != "dump")) {
        std::println("Usage:\nedna [info | dump | run] <args...>\n\tinfo: print info\n\tdump: print bytecode dump only\n\trun: run without bytecode dump\n");
        return 1;
    }

    std::string arg_2;

    if (argc == 3) {
        arg_2 = argv[2];
    } else {
        std::println("Missing source file argument.");
        return 1;
    }

    driver.map_lexical("null", Frontend::TokenTag::literal_null);
    driver.map_lexical("true", Frontend::TokenTag::literal_true);
    driver.map_lexical("false", Frontend::TokenTag::literal_false);
    driver.map_lexical("self", Frontend::TokenTag::keyword_self);
    driver.map_lexical("fun", Frontend::TokenTag::keyword_fun);
    driver.map_lexical("uses", Frontend::TokenTag::keyword_uses);
    driver.map_lexical("let", Frontend::TokenTag::keyword_let);
    driver.map_lexical("mut", Frontend::TokenTag::keyword_mut);
    driver.map_lexical("cond", Frontend::TokenTag::keyword_cond);
    driver.map_lexical("case", Frontend::TokenTag::keyword_case);
    driver.map_lexical("else", Frontend::TokenTag::keyword_else);
    driver.map_lexical("symbol", Frontend::TokenTag::keyword_symbol);
    driver.map_lexical("prec", Frontend::TokenTag::keyword_prec);
    driver.map_lexical("end", Frontend::TokenTag::keyword_end);
    driver.map_lexical("-", Frontend::TokenTag::op_neg);
    driver.map_lexical("!", Frontend::TokenTag::op_bang);
    driver.map_lexical("%", Frontend::TokenTag::op_mod);
    driver.map_lexical("*", Frontend::TokenTag::op_mult);
    driver.map_lexical("/", Frontend::TokenTag::op_div);
    driver.map_lexical("+", Frontend::TokenTag::op_plus);
    driver.map_lexical("-", Frontend::TokenTag::op_sub);
    driver.map_lexical("==", Frontend::TokenTag::op_equals);
    driver.map_lexical("!=", Frontend::TokenTag::op_unequal);
    driver.map_lexical("<", Frontend::TokenTag::op_lesser);
    driver.map_lexical(">", Frontend::TokenTag::op_greater);
    driver.map_lexical("<=", Frontend::TokenTag::op_lte);
    driver.map_lexical(">=", Frontend::TokenTag::op_gte);
    driver.map_lexical("&&", Frontend::TokenTag::op_and);
    driver.map_lexical("||", Frontend::TokenTag::op_or);
    driver.map_lexical("=", Frontend::TokenTag::op_assign);
    driver.map_lexical("=>", Frontend::TokenTag::arrow);
    driver.map_lexical("@", Frontend::TokenTag::locusor);
    driver.map_lexical("...", Frontend::TokenTag::ellipses);

    driver.add_expr_emitter<Compile::AtomEmitter>(Frontend::ExprTag::atom);
    driver.add_expr_emitter<Compile::CondEmitter>(Frontend::ExprTag::cond);
    driver.add_expr_emitter<Compile::BlockEmitter>(Frontend::ExprTag::block);
    driver.add_expr_emitter<Compile::ArrayLiteralEmitter>(Frontend::ExprTag::array);
    driver.add_expr_emitter<Compile::LambdaEmitter>(Frontend::ExprTag::lambda);
    driver.add_expr_emitter<Compile::LhsEmitter>(Frontend::ExprTag::lhs);
    driver.add_expr_emitter<Compile::CallEmitter>(Frontend::ExprTag::call);
    driver.add_expr_emitter<Compile::UnaryEmitter>(Frontend::ExprTag::unary);
    driver.add_expr_emitter<Compile::BinaryEmitter>(Frontend::ExprTag::binary);
    driver.add_expr_emitter<Compile::AssignEmitter>(Frontend::ExprTag::assign);

    driver.add_stmt_emitter<Compile::VarsEmitter>(Frontend::StmtTag::vars);
    driver.add_stmt_emitter<Compile::ExprStmtEmitter>(Frontend::StmtTag::expr_stmt);

    driver.add_optimizer_pass<Compile::CondenseOps>(Compile::PassTag::condense_ops);
    driver.add_optimizer_pass<Compile::TrimNOPs>(Compile::PassTag::trim_nops);

    driver.add_native_object("print", std::make_unique<Runtime::NativeCallable>(&native_print));

    {
        auto list_prototype_handle = driver.template begin_object<Runtime::Table>("__proto_list__");

        list_prototype_handle.add_method("len", std::make_unique<Runtime::NativeCallable>(&Runtime::Natives::list_len))
            ->add_method("at", std::make_unique<Runtime::NativeCallable>(&Runtime::Natives::list_at))
            ->finish();
    }

    if (arg_1 == "dump") {
        driver.allow_bytecode_dump(true);
    }

    return (driver.execute_program(arg_2) == Runtime::EvalStatus::ok) ? 0 : 1;
}
