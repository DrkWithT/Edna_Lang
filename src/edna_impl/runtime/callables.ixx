module;

#include <utility>
// #include <type_traits>
#include <memory>
// #include <algorithm>
#include <string>
#include <array>
#include <vector>
#include <format>
#include <sstream>

export module edna.runtime.callables;

export import edna.runtime.context;

namespace Edna::Runtime {
    export class Callable : public ObjectBase {
    private:
        struct Entry {
            Value key;
            Value item;
            bool is_mutable;
        };

        using properties = std::vector<Entry>;
        using items = std::vector<Value>;

        static constexpr std::array<std::string_view, static_cast<std::size_t>(Opcode::last)> op_names = {
            "nop",
            "dup",
            "push_null",
            "push_bool",
            "push_callee",
            "push_global",
            "push_str",
            "push_const",
            "get_local",
            "set_local",
            "get_prop",
            "set_prop",
            "pop",
            "make_array",
            "make_object",
            "deref",
            "negate_bool",
            "negate_num",
            "mod",
            "mul",
            "div",
            "add",
            "sub",
            "compare_eq",
            "compare_ne",
            "compare_lt",
            "compare_lte",
            "compare_gt",
            "compare_gte",
            "test",
            "jump",
            "jump_back",
            "jump_if",
            "jump_else",
            "call_ctor",
            "call_fun",
            "call_native",
            "ret",
            "padd_lk",
            "psub_lk"
        };

        static constexpr std::array<std::string_view, static_cast<std::size_t>(ValueScalarHint::last)> constant_tags = {
            "nan",
            "null",
            "boolean",
            "integer",
            "real",
            "local_id",
            "heap_id",
            "str_id"
        };

        properties m_properties;
        items m_indexables;

        std::unique_ptr<Chunk> m_chunk; // TODO: make a NativeCallable type below this entire class.

        std::uint8_t m_arity;
        bool m_is_ctor;

    public:
        explicit Callable(Chunk chunk, std::uint8_t expected_arity, bool is_ctor) noexcept
        : m_chunk (std::make_unique<Chunk>(std::move(chunk))), m_properties {}, m_indexables {}, m_arity {expected_arity}, m_is_ctor {is_ctor} {}

        [[nodiscard]] bool test(void* ctx) const noexcept override {
            return true;
        }

        [[nodiscard]] bool lt(void* ctx, const ObjectBase& object) const noexcept override {
            return false;
        }

        [[nodiscard]] bool gt(void* ctx, const ObjectBase& object) const noexcept override {
            return false;
        }

        [[nodiscard]] bool equals(void* ctx, const ObjectBase& object) const noexcept override {
            return this == std::addressof(object);
        }

        [[nodiscard]] Value get_property(void* ctx, Value key, bool use_protos) override {
            return Value::create_from_dud(); // todo
        }

        [[nodiscard]] Value get_property(void* ctx, int pos) override {
            return Value::create_from_dud(); // todo
        }

        void set_property(void* ctx, Value key, Value item, bool use_protos) override {
            ; // todo
        }

        void set_property(void* ctx, int pos, Value item) override {
            ; // todo
        }

        [[nodiscard]] Value get_prototype() const noexcept override {
            return Value::create_from_dud();
        }

        void set_prototype([[maybe_unused]] Value proto_v) noexcept override {}

        [[nodiscard]] std::string as_str(void* ctx) const override {
            std::ostringstream sout;

            sout << "Callable (...) {\n";

            for (auto constant_id = 0; const auto& constant_v : m_chunk->consts) {
                sout << std::format(
                    "constant {}: [tag = {}, scalar = {}]\n",
                    constant_id,
                    constant_tags.at(static_cast<std::size_t>(constant_v.hint())),
                    constant_v.scalar()
                );
                constant_id++;
            }

            for (std::size_t instruction_pos = 0; instruction_pos < m_chunk->code.size(); instruction_pos++) {
                const auto [opcode, arg] = m_chunk->code.at(instruction_pos);

                sout << std::format(
                    "  {}: {} {}\n",
                    instruction_pos,
                    op_names.at(static_cast<std::uint32_t>(opcode)),
                    static_cast<std::uint32_t>(arg)
                );
            }

            sout << "}";

            return sout.str();
        }

        [[nodiscard]] const void* get_code_data() const noexcept override {
            return m_chunk.get();
        }

        [[nodiscard]] void* get_code_data() noexcept override {
            return m_chunk.get();
        }

        [[nodiscard]] native_routine_type get_native_fn_ptr() const noexcept override {
            return nullptr;
        }
    };

    export class NativeCallable : public ObjectBase {
    private:
        native_routine_type m_native_fp;

    public:
        constexpr NativeCallable(native_routine_type fp) noexcept
        : m_native_fp {fp} {}

        [[nodiscard]] bool test(void* ctx) const noexcept override {
            return m_native_fp != nullptr;
        }

        [[nodiscard]] bool lt(void* ctx, const ObjectBase& object) const noexcept override {
            return false;
        }

        [[nodiscard]] bool gt(void* ctx, const ObjectBase& object) const noexcept override {
            return false;
        }

        [[nodiscard]] bool equals(void* ctx, const ObjectBase& object) const noexcept override {
            return object.get_native_fn_ptr() == m_native_fp;
        }

        [[nodiscard]] Runtime::Value get_prototype() const noexcept override {
            return Value::create_from_dud();
        }

        void set_prototype([[maybe_unused]] Runtime::Value proto_v) noexcept override {}

        [[nodiscard]] Runtime::Value get_property(void* ctx, Runtime::Value key, bool use_protos) override {
            return Value::create_from_dud();
        }

        [[nodiscard]] Runtime::Value get_property(void* ctx, int pos) override {
            return Value::create_from_dud();
        }

        void set_property(void* ctx, Runtime::Value key, Runtime::Value item, bool use_protos) override {}

        void set_property(void* ctx, int pos, Value item) override {}

        [[nodiscard]] std::string as_str(void* ctx) const override {
            return std::format("NativeCallable({}) {{...}}", reinterpret_cast<const void*>(m_native_fp));
        }

        [[nodiscard]] const void* get_code_data() const noexcept override {
            return nullptr;
        }

        [[nodiscard]] void* get_code_data() noexcept override {
            return nullptr;
        }

        [[nodiscard]] native_routine_type get_native_fn_ptr() const noexcept override {
            return m_native_fp;
        }
    };
}
