module;

#include <cstdint>
// #include <utility>
// #include <type_traits>
#include <array>
#include <span>
// #include <print>

export module edna.runtime.handlers1;

export import edna.runtime.meta;
import edna.runtime.tables;

namespace Edna::Runtime {
    export struct Handlers {
        using handler_type = EvalStatus(*)(EvalContext&, const Instruction*, const Value*, Value*);

        //* Handler signatures / ptrs:

        [[nodiscard]] static constexpr EvalStatus op_nop(EvalContext& c, const Instruction* ip, const Value* cvp, Value* stack) {
            ip++;

            [[clang::musttail]]
            return dispatch(c, ip, cvp, stack);
        }

        [[nodiscard]] static constexpr EvalStatus op_dup(EvalContext& c, const Instruction* ip, const Value* cvp, Value* stack) {
            c.sp++;
            stack[c.sp] = stack[c.sp];
            ip++;

            [[clang::musttail]]
            return dispatch(c, ip, cvp, stack);
        }

        [[nodiscard]] static constexpr EvalStatus op_push_null(EvalContext& c, const Instruction* ip, const Value* cvp, Value* stack) {
            c.sp++;
            stack[c.sp] = Value::create_from_dud();
            ip++;

            [[clang::musttail]]
            return dispatch(c, ip, cvp, stack);
        }

        [[nodiscard]] static constexpr EvalStatus op_push_bool(EvalContext& c, const Instruction* ip, const Value* cvp, Value* stack) {
            c.sp++;
            stack[c.sp] = Value::create_from_bool(ip->arg);
            ip++;

            [[clang::musttail]]
            return dispatch(c, ip, cvp, stack);
        }

        [[nodiscard]] static constexpr EvalStatus op_push_global(EvalContext& c, const Instruction* ip, const Value* cvp, Value* stack) {
            // TODO: use argument of opcode as globals index.
            c.sp++;
            stack[c.sp] = c.globals.at(ip->arg);
            ip++;

            [[clang::musttail]]
            return dispatch(c, ip, cvp, stack);
        }

        [[nodiscard]] static constexpr EvalStatus op_push_callee(EvalContext& c, const Instruction* ip, const Value* cvp, Value* stack) {
            c.sp++;
            stack[c.sp] = stack[c.bp];
            ip++;

            [[clang::musttail]]
            return dispatch(c, ip, cvp, stack);
        }

        [[nodiscard]] static constexpr EvalStatus op_push_const(EvalContext& c, const Instruction* ip, const Value* cvp, Value* stack) {
            c.sp++;
            stack[c.sp] = cvp[ip->arg];
            ip++;

            [[clang::musttail]]
            return dispatch(c, ip, cvp, stack);
        }

        [[nodiscard]] static constexpr EvalStatus op_get_local(EvalContext& c, const Instruction* ip, const Value* cvp, Value* stack) {
            c.sp++;

            if (const Value temp = stack[c.bp + ip->arg]; temp.hint() == ValueScalarHint::local_id) {
                stack[c.sp] = stack[c.bp + temp.scalar()];
            } else {
                stack[c.sp] = temp;
            }

            ip++;

            [[clang::musttail]]
            return dispatch(c, ip, cvp, stack);
        }

        [[nodiscard]] static constexpr EvalStatus op_set_local(EvalContext& c, const Instruction* ip, const Value* cvp, Value* stack) {
            stack[c.bp + ip->arg] = stack[c.sp];
            c.sp--;
            ip++;

            [[clang::musttail]]
            return dispatch(c, ip, cvp, stack);
        }

        [[nodiscard]] static constexpr EvalStatus op_push_obj(EvalContext& c, const Instruction* ip, const Value* cvp, Value* stack) {
            return EvalStatus::unsupported_op;
        }

        [[nodiscard]] static constexpr EvalStatus op_get_prop(EvalContext& c, const Instruction* ip, const Value* cvp, Value* stack) {
            // ? NOTE: This indexing locates the target object to access something from.
            const std::uint32_t base_slot = c.sp - static_cast<std::uint32_t>(ip->arg);
            auto temp_obj_ptr = c.heap.at(stack[base_slot].scalar());

            for (std::uint32_t key_offset = 0; key_offset < ip->arg; key_offset++) {
                // ? NOTE: Start iterating here through the sequence of keys from the spot just above the target object...
                if (const Value temp_key = stack[base_slot + 1 + key_offset]; temp_key.hint() == ValueScalarHint::integer) {
                    stack[base_slot] = temp_obj_ptr->get_property(std::addressof(c), temp_key.scalar());
                } else if (temp_key.hint() == ValueScalarHint::heap_id) {
                    stack[base_slot] = temp_obj_ptr->get_property(std::addressof(c), temp_key, true);
                } else {
                    return EvalStatus::bad_op_arg;
                }
            }

            c.sp = base_slot;
            ip++;

            [[clang::musttail]]
            return dispatch(c, ip, cvp, stack);
        }

