### Runtime Notes

#### Value:
 - NaN-boxed `std::uint64_t`:
 - Layout (MSB -> LSB):
    - sign bit: `0, 1` (1b)
    - quiet-NaN prefix: `7FFC` aka `*111_1111_1111_1100` (13 / 16 b)
    - extra bits: (??)
        - padding: (about 11b)
        - 32-bit integer: possibly a scalar primitive, local-id, or object-slot id
        - 4-bit integer: tiny tag
            - Tags: `0 => null, 1 => bool, 2 => int, 3 => local, 4 => heap`

#### Object Model:
 - Hybrid structure:
    - `PropMap`: type `std::map<ObjectBase*, Item>`
    - `ItemPool`: type `std::vector<Item>`
 - Common operations:
    - `[test]`: as boolean
    - `[prototype]`: get instance / secret prototype
    - `[get]`: property / item read operation, taking (`Context& c`, `Value key | int pos`, `bool lookup_dynamic`)
    - `[set]`: property / item set operation, taking (`Context& c`, `Value item | int pos`, `bool lookup_dynamic`)
    - `[str]`: textual formatting
    - `[size]`: length of any iterable
    - `[call]`: invoke a function / lambda

#### VM:
 - Stack based with base offsets
 - Pre-allocated heap and stack, sized by user-passed limits.
 - Call frame _semantics_ contain callee stack BP, BASE_IP, and BASE-CVP with caller BP, IP. In the implementation, the native stack _itself_ is used in the `call_fun` opcode handler to save caller state.

#### GC:
 - Mark-and-Sweep collection:
    - Assume that each heap ID represents 1 allocated object. 
    - Stack object values are marked to the furthest objects in the heap via BFS.
    - Unreachable values are collected. Their heap IDs are saved to the free list.
    - Tenured objects, such as built-in `print`, cannot be collected.

#### Bytecode set:
 - NOP: No opcode or stack args needed: `IP++`
 - DUP: Push duplicate of top stack value: `STACK[SP + 1] = STACK[SP]`
 - PUSH_NULL: Pushes null value to the temporary stack.
 - PUSH_BOOL: Args `flag = 0 | 1`. Pushes a boolean to the temporary stack.
 - PUSH_SELF: Push callee / instance self-reference: Stack `<self>(CALLEE_BP - 1) <callee>(CALLEE_BP) <locals...>` to `... <self>`
   - **NOTE:** The callee reference always equals the `self` argument for plain functions.
 - PUSH_CONST: Args `const-id`, Stack: `...` to `<const-temp>`
 - GET_LOCAL: Args `local-offset`, Stack: `...` to `<temp>`
 - SET_LOCAL: Args `local-offset`, Stack-to-locals: `<local-Nth-temp>` fills `<local-N>`
 - PUSH_OBJ: Stack: `...` to `<heap-id>`
 - GET_PROP: Args: `<prop-key-count>`, Stack: `<object>, <props, (key-count)>` to `<object> <heap-id-or-int>`... if `prop-key-count < 1`, _squash both_ stack temps to the property value!
 - SET_PROP: Stack: `<object>, <key>, <value>` to `...`, all popped after side effect
 - POP: Args: `<pop-count>`, `SP -= POP_COUNT`
 - MAKE_ARRAY: Args: `<count>`, Stack: `<object> <items...>` to `<object>`
 - MAKE_OBJECT: Args: `<pair-count>`, Stack: `<object> (<key> <value>)*`
   - NOTE: add stubs of constructor functions that attach self.prototype, and self.proto...
 - DEREF: resolves a local "ref" to an actual Value.
 - NEGATE_BOOL: Stack: `<temp>` to `<temp-negated>`
 - NEGATE_NUM: Stack: `<temp>` to `<temp-negated>`
 - MOD: ...
 - MUL: ...
 - DIV: ...
 - ADD: ...
 - SUB: ...
 - TEST: Stack: `<temp>` to `<temp-bool>`
 - JUMP: Arg: `<absolute-offset>`
 - JUMP_BACK: Arg: `<absolute-offset>`
 - JUMP_EQ: Args: `<pops>`, Stack: `<lhs>, <rhs>` to `<temp-bool>`
 - JUMP_NE: Args: `<pops>`, Stack: `<lhs>, <rhs>` to `<temp-bool>`
 - JUMP_LT: Args: `<pops>`, Stack: `<lhs>, <rhs>` to `<temp-bool>`
 - JUMP_GT: Args: `<pops>`, Stack: `<lhs>, <rhs>` to `<temp-bool>`
 - JUMP_LTE: Args: `<pops>`, Stack: `<lhs>, <rhs>` to `<temp-bool>`
 - JUMP_GTE: Args: `<pops>`, Stack: `<lhs>, <rhs>` to `<temp-bool>`
 - CALL_CTOR: Arg `<arg-count>`, Stack: `<this-argument: object {}> <callee> <args...>` to `<temp-object>`
 - CALL_FUN: Args: `<arg-count>`, Stack: `<this-argument: object {...}> <callee> <args...>` to `<temp>`
 - RET
 - ~~THROW_OBJ~~
 - ~~CATCH_OBJ~~

### Calls:
Sample code:
`fun foo(x, y, z) => x + y + z`

Stack's Call Layout of `foo(1, 2, 3)`:
```
-1        CALLEE_BP                    +0  +1  +2...
<selfArg> <callee = callable-ref(foo)> <1> <2> <3>...
                                               SP
CALLEE_BP = SP - ARGC = SP - 3
```

### TODOs:
 - Variables:
   - If prepassing: only reserve the local slot.
   - Otherwise: generate LHS and RHS before appropriate SET_LOCAL / SET_PROP opcode.
