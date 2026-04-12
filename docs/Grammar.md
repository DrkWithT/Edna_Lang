### Grammar

#### Other
```
<comment> = ";" <NOT-LF>* LF
```

#### Expressions
```
<primary> = "null" | <boolean> | <int> | <real> | <string> | <identifier> | <array> | <block> | <cond> | <lambda> | "(" <expr> ")"
<array> = "[" ( <expr> ( "," <expr> )* )? "]"
<block> = "{" <stmt>+ "}"
    ; NOTE: all blocks have their trailing expr-statement as a return!
<cond> = "cond" "{" <cases>* <else> "}"
<cases> = ( "case" <compare> "=>" <expr> )*
<else> = "else" "=>" <expr>
<lambda> = "fun" "(" <identifier> ( "," <identifer> ) ( "..." <identifier> )? ")" ("uses" <identifier>)? "=>" <expr>
<lhs> = <primary> ("." <identifier>)*
<call> = <lhs> ( "(" <expr> ( "," <expr> )* ")" )?
<unary> = ( "-" | "!" )? <call>
<factor> = <unary> ( ("*" | "/") <unary> )*
<term> = <factor> ( ("+" | "-") <factor> )*
<equality> = <term> ( ("==" | "!=") <term> )*
<compare> = <equality> ( ( "<" | ">" | ">=" | "<=" ) <equality> )*
<and> = <compare> ("&&" <compare>)*
<or> = <and> ("||" <and>)*
<expr> = <or>
```

#### Statements
```
<program> = <stmt>
<stmt> = <function> | <var> | <symbol-def> | <expr-stmt> ; function decls are like hoisted lambda variables
<function> = "fun" "(" <identifier> ( "," <identifer> ) ")" "=>" <expr>
<var> = ( "let" | "mut" ) ( <identifier> "=" <expr> )+
<operator-literal> = <OP_SYMBOLS>+
<symbol-def> = "symbol" <operator-literal> "prec" <operator-literal> "=" <lambda>
<expr-stmt> = <call> ( "=" <expr> )?
```

#### Future Expressions
```
<object> = ( "sealed" | "frozen" )? "{" ( <identifier> ":" <expr> )* "}"
```

#### Future Syntax
```
<macro> = "macro" <identifier> <macro-args> <macro-body>
    ; NOTE: call a macro as #<identifier>
<macro-args> = "(" <macro-params> ")"
<macro-params> = ("literal" | "operator" | "expr") <identifier> "..."?
<macro-body> = <block>
    ; NOTE: macro args are substituted via $<identifier> and then syntactically & semantically checked!
```