        [[nodiscard]] static constexpr EvalStatus op_set_prop(EvalContext& c, const Instruction* ip, const Value* cvp, Value* stack) {
            // ? NOTE: This indexing locates the target object to access something from.
            const std::uint32_t base_slot = c.sp - static_cast<std::uint32_t>(ip->arg) - 1;
            auto temp_obj_ptr = c.heap.at(stack[base_slot].scalar());

            for (std::uint32_t key_offset = 0; key_offset < ip->arg - 1; key_offset++) {
                // ? NOTE: Start iterating here through the sequence of keys from the spot just above the target object...
                if (const Value temp_key = stack[base_slot + 1 + key_offset]; temp_key.hint() == ValueScalarHint::integer) {
                    stack[base_slot] = temp_obj_ptr->get_property(std::addressof(c), temp_key.scalar());
                } else if (temp_key.hint() == ValueScalarHint::heap_id) {
                    stack[base_slot] = temp_obj_ptr->get_property(std::addressof(c), temp_key, true);
                } else {
                    return EvalStatus::bad_op_arg;
                }

                if (const auto& next_target = stack[base_slot].hint() == ValueScalarHint::heap_id) {
                    temp_obj_ptr = c.heap.at(stack[base_slot].scalar());
                } else {
                    return EvalStatus::bad_op_arg;
                }

                if (temp_obj_ptr == nullptr) {
                    return EvalStatus::bad_op_arg;
                }
            }

            // ? NOTE: layout of property-set for stack:
            // * Example: object.foo.bar = 1234
            // * Top: |  1234  | <-- SET: result.bar = 1234
            // *      |  .bar  | <-- STOP access loop here!
            // *      |  .foo  |
            // * Btm: | object | <-- This is the base_slot. The SP reverts here.
            if (const Value edit_key = stack[c.sp - 1], result = stack[c.sp]; edit_key.hint() == ValueScalarHint::integer) {
                temp_obj_ptr->set_property(std::addressof(c), edit_key.scalar(), result);
                stack[base_slot] = result;
                c.sp = base_slot;
            } else if (edit_key.hint() == ValueScalarHint::heap_id) {
                temp_obj_ptr->set_property(std::addressof(c), edit_key, result, true);
                stack[base_slot] = result;
                c.sp = base_slot;
            } else {
                return EvalStatus::bad_op_arg;
            }

            ip++;

            [[clang::musttail]]
            return dispatch(c, ip, cvp, stack);
        }

        [[nodiscard]] static constexpr EvalStatus op_pop(EvalContext& c, const Instruction* ip, const Value* cvp, Value* stack) {
            c.sp -= ip->arg; //? use lazy deletions
            ip++;

            [[clang::musttail]]
            return dispatch(c, ip, cvp, stack);
        }

        [[nodiscard]] static constexpr EvalStatus op_make_array(EvalContext& c, const Instruction* ip, const Value* cvp, Value* stack) {
            const std::uint32_t base_pos = c.sp - static_cast<std::uint32_t>(ip->arg) + 1;
            const std::span<Value> temps {
                stack + base_pos,
                stack + c.sp + 1,
            };

            if (auto array_table = new Table {}; array_table) {
                for (int next_index = 0; const auto& v : temps) {
                    array_table->set_property(std::addressof(c), next_index, v);
                    next_index++;
                }

                c.sp = base_pos;
                stack[c.sp] = Value::create_from_id(c.heap.store(array_table), HeapIdOpt {});
                ip++;
            } else {
                return EvalStatus::alloc_fail;
            }

            [[clang::musttail]]
            return dispatch(c, ip, cvp, stack);
        }

        [[nodiscard]] static constexpr EvalStatus op_make_object(EvalContext& c, const Instruction* ip, const Value* cvp, Value* stack) {
            return EvalStatus::unsupported_op;
        }

