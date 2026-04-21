module;

#include <utility>
#include <type_traits>
#include <memory>
#include <algorithm>
#include <string>
// #include <vector>
#include <variant>

export module edna.runtime.callables;

export import edna.runtime.context;

namespace Edna::Runtime {
    export class Callable : public ObjectBase<Value> {
    public:
        using properties = typename ObjectBase::properties;
        using items = typename ObjectBase::items;
        using native_type = EvalStatus(*)(Runtime::EvalContext*, std::uint8_t argc);
        using bytecode_type = Chunk*;

    private:
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

        [[nodiscard]] bool lt(void* ctx, const ObjectBase<Value>& object) const noexcept override {
            return false;
        }

        [[nodiscard]] bool gt(void* ctx, const ObjectBase<Value>& object) const noexcept override {
            return false;
        }

        [[nodiscard]] bool equals(void* ctx, const ObjectBase<Value>& object) const noexcept override {
            return this == std::addressof(object);
        }

        [[nodiscard]] Value get_property(void* ctx, Value key, bool use_protos) override {
            return Value::create_from_dud(); // todo
        }

        [[nodiscard]] Value get_property(void* ctx, int pos, bool use_protos) override {
            return Value::create_from_dud(); // todo
        }

        void set_property(void* ctx, Value key, bool use_protos) override {
            ; // todo
        }

        void set_property(void* ctx, int pos, bool use_protos) override {
            ; // todo
        }

        [[nodiscard]] const Value* get_prototype(void* ctx, bool use_proto) const noexcept override {
            return nullptr;
        }

        [[nodiscard]] Value* get_prototype(void* ctx, bool use_proto) noexcept override {
            return nullptr;
        }

        [[nodiscard]] std::string as_str(void* ctx) const override {
            return std::string {"Callable {...}"}; // todo
        }

        [[nodiscard]] const void* get_code_data() const noexcept override {
            return m_chunk.get();
        }

        [[nodiscard]] const void* get_native_fn_ptr() const noexcept override {
            return nullptr;
        }
    };
}
