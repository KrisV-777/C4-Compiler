# C4 Compiler

A compiler for a subset of C, which I wrote as part of a university course I took during winter 2024. The project was originally intended to be done in a group of three and was divided into multiple phases to be worked on step wise throughout the semester, however my group quit halfway through the first phase, and I ended up implementing the entire project myself (and therefore not having enough to fix all issues and complete the bonus tasks, sad sad).  
The implementation follows the [ISO Standard, Draft N1570](https://www.open-std.org/jtc1/sc22/wg14/www/docs/n1570.pdf) under the given constraints:

## Expressions (§6.5)
Only the given expressions are handled fully:
- Identifiers, constants, and string literals
- Parenthesized expressions
- Array subscripting (`[]`)
- Function calls
- Member access (`.` and `->`)
- `sizeof` operator
- Unary operators: `&`, `*`, `-`, `!`
- Binary operators: `*`, `+`, `-`, `<`, `==`, `!=`, `&&`, `||`
- Ternary operator (`?:`)
- Assignment (`=`)

For all other expressions, only chain productions (A → B) are used.

## Declarations (§6.7)
Follow the standard fully with the given restrictions:
- Only one `init-declarator` (without an initializer) is supported in an `init-declarator-list`.
- `declaration-specifiers` and `specifier-qualifier-list` only allow `type-specifier`.
- `type-specifier` is limited to: `void`, `char`, `int`, and `struct-or-union-specifier`.
  - Only `struct` is considered, without type qualifiers or bit fields.
- `declarator` and `direct-declarator` support:
  - Pointers (without type-qualifier lists)
  - Identifiers
  - Parenthesized declarators
  - Function declarators with `parameter-type-list`
- `parameter-type-list` only includes `parameter-list` (no variadic arguments `...`).
- All productions for `parameter-declaration` are included.
- `abstract-declarator` and `direct-abstract-declarator` follow the same restrictions.

## Statements (§6.8)
Implemented statements:
- Labeled statements with an identifier
- Compound statements
- Expression statements (both null and regular expressions)
- Selection statements: `if` and `if-else`
- Iteration statements: `while`
- All jump statements

## External Definitions (§6.9)
Fully supported, except that `declaration-list` within function definitions is not included.


# Implementation Progress

The project has implemented:
- Lexing
- Parsing
- Reconstruction of parsed C Code
- Semantic Analysis
- Code Generation

Optimizations have been omitted from the implementation (because time is limited, n all that, I suppose).

# Known Issues

- Lexing
  - None
- Parsing
  - None
- Reconstruction
  - None
- Semantic Analysis
  - Assignments of Function Pointers
  - Shadowing/Overwrites
- Code Generation
  - Member access of a dereferenced struct (`(*S).a`)
  - Generated code tends to run into UB in various instances, despite the underlying C code being correct

(Would be nice if I had examples of failing test cases, wouldn't it?)