        // TODO: Should be for iterator.next() later on ??
        [[nodiscard]] static constexpr EvalStatus op_deref(EvalContext& c, const Instruction* ip, const Value* cvp, Value* stack) {
            return EvalStatus::unsupported_op;
        }

        [[nodiscard]] static constexpr EvalStatus op_negate_bool(EvalContext& c, const Instruction* ip, const Value* cvp, Value* stack) {
            c.sp++;

            if (const Value temp = stack[c.sp - 1]; !temp.is_nan() || temp.hint() != ValueScalarHint::boolean) {
                stack[c.sp] = Value::create_as_nan();
            } else {
                stack[c.sp] = Value::create_from_bool(temp.scalar() == 0);
            }

            ip++;

            [[clang::musttail]]
            return dispatch(c, ip, cvp, stack);
        }

        [[nodiscard]] static constexpr EvalStatus op_negate_num(EvalContext& c, const Instruction* ip, const Value* cvp, Value* stack) {
            c.sp++;

            if (const Value temp = stack[c.sp - 1]; !temp.is_nan()) {
                stack[c.sp] = Value::create_from_double(-temp.as_double());
            } else if (temp.hint() == ValueScalarHint::integer) {
                stack[c.sp] = Value::create_from_int(-temp.scalar());
            } else {
                //? Handle NaN case: -NaN is still NaN !
                stack[c.sp] = Value::create_as_nan();
            }

            ip++;

            [[clang::musttail]]
            return dispatch(c, ip, cvp, stack);
        }

        [[nodiscard]] static constexpr EvalStatus op_mod(EvalContext& c, const Instruction* ip, const Value* cvp, Value* stack) {
            return EvalStatus::unsupported_op;
        }

        [[nodiscard]] static constexpr EvalStatus op_mul(EvalContext& c, const Instruction* ip, const Value* cvp, Value* stack) {
            c.sp--;

            if (const Value lhs = stack[c.sp], rhs = stack[c.sp + 1]; !lhs.is_nan() && !rhs.is_nan()) {
                stack[c.sp] = Value::create_from_double(lhs.as_double() * rhs.as_double());
            } else if (const auto lhs_hint = lhs.hint(), rhs_hint = rhs.hint(); lhs_hint == rhs_hint) {
                switch (lhs_hint) {
                    case ValueScalarHint::integer:
                        stack[c.sp] = Value::create_from_int(lhs.scalar() * rhs.scalar());
                        break;
                    default:
                        stack[c.sp] = Value::create_as_nan();
                        break;
                }
            } else {
                stack[c.sp] = Value::create_as_nan();
            }

            ip++;

            [[clang::musttail]]
            return dispatch(c, ip, cvp, stack);
        }

        [[nodiscard]] static constexpr EvalStatus op_div(EvalContext& c, const Instruction* ip, const Value* cvp, Value* stack) {
            c.sp--;

            if (const Value lhs = stack[c.sp], rhs = stack[c.sp + 1]; lhs.hint() == ValueScalarHint::real && rhs.hint() == ValueScalarHint::real) {
                if (const auto lhs_float = lhs.as_double(), rhs_float = rhs.as_double(); lhs_float == 0.0 && rhs_float == 0.0) {
                    stack[c.sp] = Value::create_as_nan();
                } else if (lhs_float > 0.0 && rhs_float == 0.0) {
                    stack[c.sp] = Value::create_as_inf();
                } else if (lhs_float < 0.0 && rhs_float == 0.0) {
                    stack[c.sp] = Value::create_as_neg_inf();
                } else {
                    stack[c.sp] = Value::create_from_double(lhs_float / rhs_float);
                }
            } else if (const auto lhs_hint = lhs.hint(), rhs_hint = rhs.hint(); lhs_hint == rhs_hint) {
                if (lhs_hint == ValueScalarHint::integer && rhs.scalar() != 0) {
                    stack[c.sp] = Value::create_from_int(lhs.scalar() / rhs.scalar());
                } else if (lhs_hint == ValueScalarHint::integer && lhs.scalar() > 0) {
                    stack[c.sp] = Value::create_as_inf();
                } else if (lhs_hint == ValueScalarHint::integer && lhs.scalar() < 0) {
                    stack[c.sp] = Value::create_as_neg_inf();
                } else {
                    stack[c.sp] = Value::create_as_nan();
                }
            } else {
                stack[c.sp] = Value::create_as_nan();
            }

            ip++;

            [[clang::musttail]]
            return dispatch(c, ip, cvp, stack);
        }

