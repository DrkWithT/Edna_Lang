module;

#include <utility>
#include <type_traits>
#include <vector>
#include <algorithm>

export module edna.runtime.tables;

export import edna.runtime.context;

namespace Edna::Runtime {
    export class Table : public ObjectBase {
    public:
        // ? std::vector<PropertyEntry<Value>>
        struct Entry {
            Runtime::Value key;
            Runtime::Value item;
            bool is_mutable;
        };

        using properties = std::vector<Entry>;
        using items = std::vector<Value>;
        using bytecode_type = Chunk*;

    private:
        properties m_properties;
        items m_items;
        Value m_prototype;

    public:
        constexpr Table() noexcept
        : m_properties {}, m_items {}, m_prototype {} {}

        [[nodiscard]] const items& indexables() const noexcept {
            return m_items;
        }

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
            return this == reinterpret_cast<const void*>(std::addressof(object));
        }

        [[nodiscard]] Runtime::Value get_prototype() const noexcept override {
            return m_prototype;
        }

        void set_prototype(Runtime::Value proto_v) noexcept override {
            m_prototype = proto_v;
        }

        [[nodiscard]] Runtime::Value get_property(void* ctx, Runtime::Value key, bool use_protos) override {
            if (auto prop_it = std::find_if(
                m_properties.begin(),
                m_properties.end(),
                [&key] (const auto& prop_pair) noexcept {
                    return prop_pair.key.hint() == key.hint() && prop_pair.key.scalar() == key.scalar();
                }
            ); prop_it != m_properties.end()) {
                return prop_it->item;
            } else if (auto prototype_ptr = reinterpret_cast<EvalContext*>(ctx)->heap.at(m_prototype.scalar()); prototype_ptr != nullptr) {
                return prototype_ptr->get_property(ctx, key, use_protos);
            }

            return Value::create_from_dud();
        }

        [[nodiscard]] Runtime::Value get_property(void* ctx, int pos) override {
            if (const auto pos_usize = static_cast<std::size_t>(pos); pos_usize < m_items.size()) {
                return m_items[pos_usize];
            }

            return Value::create_from_dud();
        }

        void set_property(void* ctx, Runtime::Value key, Value item, bool use_protos) override {
            if (auto prop_it = std::find_if(
                m_properties.begin(),
                m_properties.end(),
                [&key] (const auto& prop_pair) noexcept {
                    return prop_pair.key.hint() == key.hint() && prop_pair.key.scalar() == key.scalar();
                }
            ); prop_it != m_properties.end()) {
                prop_it->item = item;
            } else if (!use_protos) {
                m_properties.emplace_back(key, item, true);
            } else if (auto prototype_ptr = reinterpret_cast<EvalContext*>(ctx)->heap.at(m_prototype.scalar()); prototype_ptr != nullptr) {
                prototype_ptr->set_property(ctx, key, item, use_protos);
            }
        }

        void set_property(void* ctx, int pos, Runtime::Value item) override {
            const auto pos_usize = static_cast<std::size_t>(pos);

            if (pos_usize >= m_items.size()) {
                m_items.resize(pos_usize + 1);
            }

            m_items[pos_usize] = item;
        }

        [[nodiscard]] std::string as_str(void* ctx) const override {
            return "Table {...}"; // todo: implment pretty formatting of fields, items...
        }

        [[nodiscard]] const void* get_code_data() const noexcept override {
            return nullptr;
        }

        [[nodiscard]] void* get_code_data() noexcept override {
            return nullptr;
        }

        [[nodiscard]] native_routine_type get_native_fn_ptr() const noexcept override {
            return nullptr;
        }
    };
}
