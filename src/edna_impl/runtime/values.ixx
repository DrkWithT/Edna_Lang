module;

#include <utility>
#include <type_traits>
#include <cmath>
#include <numeric>
#include <memory>
#include <string>
#include <vector>
#include <print>

export module edna.runtime.values;

export import edna.runtime.string_pool;

namespace Edna::Runtime {
    export enum class EvalStatus : std::uint8_t {
        pending,
        ok,
        alloc_fail,
        bad_op_arg,
        unsupported_op,
        build_failure
    };

    //? NOTE: I must use void* to pass in an EvalContext*. This is because Clang++ will not compile my program with error 'note: hidden overloaded virtual function 'Edna::Runtime::ObjectBase::get_property' declared here: type mismatch at 1st parameter ('EvalContext *' vs 'EvalContext *')' strangely, even if a non-exported forward declaration of Runtime::EvalContext* is used otherwise. std::any would incur a RTTI runtime cost in the hot interpreter code for every native built-in call, which seems excessive to me.
    export using native_routine_type = EvalStatus(*)(void*, std::uint8_t argc);

    export struct ValueNullOpt {};
    export struct ValueNaNOpt {};
    export struct ValueInfOpt {};
    export struct ValueNegInfOpt {};
    export struct ValueScalarOpt {};
    export struct UseNativeTypeOpt {};
    export struct LocalIdOpt {};
    export struct HeapIdOpt {};
    export struct StrIdOpt {};

    export enum class ValueScalarHint : std::uint8_t {
        nan,
        null,
        boolean,
        integer,
        real,
        local_id,
        heap_id,
        str_id,
        last
    };

    export class Value {
    public:
        using bits_type = std::uint64_t;
        using num_type = double;

        static_assert(sizeof(num_type) == 8 && sizeof(bits_type) == 8, "Unsupported platform, 64-bit integers and floats are required for NaN boxing.");
    
    private:
        // ? BOXED LAYOUT:
        // ? | QNAN-pfx | unused bits lmao |
        // ?                   |         \
        // ? | QNAN-pfx | 32-bit scalar | 4-bit hint |
        static constexpr bits_type qnan_prefix = 0x7ffc000000000000;
        static constexpr bits_type pos_inf_prefix = 0x7f80000000000000;
        static constexpr bits_type neg_inf_prefix = 0xff80000000000000;

        num_type data;

        [[nodiscard]] constexpr bits_type alias_data_to_bits_type(this auto&& self) noexcept {
            bits_type temp {};

            std::memcpy(&temp, &self.data, sizeof(num_type));

            return temp;
        }

        [[nodiscard]] static constexpr num_type data_from_bits_type(bits_type bits) noexcept {
            num_type temp {};

            std::memcpy(&temp, &bits, sizeof(bits_type));
            return temp;
        }

        [[nodiscard]] static constexpr bits_type encode_bits_type(ValueScalarHint hint, int scalar) noexcept {
            bits_type temp = qnan_prefix;

            //? 1: 4-bits boxed value prefix
            temp |= (std::to_underlying(hint) & 0xf);

            //? 2: 32-bit integer value encoded
            temp |= (scalar << 4);

            return temp;
        }

    public:
        constexpr Value() noexcept
        : Value (ValueNullOpt {}) {}

        constexpr Value([[maybe_unused]] ValueNullOpt opt) noexcept
        : Value (ValueScalarOpt {}, ValueScalarHint::null, 0) {}

        constexpr Value([[maybe_unused]] ValueNaNOpt opt) noexcept
        : data {data_from_bits_type(qnan_prefix)} {}

        constexpr Value([[maybe_unused]] ValueInfOpt opt) noexcept
        : data {data_from_bits_type(pos_inf_prefix)} {}

        constexpr Value([[maybe_unused]] ValueNegInfOpt opt) noexcept
        : data {data_from_bits_type(neg_inf_prefix)} {}

        constexpr Value(double d) noexcept
        : data {d} {}

        constexpr Value([[maybe_unused]] ValueScalarOpt opt, ValueScalarHint hint, int scalar) noexcept
        : data {data_from_bits_type(encode_bits_type(hint, scalar))} {}

        [[nodiscard]] static constexpr Value create_from_dud() noexcept {
            return Value {ValueNullOpt {}};
        }