        [[nodiscard]] static constexpr EvalStatus op_add(EvalContext& c, const Instruction* ip, const Value* cvp, Value* stack) {
            c.sp--;

            if (const Value lhs = stack[c.sp], rhs = stack[c.sp + 1]; !lhs.is_nan() && !rhs.is_nan()) {
                stack[c.sp] = Value::create_from_double(lhs.as_double() + rhs.as_double());
            } else if (const auto lhs_hint = lhs.hint(), rhs_hint = rhs.hint(); lhs_hint == rhs_hint) {
                switch (lhs_hint) {
                    case ValueScalarHint::integer:
                        stack[c.sp] = Value::create_from_int(lhs.scalar() + rhs.scalar());
                        break;
                    default:
                        stack[c.sp] = Value::create_as_nan();
                        break;
                }
            } else {
                stack[c.sp] = Value::create_as_nan();
            }

            ip++;

            [[clang::musttail]]
            return dispatch(c, ip, cvp, stack);
        }

        [[nodiscard]] static constexpr EvalStatus op_sub(EvalContext& c, const Instruction* ip, const Value* cvp, Value* stack) {
            c.sp--;

            if (const Value lhs = stack[c.sp], rhs = stack[c.sp + 1]; !lhs.is_nan() && !rhs.is_nan()) {
                stack[c.sp] = Value::create_from_double(lhs.as_double() - rhs.as_double());
            } else if (const auto lhs_hint = lhs.hint(), rhs_hint = rhs.hint(); lhs_hint == rhs_hint) {
                switch (lhs_hint) {
                    case ValueScalarHint::integer:
                        stack[c.sp] = Value::create_from_int(lhs.scalar() - rhs.scalar());
                        break;
                    default:
                        stack[c.sp] = Value::create_as_nan();
                        break;
                }
            } else {
                stack[c.sp] = Value::create_as_nan();
            }

            ip++;

            [[clang::musttail]]
            return dispatch(c, ip, cvp, stack);
        }


        [[nodiscard]] static constexpr EvalStatus op_compare_eq(EvalContext& c, const Instruction* ip, const Value* cvp, Value* stack) {
            c.sp--;

            if (const Value lhs = stack[c.sp], rhs = stack[c.sp + 1]; !lhs.is_nan() && !rhs.is_nan()) {
                stack[c.sp] = Value::create_from_bool(lhs.as_double() == rhs.as_double());
            } else if (const auto lhs_hint = lhs.hint(), rhs_hint = rhs.hint(); lhs_hint == rhs_hint) {
                switch (lhs_hint) {
                case ValueScalarHint::null:
                    stack[c.sp] = Value::create_from_bool(true);
                    break;
                case ValueScalarHint::boolean:
                case ValueScalarHint::integer:
                case ValueScalarHint::local_id:
                case ValueScalarHint::heap_id:
                    stack[c.sp] = Value::create_from_bool(lhs.scalar() == rhs.scalar());
                    break;
                default:
                    stack[c.sp] = Value::create_from_bool(false);
                    break;
                }
            } else {
                stack[c.sp] = Value::create_from_bool(false);
            }

            ip++;

            [[clang::musttail]]
            return dispatch(c, ip, cvp, stack);
        }

        [[nodiscard]] static constexpr EvalStatus op_compare_ne(EvalContext& c, const Instruction* ip, const Value* cvp, Value* stack) {
            c.sp--;

            if (const Value lhs = stack[c.sp], rhs = stack[c.sp + 1]; !lhs.is_nan() && !rhs.is_nan()) {
                stack[c.sp] = Value::create_from_bool(lhs.as_double() != rhs.as_double());
            } else if (const auto lhs_hint = lhs.hint(), rhs_hint = rhs.hint(); lhs_hint == rhs_hint) {
                switch (lhs_hint) {
                case ValueScalarHint::null:
                    stack[c.sp] = Value::create_from_bool(false);
                    break;
                case ValueScalarHint::boolean:
                case ValueScalarHint::integer:
                case ValueScalarHint::local_id:
                case ValueScalarHint::heap_id:
                    stack[c.sp] = Value::create_from_bool(lhs.scalar() != rhs.scalar());
                    break;
                default:
                    stack[c.sp] = Value::create_from_bool(true);
                    break;
                }
            } else {
                stack[c.sp] = Value::create_from_bool(true);
            }

            ip++;

            [[clang::musttail]]
            return dispatch(c, ip, cvp, stack);
        }

