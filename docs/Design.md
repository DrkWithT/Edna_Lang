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
    - User defined operators with custom precedence levels
    - Function annotations
 - Types:
    - Strict equality is similar to JS: if the types are different, it's a no-go.
    - Type conversions should be closed and non-lossy e.g `bool` -> `int` -> `double`.
 - Functions:
    - Can be procedures or constructors.
    - Procedures have custom capture clauses for finer control.
    - Constructors are procedures that just assemble a dynamic object with properties.
    - Can have a trailing pack parameter.
 - Objects:
    - Prototype-based OO with explicit qualifiers:
        - `sealed` vs. `frozen`
 - Syntax:
    - Rust mixed with JS

```
; line comment here

mut foo = 42

; custom operators
symbol `?` prec unary `-` = fun (a) => a == null

; use custom null-check operator
let isAnswer = ?foo

let pairProto = frozen {
    display: fun () {
        mut s = ''

        s.append(self.first).append(',').append(self.second)
    }
}

let person = frozen {
    name: 'Jane Doe',
    age: 23,
    proto: pairProto
}

fun answer(x) uses foo => x + foo

fun fib(n) => {
    cond {
        case n < 2 => { n }
        else => { fib(n - 1) + fib(n - 2) }
    }
}

fun matchAny(arg, ...targets) => targets.any(
    fun (item) uses arg => arg == item
)

ctor Pair(a, b) extends pairProto => {
    self.first = a
    self.second = b
    self
}

let printingPair = new Pair(10, 20)

print(printingPair.display())
```
