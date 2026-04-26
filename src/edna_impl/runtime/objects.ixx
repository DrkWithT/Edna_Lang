module;

#include <utility>
#include <type_traits>
#include <numeric>
#include <memory>
#include <string>
#include <vector>
#include <stack>

export module edna.runtime.objects;

namespace Edna::Runtime {
    export enum class EvalStatus : std::uint8_t {
        pending,
        ok,
        memory,
        bad_op_arg,
        unsupported_op,
        build_failure
    };

    export using native_routine_type = EvalStatus(*)(void*, std::uint8_t argc);

    template <typename V>
    struct PropertyEntry {
        V key;
        V item;
        bool can_edit;
    };

    //? NOTE: This is the basic class of all native object types in Edna, allowing basic comparisons, call for functions, etc.
    //! WARNING: The methods with void* ctx MUST have the EvalContext passed by pointer, so the reinterpret_cast back to EvalContext is sound.
    export template <typename V>
    class ObjectBase {
    public:
        using properties = std::vector<PropertyEntry<V>>;
        using items = std::vector<V>;

        virtual ~ObjectBase() = default;

        virtual bool test(void* ctx) const noexcept = 0;
        virtual bool lt(void* ctx, const ObjectBase& object) const noexcept = 0;
        virtual bool gt(void* ctx, const ObjectBase& object) const noexcept = 0;
        virtual bool equals(void* ctx, const ObjectBase& object) const noexcept = 0;

        virtual const ObjectBase* get_prototype(void* ctx, bool use_proto) const noexcept = 0;
        virtual ObjectBase* get_prototype(void* ctx, bool use_proto) noexcept = 0;

        virtual V get_property(void* ctx, V key, bool use_protos) = 0;
        virtual V get_property(void* ctx, int pos, bool use_protos) = 0;
        virtual void set_property(void* ctx, V key, bool use_protos) = 0;
        virtual void set_property(void* ctx, int pos, bool use_protos) = 0;

        virtual std::string as_str(void* ctx) const = 0;

        virtual const void* get_code_data() const noexcept = 0;
        virtual native_routine_type get_native_fn_ptr() const noexcept = 0;
    };


    export template <typename BasicValue>
    class ObjectHeap {
    public:
        static constexpr std::size_t object_cost_v = 72;
        static constexpr int dud_id = -1;
        using object_base_value = ObjectBase<BasicValue>;

    private:
        std::stack<int> m_free_list;
        std::vector<std::unique_ptr<object_base_value>> m_cells;
        std::size_t m_overhead;
        std::size_t m_ripeness_threshold;
        int m_next_id;
        int m_max_id;
        int m_tenure_count;

        [[nodiscard]] constexpr int try_use_id() noexcept {
            if (!m_free_list.empty()) {
                const int reused_id = m_free_list.top();

                m_free_list.pop();

                return reused_id;
            }

            auto next_id = m_next_id;

            if (next_id >= m_max_id) {
                return dud_id;
            }

            m_next_id++;

            return next_id;
        }

    public:
        ObjectHeap()
        : ObjectHeap {4096UL} {}

        ObjectHeap(std::size_t capacity)
        : m_free_list {}, m_cells {}, m_overhead {}, m_ripeness_threshold {(object_cost_v * capacity * 2) / 3}, m_next_id {0}, m_max_id {std::numeric_limits<int>::max() - 1}, m_tenure_count {0} {
            m_cells.reserve(static_cast<std::size_t>(capacity));
            m_cells.resize(static_cast<std::size_t>(capacity));
        }

        constexpr void tenure_preloads() noexcept {
            m_tenure_count = m_next_id;
        }

        constexpr bool needs_gc() const noexcept {
            return m_overhead >= m_ripeness_threshold;
        }

        //! WARNING: object_p is meant to be a raw owning pointer (that's passed from the tail call optimized VM which cannot have non-trivially destructible things in opcode handlers) to some object. This overload exists to quickly manage the raw pointer in the "heap".
        template <typename ObjectType> requires (std::is_base_of_v<object_base_value, ObjectType>)
        [[nodiscard]] int store(ObjectType* object_p) noexcept {
            const auto result_id = try_use_id();

            if (result_id != dud_id) {
                m_cells[result_id] = std::unique_ptr<ObjectType>(object_p);
                m_overhead += object_cost_v;
            }

            return result_id;
        }

        template <typename ObjectType> requires (std::is_base_of_v<object_base_value, ObjectType>)
        [[nodiscard]] int store(std::unique_ptr<ObjectType> object_p) {
            const auto result_id = try_use_id();

            if (result_id != dud_id) {
                m_cells[result_id] = std::move(object_p);
                m_overhead += object_cost_v;
            }

            return result_id;
        }

        constexpr const object_base_value* at(int heap_id) const noexcept {
            if (heap_id < 0 || heap_id >= m_max_id) {
                return nullptr;
            }

            return m_cells[heap_id].get(); 
        }

        constexpr object_base_value* at(int heap_id) noexcept {
            if (heap_id < 0 || heap_id >= m_max_id) {
                return nullptr;
            }

            return m_cells[heap_id].get();
        }

        constexpr void destroy_at(int heap_id) {
            if (heap_id >= m_tenure_count && heap_id < m_max_id) {
                m_cells[heap_id] = {};
                m_free_list.push(heap_id);
                m_overhead -= object_cost_v;
            }
        }
    };
}