        [[nodiscard]] static constexpr EvalStatus op_compare_lt(EvalContext& c, const Instruction* ip, const Value* cvp, Value* stack) {
            c.sp--;

            if (const Value lhs = stack[c.sp], rhs = stack[c.sp + 1]; !lhs.is_nan() && !rhs.is_nan()) {
                stack[c.sp] = Value::create_from_bool(lhs.as_double() < rhs.as_double());
            } else if (const auto lhs_hint = lhs.hint(), rhs_hint = rhs.hint(); lhs_hint == rhs_hint) {
                switch (lhs_hint) {
                case ValueScalarHint::boolean:
                case ValueScalarHint::integer:
                    stack[c.sp] = Value::create_from_bool(lhs.scalar() < rhs.scalar());
                    break;
                default:
                    stack[c.sp] = Value::create_from_bool(false);
                    break;
                }
            } else {
                stack[c.sp] = Value::create_from_bool(false);
            }

            ip++;

            [[clang::musttail]]
            return dispatch(c, ip, cvp, stack);
        }

        [[nodiscard]] static constexpr EvalStatus op_compare_lte(EvalContext& c, const Instruction* ip, const Value* cvp, Value* stack) {
            c.sp--;

            if (const Value lhs = stack[c.sp], rhs = stack[c.sp + 1]; !lhs.is_nan() && !rhs.is_nan()) {
                stack[c.sp] = Value::create_from_bool(lhs.as_double() <= rhs.as_double());
            } else if (const auto lhs_hint = lhs.hint(), rhs_hint = rhs.hint(); lhs_hint == rhs_hint) {
                switch (lhs_hint) {
                case ValueScalarHint::boolean:
                case ValueScalarHint::integer:
                    stack[c.sp] = Value::create_from_bool(lhs.scalar() <= rhs.scalar());
                    break;
                default:
                    stack[c.sp] = Value::create_from_bool(false);
                    break;
                }
            } else {
                stack[c.sp] = Value::create_from_bool(false);
            }

            ip++;

            [[clang::musttail]]
            return dispatch(c, ip, cvp, stack);
        }

        [[nodiscard]] static constexpr EvalStatus op_compare_gt(EvalContext& c, const Instruction* ip, const Value* cvp, Value* stack) {
            c.sp--;

            if (const Value lhs = stack[c.sp], rhs = stack[c.sp + 1]; !lhs.is_nan() && !rhs.is_nan()) {
                stack[c.sp] = Value::create_from_bool(lhs.as_double() > rhs.as_double());
            } else if (const auto lhs_hint = lhs.hint(), rhs_hint = rhs.hint(); lhs_hint == rhs_hint) {
                switch (lhs_hint) {
                case ValueScalarHint::boolean:
                case ValueScalarHint::integer:
                    stack[c.sp] = Value::create_from_bool(lhs.scalar() > rhs.scalar());
                    break;
                default:
                    stack[c.sp] = Value::create_from_bool(false);
                    break;
                }
            } else {
                stack[c.sp] = Value::create_from_bool(false);
            }

            ip++;

            [[clang::musttail]]
            return dispatch(c, ip, cvp, stack);
        }

        [[nodiscard]] static constexpr EvalStatus op_compare_gte(EvalContext& c, const Instruction* ip, const Value* cvp, Value* stack) {
            c.sp--;

            if (const Value lhs = stack[c.sp], rhs = stack[c.sp + 1]; !lhs.is_nan() && !rhs.is_nan()) {
                stack[c.sp] = Value::create_from_bool(lhs.as_double() >= rhs.as_double());
            } else if (const auto lhs_hint = lhs.hint(), rhs_hint = rhs.hint(); lhs_hint == rhs_hint) {
                switch (lhs_hint) {
                case ValueScalarHint::boolean:
                case ValueScalarHint::integer:
                    stack[c.sp] = Value::create_from_bool(lhs.scalar() >= rhs.scalar());
                    break;
                default:
                    stack[c.sp] = Value::create_from_bool(false);
                    break;
                }
            } else {
                stack[c.sp] = Value::create_from_bool(false);
            }

            ip++;

            [[clang::musttail]]
            return dispatch(c, ip, cvp, stack);
        }

