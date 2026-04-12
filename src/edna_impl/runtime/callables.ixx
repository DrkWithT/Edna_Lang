module;

#include <utility>
#include <type_traits>
#include <memory>
#include <algorithm>
#include <string>
#include <vector>
#include <variant>

export module edna.runtime.callables;

export import edna.runtime.context;

namespace Edna::Runtime {
    class Callable : public ObjectBase {
    public:
        using properties = typename ObjectBase::properties;
        using items = typename ObjectBase::items;
        using native_fun = EvalStatus(*)(EvalContext*, std::uint8_t argc);
        using code_ptr = const Instruction*;

    private:
        using content_type = std::variant<native_fun, code_ptr>;

        std::variant<native_fun, code_ptr> m_contents;
        properties m_properties;
        items m_indexables;
        std::uint8_t m_arity;
        bool m_is_ctor;

        [[nodiscard]] constexpr bool has_native() const noexcept {
            return std::holds_alternative<native_fun>(m_contents);
        }

    public:
        template <typename CodeType> requires (std::is_constructible_v<content_type, CodeType>)
        explicit Callable(CodeType code, std::uint8_t expected_arity, bool is_ctor) noexcept (std::is_nothrow_constructible_v<content_type, CodeType>)
        : m_contents (std::move(code)), m_properties {}, m_indexables {}, m_arity {expected_arity}, m_is_ctor {is_ctor} {}

        [[nodiscard]] bool test(void* ctx) const noexcept override {
            return true;
        }

        [[nodiscard]] bool operator<(void* ctx, const ObjectBase& object) const noexcept override {
            return false;
        }

        [[nodiscard]] bool operator>(void* ctx, const ObjectBase& object) const noexcept override {
            return false;
        }

        [[nodiscard]] bool operator==(void* ctx, const ObjectBase& object) const noexcept override {
            return this == std::addressof(object);
        }

        [[nodiscard]] Value get_property(void* ctx, Value key, bool use_protos) override {
            return Value::create_from(); // todo
        }

        [[nodiscard]] Value get_property(void* ctx, int pos, bool use_protos) override {
            return Value::create_from(); // todo
        }

        void set_property(void* ctx, Value key, bool use_protos) override {
            ; // todo
        }

        void set_property(void* ctx, int pos, bool use_protos) override {
            ; // todo
        }

        [[nodiscard]] std::string as_str(void* ctx) const override {
            return {"Callable {...}"}; // todo
        }

        /**
         * @brief //? See `docs/Runtime.md` at "Calls" for how the layout of call values is set.
         * 
         * @param ctx 
         * @param argc 
         * @return true 
         * @return false 
         */
         [[nodiscard]] bool call(void* ctx, std::uint8_t argc) override {
            // todo: allow variadics later... an array can be made to fill a final rest parameter.
            if (argc != m_arity) {
                return false;
            }

            // todo: whether native or bytecode, invoke regularly, not injecting the instance prototype from slot 1 (slot 0 is the trait-prototype)...
            auto context = reinterpret_cast<EvalContext*>(ctx);

            if (has_native()) {
                context->status = std::get<native_fun>(m_contents)(context, argc);
                return context->status == EvalStatus::pending; //? or, just make the status check ensure an "OK"
            }

            const Instruction* caller_ret_ip_v = context->ip + 1;
            const std::uint16_t caller_bp_v = context->bp;
            const std::uint16_t callee_bp_v = context->sp - args;

            context.frames.emplace_back(caller_ret_ip_v, caller_bp_v, callee_bp_v);

            context->ip = std::get<code_ptr>(m_contents);
            context->bp = callee_bp_v;

            return true;
        }

        [[nodiscard]] bool call_as_ctor(void* ctx, std::uint8_t argc) override {
            if (!m_is_ctor) {
                return false;
            }

            // todo LATER: whether native or bytecode, construct selfArg at locals[CALLEE_BP - 1] as a fresh Table object with the instance-prototype injected as its trait prototype...
            return true;
        }
    };
}
