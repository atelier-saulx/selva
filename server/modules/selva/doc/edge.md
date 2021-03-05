# Edge

Edge is the part that makes Selva a true graph database, as it allows making
graph connections between any nodes using customizable edge field names and
constraints.

The following drawing shows some examples of customized edge fields. From the
user's perspective all the graph fields can be treated almost the same way. The
difference comes from the constraints applied to each field. The hierarchical
fields `parents` and `children` have a set of hard-coded rules that are applied
to them, where as the constraints (or behavior) of the custom edge fields can
be selected when each field and its first edge is created.

```
        +--------------------+
        |  id = root         |<------+----------------------------+
        |                    |       |                            |
        | parents            |       |                            | 
        |           children +-----+---------------------+--------------------------+
  +---->|                    |     | |                   |        |                 |
  |     +--------------------+     | |                   |        |                 |
  |     |        {           |     | |                   |        |                 |
  |     |         my:        |     | |                   |        |                 |
  |     |           {        |     | |                   |        |                 |
  |     |             custom |     | |                   |        |                 |
  |     |           }        |     | |                   |        |                 |
  |     |         others     |     | |                   |        |                 |
  |     |        }           |     | |                   |        |                 |
  |     +--------------------+     | |                   |        |                 |
  |                                | |                   |        |                 |
  |                      +---------+ |                   |        |                 |
  |                      v           |                   v        |                 v
  |     +--------------------+       |   +--------------------+   |      +--------------------+
  |     |  id = atnode1      |       |   |  id = btnode2      |   |      |  id = atnode3      |
  |     |                    |       |   |                    |   |      |                    |
  +-----+ parents            |       +---+ parents            |   +------+ parents            |
        |           children |           |           children |          |           children |
        |                    |     +---->|                    |     +--->|                    |
        +--------------------+     |     +--------------------+     |    +--------------------+
        |       {            |     |     |     {              |     |    |        {           |
        |         my: {      |     |     |       my: {        |     |    |          my: {     |
        |           abc: []  |-----+     |         custom: [] |     |    |            abc: [] |
        |         }          |           |       }            |     |    |          }         |
        |         edges: []  |-----+     |       edges: []    |     |    |          edges: [] |
        |        }           |     |     |     }              |     |    |        }           |
        +--------------------+     |     +--------------------+     |    +--------------------+
                                   |                                |
                                   +--------------------------------+
```
