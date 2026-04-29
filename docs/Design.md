### Edna Language - Design

#### Background
A good scripting language should be flexible, expressive, and simple to not hinder the programmer. However, existing ones have issues... Their type system might be too loose. Their syntax might be atrocious like Bash scripts or too verbose. Enter Edna, my personal replacement for all other scripting languages I've used. Let's make scripting actually enjoyable, hence the name Edna, meaning "pleasure".

#### Philosophy
 - Programmers use tools to ease work.
    - The language's feature-set should be lean and more explicit.
    - The semantics should be easy to grasp.
    - NO type coercion unless it makes sense. (JavaScript is agony)
 - Code should read like fine fanfiction (yes, it should be extensible like fanfiction too).

#### Features
 - Extenders:
    - User defined operators with custom precedence levels (TODO)
    - AST macros (TODO)
 - Types:
    - Strict equality is similar to JS: if the types are different, it's a no-go.
    - No type conversions, as there are explicit type conversion functions (TODO)
 - Functions:
    - Can be procedures or constructors.
    - Procedures have custom capture clauses for finer control.
    - Constructors are procedures that just assemble a dynamic object with properties.
    - Can have a trailing pack parameter.
 - Objects:
    - Prototype-based OO with qualifiers:
        - `sealed` vs. `frozen`
    - Prototype chain is meant for inherited methods, etc... Only singular inheritance is allowed.
    - Classes will be syntax sugar for a table with an injected prototype.
    - Methods take an implicit `self` parameter to their caller object.
    - Use `super` to refer to the next prototype.
 - Syntax:
    - Rust mixed with JS

```
; line comment here

mut foo = 42

; custom operators
symbol `?` prec unary `-` = fun (a) => a == null

; use custom null-check operator
let isAnswer = ?foo

fun answer(x) uses (foo, bar, baz) => x + foo

fun fib(n) {
    cond {
        case n < 2 => { n }
        else => { fib(n - 1) + fib(n - 2) }
    }
}

fun matchAny(arg, ...targets) => targets.any(
    fun (item) uses arg => arg == item
)

let foo = [1, 2, 3, 4]
let count = foo.len()
let foo_it = foo.iter()
let foo_rev = foo.iter_r()
let one = foo_it.peek()
let four = foo_it.skip(3)
foo.reverse()
let dud = foo_it.done()
let median = (foo.1 + foo.2) / 2

```

#### Roadmap
 1. Add native print function support. **OK**
 2. Refactor bytecode compiler & runtime into classes. **OK**
 3. Add peephole optimization passes for bytecode: **OK**
    - Place super instructions.
    - Remove non-trailing NOPs.
 4. Implement native prototype support: This is crucial for tables! **WIP**
   - Create general list prototype.
   - Create registry methods for native prototypes e.g list's...
   - Test tables in programs.
 5. Add simple methods for tables:
    - Getters: `empty(), len(), has(key)`
    - Iterators: `iter(), iter_r(), peek(offset), skip(), skip_n(skips), done()`