        [[nodiscard]] static constexpr EvalStatus op_test(EvalContext& c, const Instruction* ip, const Value* cvp, Value* stack) {
            if (const Value temp = stack[c.sp]; !temp.is_nan()) {
                stack[c.sp + 1] = Value::create_from_bool(temp.as_double() != 0.0);
            } else {
                switch (temp.hint()) {
                case ValueScalarHint::null:
                    stack[c.sp + 1] = Value::create_from_bool(false);
                    break;
                case ValueScalarHint::boolean:
                    stack[c.sp + 1] = Value::create_from_bool(temp.scalar() != 0);
                    break;
                case ValueScalarHint::integer:
                    stack[c.sp + 1] = Value::create_from_bool(temp.scalar() != 0);
                    break;
                default:
                    return EvalStatus::bad_op_arg;
                }
            }

            c.sp++;
            ip++;

            [[clang::musttail]]
            return dispatch(c, ip, cvp, stack);
        }

        [[nodiscard]] static constexpr EvalStatus op_jump(EvalContext& c, const Instruction* ip, const Value* cvp, Value* stack) {
            ip += ip->arg;

            [[clang::musttail]]
            return dispatch(c, ip, cvp, stack);
        }

        [[nodiscard]] static constexpr EvalStatus op_jump_back(EvalContext& c, const Instruction* ip, const Value* cvp, Value* stack) {
            ip -= ip->arg;

            [[clang::musttail]]
            return dispatch(c, ip, cvp, stack);
        }

        [[nodiscard]] static constexpr EvalStatus op_jump_if(EvalContext& c, const Instruction* ip, const Value* cvp, Value* stack) {
            if (stack[c.sp].scalar() != 0) {
                ip += ip->arg;
            } else {
                c.sp--;
                ip++;
            }

            [[clang::musttail]]
            return dispatch(c, ip, cvp, stack);
        }

        [[nodiscard]] static constexpr EvalStatus op_jump_else(EvalContext& c, const Instruction* ip, const Value* cvp, Value* stack) {
            //? NOTE: In the official handlers, this would lack the boolean test logic, assuming that temp is already a boolean.
            if (stack[c.sp].scalar() == 0) {
                ip += ip->arg;
            } else {
                c.sp--;
                ip++;
            }

            [[clang::musttail]]
            return dispatch(c, ip, cvp, stack);
        }

        [[nodiscard]] static constexpr EvalStatus op_call_ctor(EvalContext& c, const Instruction* ip, const Value* cvp, Value* stack) {
            return EvalStatus::unsupported_op;
        }

        [[nodiscard]] static constexpr EvalStatus op_call_fun(EvalContext& c, const Instruction* ip, const Value* cvp, Value* stack) {
            if (auto object = c.heap.at(static_cast<int>(stack[c.sp - static_cast<std::uint32_t>(ip->arg)].scalar())); object == nullptr) {
                return EvalStatus::bad_op_arg;
            } else if (auto chunk_addr = object->get_code_data(); !chunk_addr) {
                return EvalStatus::bad_op_arg;
            } else {
                //? NOTE: If the VM object is callable, fetch its bytecode chunk as a pointer, saving / restoring caller state via the NATIVE stack.
                // const Instruction* caller_ret_ip_v = ip + 1;
                const auto chunk_p = reinterpret_cast<const Chunk*>(chunk_addr);
                const std::uint16_t caller_bp_v = c.bp;
                const std::uint16_t callee_bp_v = c.sp - ip->arg;

                c.depth++;
                c.bp = callee_bp_v;

                c.status = dispatch(c, chunk_p->code.data(), chunk_p->consts.data(), stack);

                stack[callee_bp_v - 1] = stack[c.sp];
                c.sp = callee_bp_v - 1;
                c.bp = caller_bp_v;
            }

            if (c.status != EvalStatus::pending) {
                return c.status;
            }

            [[clang::musttail]]
            return dispatch(c, ip + 1, cvp, stack);
        }

