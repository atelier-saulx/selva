# RPN Filter Expressions

RPN filter expressions allows creating simple expressions for filtering lookups
in Hierarchy. The expressions are pure functions that can have no side effects
of any kind. An expression can only return boolean true or false, or its
execution can fail with an error. The name RPN comes from Reverse Polish
notation, which is the notation used in the expression language. Briefly the
benefit of using this notation is that the expressions don't need parenthesis
and it's very fast to parse because there are no precedence rules.

The RPN syntax consists of tokens that are separated by whitespace characters.

As an example, the following query selects all descendants of a node called
`grphnode_1` that are of type `2X`. It's also possible to write the same
expression using a single function but it wouldn't be as interesting example as
the following filter is.

```
SELVA.HIERARCHY.find test dfs descendants "grphnode_1" '$0 b "2X" d'
```

Breaking down the filter:

```
$1      [reg ref]   Reads a string value from the register 1.
b       [function]  Extracts the type string from the previous result.
"2X"    [operand 0] Is a string representing a node type.
d       [function]  Compares operand 0 with the result of the previous function.
```

## Syntax

**Literals**

```
L -> #<num>
L -> "<text>"
S -> {E}
E -> E,E
E -> L
```

Numeric literals can be entered in an expression by prefixing a base 10
integer or decimal form number with the `#` character.

String literals are passed as strings quoted with the `"` character.

Unordered sets can be passed as literals by grouping the set values with the
`{` and `}` characters similar to the common math notation for unordered sets.
The values in an unordered set must be delimited by a comma (`,`) and there
must be no whitespace characters or any other characters between the delimiter
and two values. A valid set element must follow a delimiter character, i.e. a
trailing comma is not allowed.

| Example         | Description                        |
| --------------- | ---------------------------------- |
| `#5`            | An integer literal.                |
| `#1.1`          | A real number passed as a literal. |
| `"Hello world"` | A string literal.                  |
| `{"a","b"}`     | A set of strings.                  |

It's advisable to pass user defined strings by in the RPN registers instead of
using literals, as this allows any data to be passed properly (even binary),
where as a string literal cannot contain nul-bytes or quotes. This rule is
somewhat comparable to why SQL queries are usually parametrized.

For example, instead of writing

```
SELVA.HIERARCHY.find test dfs descendants "grphnode_1" '"field" f "test c'
```

you should consider writing

```
SELVA.HIERARCHY.find test dfs descendants "grphnode_1" '"field" f $1 c' "test"
```

**Register references**

| Prefix | Description                      | Example        |
| ------ | -------------------------------- | -------------- |
| `@`    | Read an integer from a register. | `@1 => reg[1]` |
| `$`    | Read a string from a register.   | `$1 => reg[1]` |
| `&`    | Read a set from a register.      | `&1 => reg[1]` |

User registers start from index 1, and register number 0 is reserved for the current node ID.

**Arithmetic operators**

| Operator | Operands | Description              | Example (expr => result) |
| -------- | -------- | ------------------------ | ------------------------ |
| `A`      | `a + b`  | Addition operator.       | `#2 #1 A => 3`           |
| `B`      | `a - b`  | Subtraction operator.    | `#2 #1 B => 1`           |
| `C`      | `a / b`  | Division operator.       | `#2 #4 C => 2`           |
| `D`      | `a * b`  | Multiplication operator. | `#2 #2 D => 4`           |
| `E`      | `a % b`  | Remainder operator.      | `#4 #9 E => 1`           |

**Relational operators**

| Operator | Operands | Description                     | Example (expr => result) |
| -------- | -------- | ------------------------------- | ------------------------ |
| `F`      | `a == b` | Equality operator.              | `#1 #1 F => 1`           |
| `G`      | `a != b` | Not equal operator.             | `#1 #2 G => 1`           |
| `H`      | `a < b`  | Less than operator.             | `#2 #1 H => 1`           |
| `I`      | `a > b`  | Greater than operator.          | `#2 #1 I => 0`           |
| `J`      | `a <= b` | Less than or equal operator.    | `#2 #1 J => 1`           |
| `K`      | `a >= b` | Greater than or equal operator. | `#2 #1 K => 0`           |

