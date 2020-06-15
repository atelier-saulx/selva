Tools
=====

**hiearachy-dot-dump.js**

Dump a hierarchy object in dot format.

Dump the whole tree:

```
node hiearachy-dot-dump.js test
```

Limit the number of nodes traversed:

```
node hiearachy-dot-dump.js test 5
```

Dump the tree towards descendants starting from the node `a`:

```
node hiearachy-dot-dump.js test descendants a
```

Dump the three towards ancestors starting from the node `g`:

```
node hiearachy-dot-dump.js test ancestors g
```

Limit the number of nodes traversed:

```
node hiearachy-dot-dump.js test ancestors g 10
```