        [[nodiscard]] static constexpr Value create_as_nan() noexcept {
            return Value {ValueScalarOpt {}, ValueScalarHint::nan, 0};
        }

        [[nodiscard]] static constexpr Value create_as_inf() noexcept {
            return Value {ValueInfOpt {}};
        }

        [[nodiscard]] static constexpr Value create_as_neg_inf() noexcept {
            return Value {ValueNegInfOpt {}};
        }

        [[nodiscard]] static constexpr Value create_from_bool(bool b) noexcept {
            return Value {ValueScalarOpt {}, ValueScalarHint::boolean, static_cast<int>(b)};
        }

        [[nodiscard]] static constexpr Value create_from_int(int i) noexcept {
            return Value {ValueScalarOpt {}, ValueScalarHint::integer, i};
        }

        [[nodiscard]] static constexpr Value create_from_id(int i, [[maybe_unused]] LocalIdOpt opt) noexcept {
            return Value {ValueScalarOpt {}, ValueScalarHint::local_id, i};
        }

        [[nodiscard]] static constexpr Value create_from_id(int i, [[maybe_unused]] HeapIdOpt opt) noexcept {
            return Value {ValueScalarOpt {}, ValueScalarHint::heap_id, i};
        }

        [[nodiscard]] static constexpr Value create_from_id(int i, [[maybe_unused]] StrIdOpt opt) noexcept {
            return Value {ValueScalarOpt {}, ValueScalarHint::str_id, i};
        }

        [[nodiscard]] static constexpr Value create_from_double(double d) noexcept {
            return Value {d};
        }

        //! Deduces whether the boxed double is a QNAN.
        [[nodiscard]] constexpr bool is_nan() const noexcept {
            return hint() != ValueScalarHint::real;
        }

        //! Deduces the value discriminator tag. Only works with boxed QNANs.
        [[nodiscard]] constexpr ValueScalarHint hint() const noexcept {
            if (!std::isnan(data)) {
                return ValueScalarHint::real;
            }

            const auto data_as_bytes = reinterpret_cast<const std::byte*>(&data);

            return static_cast<ValueScalarHint>(std::to_underlying(*data_as_bytes) & 0x0f);
        }

        //! Deduces the NaN-boxed scalar. Only works with QNANs.
        [[nodiscard]] constexpr int scalar() const noexcept {
            const auto data_as_bytes = reinterpret_cast<const std::byte*>(&data);

            return ((std::to_underlying(data_as_bytes[0]) & 0xf0) >> 4)
                + (std::to_underlying(data_as_bytes[1]) << 4)
                + (std::to_underlying(data_as_bytes[2]) << 12)
                + (std::to_underlying(data_as_bytes[3]) << 20)
                + ((std::to_underlying(data_as_bytes[4]) & 0x0f) << 28);
        }

        [[nodiscard]] constexpr bits_type as_bits_type() const noexcept {
            return alias_data_to_bits_type();
        }

        [[nodiscard]] constexpr double as_double() const noexcept {
            return data;
        }

        constexpr void load_aliased_data([[maybe_unused]] ValueNaNOpt opt) noexcept {
            data = data_from_bits_type(qnan_prefix);
        }

        constexpr void load_num_data(double data_) noexcept {
            data = data_;
        }

        constexpr void load_aliased_data([[maybe_unused]] ValueScalarOpt opt, ValueScalarHint hint, int scalar) noexcept {
            data = data_from_bits_type(encode_bits_type(hint, scalar));
        }
    };

    //? NOTE: This is the basic class of all native object types in Edna, allowing basic comparisons, call for functions, etc.
    //! WARNING: The methods with EvalContext* ctx MUST have the EvalContext passed by pointer, so the reinterpret_cast back to EvalContext is sound.
    export class ObjectBase {
    public:
        using items = std::vector<Runtime::Value>;

        virtual ~ObjectBase() = default;

        virtual bool test(void* ctx) const noexcept = 0;
        virtual bool lt(void* ctx, const ObjectBase& object) const noexcept = 0;
        virtual bool gt(void* ctx, const ObjectBase& object) const noexcept = 0;
        virtual bool equals(void* ctx, const ObjectBase& object) const noexcept = 0;

        virtual Value get_prototype() const noexcept = 0;
        virtual void set_prototype(Value proto_v) noexcept = 0;

