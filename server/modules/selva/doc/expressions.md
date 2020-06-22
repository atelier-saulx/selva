RPN Boolean Expression Language
===============================

The following query finds all descendants of a node called `head` that are of
type `2X`.

```
SELVA.HIERARCHY.find test descendants "head\x00\x00\x00\x00\x00\x00" '#0 #1 @ b "2X d'
```

Breaking down the filter:

```
#0      [operand 1] is a register index. The current nodeId is stored in the register 0.
#1      [operand 0] is a type specifier (string).
@       [function]  Reads the register taking two previous operands.
b       [function]  Extracts the type string from the previous result.
"2X     [operand 0] Is a string representing a node type.
d       [function]  Compares operand 0 with the result of the previous function.
```


Syntax
------


**Number**

Numbers are prefixed with `#`.

**Strings**

A string starts with a `"` character.

Strings cannot be quoted and it's advisable to place strings in the registers
given as arguments to the expression parser.


**Arithmetic operators**

| Operator | Operands           | Description                       | Syntax                    |
|----------|--------------------|-----------------------------------|---------------------------|
| `A`      | `a + b`            | Addition operator.                | `1 2 A => 3`              |
| `B`      | `a - b`            | Subtraction operator.             | `1 2 B => 1`              |
| `C`      | `a / b`            | Division operator.                | `2 4 C => 2`              |
| `D`      | `a * b`            | Multiplication operator.          | `2 2 D => 4`              |
| `E`      | `a % b`            | Remainder operator.               | `4 9 E => 1`              |

**Relational operators**

| Operator | Operands           | Description                       | Example (expr => result)  |
|----------|--------------------|-----------------------------------|---------------------------|
| `F`      | `a == b`           | Equality operator.                | `1 1 F => 1`              |
| `G`      | `a != b`           | Not equal operator.               | `1 2 G => 1`              |
| `H`      | `a < b`            | Less than operator.               | `2 1 H => 1`              |
| `I`      | `a > b`            | Greater than operator.            | `2 1 I => 0`              |
| `J`      | `a <= b`           | Less than or equal operator.      | `2 1 J => 1`              |
| `K`      | `a >= b`           | Greater than or equal operator.   | `2 1 K => 0`              |

**Logical operators**

| Operator | Operands           | Description                       | Example (expr => result)  |
|----------|--------------------|-----------------------------------|---------------------------|
| `L`      | `!a`               | Logical NOT operator. (unary)     | `1 L => 0`                |
| `M`      | `a AND b`          | Logical AND operator.             | `1 1 M => 1`              |
| `N`      | `a OR b`           | Logical OR operator.              | `0 1 N => 1`              |
| `O`      | `!!a XOR !!b`      | Logical XOR operator.             | `1 1 O => 0`              |

**Functions**

| Operator | Arguments          | Description                       | Example (expr => result)  |
|----------|--------------------|-----------------------------------|---------------------------|
| `@`      | `(type)reg[a]`     | Read the value of register `a`. 1)| `[index] [type] @`        |
| `a`      | `a in b`           | `in` function.                    | `0 @ 1 @ a => 0`          |
| `b`      | `id`               | Returns the type of a node id.    | `xy123 b => xy`           |
| `c`      | `!strcmp(s1, s2)`  | Compare strings.                  | `0 @ hello c => 1`        |
| `d`      | `!cmp(id1, id2)`   | Compare node IDs.                 | `0 @ 1 @ d => 1`          | 
| `e`      | `!cmp(curT, id)`   | Compare the type of the current node. | `"AB e`               |

1) `@` function takes a `type` argument that selects whether the register is
   read as a string or integer. 0 is integer; 1 is string.