        [[nodiscard]] static constexpr EvalStatus op_call_native(EvalContext& c, const Instruction* ip, const Value* cvp, Value* stack) {
            if (auto object = c.heap.at(static_cast<int>(stack[c.sp - static_cast<std::uint32_t>(ip->arg)].scalar())); object == nullptr) {
                return EvalStatus::bad_op_arg;
            } else if (auto native_fp = object->get_native_fn_ptr(); !native_fp) {
                return EvalStatus::bad_op_arg;
            } else {
                c.status = native_fp(std::addressof(c), static_cast<std::uint8_t>(ip->arg));
                ip++;
            }

            [[clang::musttail]]
            return dispatch(c, ip, cvp, stack);
        }

        [[nodiscard]] static constexpr EvalStatus op_ret(EvalContext& c, const Instruction* ip, const Value* cvp, Value* stack) {
            c.depth--;

            [[unlikely]]
            if (c.depth < 1) {
                stack[0] = stack[c.sp];
                c.sp = 0;
                c.bp = 1;

                if (c.status == EvalStatus::pending) {
                    c.status = EvalStatus::ok;
                }
            }

            return c.status;
        }

        // ! CAVEAT: lower arg byte is always the local ID & higher arg byte is always the constant ID. However, the encoding only works for IDs 0-255 !
        [[nodiscard]] static constexpr EvalStatus op_padd_lk(EvalContext& c, const Instruction* ip, const Value* cvp, Value* stack) {
            c.sp++;

            if (const Value local = stack[c.bp + static_cast<std::uint16_t>(ip->arg & 0xff)], konst = cvp[static_cast<std::uint16_t>(ip->arg & 0xff00) >> 8]; !local.is_nan() && !konst.is_nan()) {
                stack[c.sp] = Value::create_from_double(local.as_double() + konst.as_double());
            } else if (const auto lhs_hint = local.hint(), rhs_hint = konst.hint(); lhs_hint == rhs_hint) {
                switch (lhs_hint) {
                    case ValueScalarHint::integer:
                        stack[c.sp] = Value::create_from_int(local.scalar() + konst.scalar());
                        break;
                    default:
                        stack[c.sp] = Value::create_as_nan();
                        break;
                }
            } else {
                stack[c.sp] = Value::create_as_nan();
            }

            ip++;

            [[clang::musttail]]
            return dispatch(c, ip, cvp, stack);
        }

        [[nodiscard]] static constexpr EvalStatus op_psub_lk(EvalContext& c, const Instruction* ip, const Value* cvp, Value* stack) {
            c.sp++;

            if (const Value local = stack[c.bp + static_cast<std::uint16_t>(ip->arg & 0xff)], konst = cvp[static_cast<std::uint16_t>(ip->arg & 0xff00) >> 8]; !local.is_nan() && !konst.is_nan()) {
                stack[c.sp] = Value::create_from_double(local.as_double() - konst.as_double());
            } else if (const auto lhs_hint = local.hint(), rhs_hint = konst.hint(); lhs_hint == rhs_hint) {
                switch (lhs_hint) {
                    case ValueScalarHint::integer:
                        stack[c.sp] = Value::create_from_int(local.scalar() - konst.scalar());
                        break;
                    default:
                        stack[c.sp] = Value::create_as_nan();
                        break;
                }
            } else {
                stack[c.sp] = Value::create_as_nan();
            }

            ip++;

            [[clang::musttail]]
            return dispatch(c, ip, cvp, stack);
        }

        //* Handler dispatch table:
        static constexpr std::array<handler_type, static_cast<std::size_t>(Opcode::last)> handlers_v1_funcs = {
            &op_nop,
            &op_dup, &op_push_null, &op_push_bool, &op_push_callee,
            &op_push_global, &op_push_const,
            &op_get_local, &op_set_local,
            &op_get_prop, &op_set_prop,
            &op_pop,
            &op_make_array, &op_make_object,
            &op_deref,
            &op_negate_bool, &op_negate_num,
            &op_mod, op_mul, &op_div, op_add, &op_sub,
            &op_compare_eq, &op_compare_ne, &op_compare_lt, &op_compare_lte, &op_compare_gt, &op_compare_gte, &op_test,
            &op_jump, &op_jump_back, &op_jump_if, &op_jump_else,
            &op_call_ctor, &op_call_fun, &op_call_native, &op_ret,
            &op_padd_lk, &op_psub_lk,
        };

        static constexpr EvalStatus dispatch(EvalContext& context, const Instruction* ip, const Value* constants, Value* stack) {
            [[clang::musttail]]
            return handlers_v1_funcs[static_cast<std::size_t>(ip->op)](context, ip, constants, stack);
        }
    };
}
