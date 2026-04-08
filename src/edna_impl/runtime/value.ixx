module;

#include <utility>
#include <type_traits>
#include <cmath>

export module edna.runtime.value;

export import edna.runtime.objects;

namespace Edna::Runtime {
    export struct ValueNaNOpt {};
    export struct ValueScalarOpt {};
    export struct LocalIdOpt {};
    export struct HeapIdOpt {};

    export enum class ValueScalarHint : std::uint8_t {
        null,
        boolean,
        integer,
        local_id,
        heap_id,
        last
    };

    class Value {
    public:
        using bits_type = std::uint64_t;
        using num_type = double;

        static_assert(sizeof(num_type) == 8 && sizeof(bits_type) == 8, "Unsupported platform, 64-bit integers and floats must be supported for NaN boxing!");
    
    private:
        static constexpr bits_type qnan_prefix = 0x7ffc000000000000;
        static constexpr bits_type pos_inf_prefix = 0x7f80000000000000;
        static constexpr bits_type neg_inf_prefix = 0xff80000000000000;

        num_type data;

        [[nodiscard]] static constexpr bits_type alias_data_to_bits_type(this auto&& self) noexcept {
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
            const auto scalar_abs = (scalar >= 0) ? scalar : -scalar;

            temp |= (scalar_abs << 4);
            temp |= 0x8000000000000000; //? encode negative sign bit

            return temp;
        }

    public:
        constexpr Value([[maybe_unused]] ValueNaNOpt opt) noexcept
        : data {data_from_bits_type(qnan_prefix)} {}

        constexpr Value(double d) noexcept
        : data {d} {}

        constexpr Value([[maybe_unused]] ValueScalarOpt opt, ValueScalarHint hint, int scalar) noexcept
        : data {data_from_bits_type(encode_bits_type(hint, scalar))} {}

        [[nodiscard]] static constexpr Value create_from() noexcept {
            return Value {ValueNaNOpt {}};
        }

        [[nodiscard]] static constexpr Value create_from(bool b) noexcept {
            return Value {ValueScalarOpt {}, ValueScalarHint::boolean, static_cast<int>(b)};
        }

        [[nodiscard]] static constexpr Value create_from(int i) noexcept {
            return Value {ValueScalarOpt {}, ValueScalarHint::integer, i};
        }

        [[nodiscard]] static constexpr Value create_from(int i, [[maybe_unused]] LocalIdOpt opt) noexcept {
            return Value {ValueScalarOpt {}, ValueScalarHint::local_id, i};
        }

        [[nodiscard]] static constexpr Value create_from(int i, [[maybe_unused]] HeapIdOpt opt) noexcept {
            return Value {ValueScalarOpt {}, ValueScalarHint::heap_id, i};
        }

        //! Deduces whether the boxed double is a QNAN.
        [[nodiscard]] constexpr bool is_nan() const noexcept {
            return std::isnan(data);
        }

        //! Deduces the value discriminator tag. Only works with boxed QNANs.
        [[nodiscard]] constexpr ValueScalarHint hint() const noexcept {
            const auto data_as_bytes = reinterpret_cast<const std::byte*>(&data);

            return static_cast<ValueScalarHint>(std::to_underlying(*data_as_bytes) & 0x0f);
        }

        //! Deduces the NaN-boxed scalar. Only works with QNANs.
        [[nodiscard]] constexpr int scalar() const noexcept {
            const auto data_as_bytes = reinterpret_cast<const std::byte*>(&data);

            return (std::to_underlying(data_as_bytes[0]) & 0xf0)
                + std::to_underlying(data_as_bytes[1]) << 8
                + std::to_underlying(data_as_bytes[2]) << 16
                + std::to_underlying(data_as_bytes[3]) << 24
                + (std::to_underlying(data_as_bytes[4]) & 0x0f) << 32;
        }

        [[nodiscard]] constexpr bits_type as_bits_type() const noexcept {
            return alias_data_to_bits_type();
        }

        [[nodiscard]] constexpr double as_double() const noexcept {
            return data;
        }



        [[nodiscard]] constexpr void load_aliased_data([[maybe_unused]] ValueNaNOpt opt) noexcept {
            data = data_from_bits_type(qnan_prefix);
        }

        [[nodiscard]] constexpr void load_num_data(double data_) noexcept {
            data = data_;
        }

        [[nodiscard]] constexpr void load_aliased_data([[maybe_unused]] ValueScalarOpt opt, ValueScalarHint hint, int scalar) noexcept {
            data = data_from_bits_type(encode_bits_type(hint, scalar));
        }

        template <Meta::ContextKind C>
        constexpr Value resolve_local_v(const C& ctx) {
            Value temp {*this};

            while (temp.is_nan() && temp.hint() == ValueScalarHint::local_id) {
                temp = ctx.stack.at(temp.scalar());
            }

            return temp;
        }

        template <Meta::ContextKind C>
        constexpr ObjectBase* resolve_object_p(const C& ctx) {
            auto resolved_value = resolve_local_v(v)

            if (resolved_value.hint() != ValueScalarHint::heap_id) {
                return nullptr;
            }

            return ctx.heap.get(resolved_value.scalar());
        }
    };
};