**Logical operators**

| Operator | Operands      | Description                        | Example (expr => result) |
| -------- | ------------- | ---------------------------------- | ------------------------ |
| `L`      | `!a`          | Logical NOT operator. (unary)      | `#1 L => 0`              |
| `M`      | `a AND b`     | Logical AND operator.              | `#1 #1 M => 1`           |
| `N`      | `a OR b`      | Logical OR operator.               | `#0 #1 N => 1`           |
| `O`      | `!!a XOR !!b` | Logical XOR operator.              | `#1 #1 O => 0`           |
| `P`      | `□a`          | Necessity. (It's necessary that a) | `#0 P #1 N => 0`         |
| `Q`      | `◇a`          | Possibly.                          | `#1 Q #0 M => 1`         |
| `T`      | `a ? b : c`   | Ternary.                           | `$3 $2 @1 T => X`        |

`P` and `Q` are short circuiting operators and don't represent classical modal
logic. The `P` operator bails out immediately if the operand is not truthy and
`Q` bails out if the operand is truthy. To understand why these operators have
a slight difference to the definition in classical modal logic, let's consider
the dual pairing of the operators:

□a = ¬◇¬a
◇a = ¬□¬a

If the translate these definitions directly to the seemingly equivalent RPN
expressions, we'll get the following result:

¬◇¬a => a L Q L
¬□¬a => a L P L => 1

| `a` | `a L Q L` | result |
| :-: | :---------| -----: |
| `0` | `0 1 1 X` |    `1` |
| `1` | `1 0 0 1` |    `1` |

| `a` | `a L P L` | result |
| :-: | :---------| -----: |
| `0` | `0 1 1 0` |    `0` |
| `1` | `1 0 0 X` |    `0` |


Therefore, neither of these yields the expected result.

**Set Operations**

| Operator | Operands          | Description                           | Example (expr => result) |
| -------- | ----------------- | ------------------------------------- | ------------------------ |
| `z`      | `C = A ∪ B`       | Union of A and B.                     | `B A a => C`             |

**Functions**

| Operator | Arguments         | Description                           | Example (expr => result) |
| -------- | ----------------- | ------------------------------------- | ------------------------ |
| `a`      | `set has a`       | `has` function for SelvaSets.         | `"c" ["a", "b"] a => 0`  |
|          |                   |                                       | `"c" "field" a => 0`     |
| `b`      | `id`              | Returns the type of a node id.        | `"xy123" b => "xy"`      |
| `c`      | `!strcmp(s1, s2)` | Compare strings.                      | `$1 "hello" c => 1`      |
| `d`      | `!cmp(id1, id2)`  | Compare node IDs.                     | `$1 $0 d => 1`           |
| `e`      | `!cmp(curT, id)`  | Compare the type of the current node. | `"ab" e`                 |
| `f`      | `node[a]`         | Get the string value of a node field. | `"field" f`              |
| `g`      | `node[a]`         | Get the number value of a node field. | `"field" g`              |
| `h`      | `!!node[a]`       | Field exists.                         | `"title.en" h => 1`      |
| `i`      | `a <= b <= c`     | (interval) `b` is within `a` and `c`. | `"#2 #1 #0 => 1`         |
| `j`      | `findFirst(A)`    | Take the name of the first non-empty field into a new set. (value is set or set is non-empty) | `{"nonfield","field"} j => [ 'field' ]` |
| `k`      | `aon(A)`          | Take all or none (AON), pass the set or result an empty set. | `{"field1","field2"} k => [ 'field1', 'field2' ]` |
| `n`      | `clk_realtime()`  | Get the current value of `CLOCK_REALTIME` in ms. | `l => 1623253120970` |

`j` and `k` are only available if `rpn_set_hierarchy_node()` is called before
executing an expression.