        virtual Value get_property(void* ctx, Value key, bool use_protos) = 0;
        virtual Value get_property(void* ctx, int pos) = 0;
        virtual void set_property(void* ctx, Value key, Value item, bool use_protos) = 0;
        virtual void set_property(void* ctx, int pos, Value item) = 0;

        virtual std::string as_str(void* ctx) const = 0;

        virtual const void* get_code_data() const noexcept = 0;
        virtual void* get_code_data() noexcept = 0;
        virtual native_routine_type get_native_fn_ptr() const noexcept = 0;
    };

    export class ObjectHeap {
    public:
        static constexpr std::size_t object_cost_v = 72;
        static constexpr int dud_id = -1;

    private:
        std::vector<int> m_free_list;
        std::vector<std::unique_ptr<ObjectBase>> m_cells;
        std::size_t m_overhead;
        std::size_t m_ripeness_threshold;
        int m_next_id;
        int m_max_id;
        int m_tenure_count;

        [[nodiscard]] constexpr int try_use_id() noexcept {
            if (!m_free_list.empty()) {
                const int reused_id = m_free_list.back();

                m_free_list.pop_back();

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
            m_cells.reserve(capacity);
            m_cells.resize(capacity);
        }

        constexpr void tenure_preloads() noexcept {
            m_tenure_count = m_next_id;
        }

        [[nodiscard]] constexpr bool needs_gc() const noexcept {
            return m_overhead >= m_ripeness_threshold;
        }

        [[nodiscard]] std::vector<std::unique_ptr<ObjectBase>>& cells() noexcept {
            return m_cells;
        }

        //! WARNING: object_p is meant to be a raw owning pointer (that's passed from the tail call optimized VM which cannot have non-trivially destructible things in opcode handlers) to some object. This overload exists to quickly manage the raw pointer in the "heap".
        template <typename ObjectType> requires (std::is_base_of_v<ObjectBase, ObjectType>)
        [[nodiscard]] int store(ObjectType* object_p) noexcept {
            const auto result_id = try_use_id();

            if (result_id != dud_id) {
                m_cells[result_id] = std::unique_ptr<ObjectType>(object_p);
                m_overhead += object_cost_v;
            }

            return result_id;
        }

        template <typename ObjectType> requires (std::is_base_of_v<ObjectBase, ObjectType>)
        [[nodiscard]] int store(std::unique_ptr<ObjectType> object_p) {
            const auto result_id = try_use_id();

            if (result_id != dud_id) {
                m_cells[result_id] = std::move(object_p);
                m_overhead += object_cost_v;
            }

            return result_id;
        }

        constexpr const ObjectBase* at(int heap_id) const noexcept {
            if (heap_id < 0 || heap_id >= m_max_id) {
                return nullptr;
            }

            return m_cells[heap_id].get(); 
        }

        constexpr ObjectBase* at(int heap_id) noexcept {
            if (heap_id < 0 || heap_id >= m_max_id) {
                return nullptr;
            }

            return m_cells[heap_id].get();
        }

        constexpr void destroy_at(int heap_id) {
            if (heap_id >= m_tenure_count && heap_id < m_max_id) {
                m_cells[heap_id] = {};
                m_free_list.push_back(heap_id);
                m_overhead -= object_cost_v;
            }
        }
    };

    export void display_value(const ObjectHeap& heap, const StringPool& strings, const Value& v) {
        if (const auto v_hint = v.hint(); v_hint == Edna::Runtime::ValueScalarHint::real) {
            std::print("{}", v.as_double());
        } else if (const auto v_hint = v.hint(); v_hint == Edna::Runtime::ValueScalarHint::null) {
            std::print("null");
        } else if (v_hint == Edna::Runtime::ValueScalarHint::boolean) {
            std::print("{}", v.scalar() != 0);
        } else if (v_hint == Edna::Runtime::ValueScalarHint::integer) {
            std::print("{}", v.scalar());
        } else if (v_hint == Edna::Runtime::ValueScalarHint::heap_id) {
            std::print("{}", heap.at(static_cast<int>(v.scalar()))->as_str(nullptr));
        } else if (v_hint == Edna::Runtime::ValueScalarHint::str_id) {
            std::print("{}", strings.cells().at(v.scalar()));
        } else if (v_hint == Edna::Runtime::ValueScalarHint::nan) {
            std::print("(QNaN)");
        } else {
            std::print("??");
        }
    }
}
