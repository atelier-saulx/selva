export default (target) => [
  {
    $language: 'de',
    $id: target,
    id: true,
    children: {
      startTime: true,
      index: true,
      id: true,
      type: true,
      name: true,
      items: {
        disabled: true,
        type: true,
        id: true,
        name: true,
        index: true,
        icon: { $inherit: true },
        parents: true,
        $list: {
          $sort: { $field: 'index', $order: 'asc' },
          $find: {
            $traverse: 'children',
          },
        },
      },
      $list: {
        $sort: { $field: 'index', $order: 'asc' },
        $find: {
          $filter: {
            $field: 'type',
            $operator: '=',
            $value: 'sequence',
          },
        },
      },
    },
  },
  {
    $id: target,
    pastSequences: {
      id: true,
      name: true,
      startTime: true,
      $list: {
        $sort: { $field: 'startTime', $order: 'desc' },
        $find: {
          $traverse: 'children',
          $filter: [
            {
              $field: 'type',
              $operator: '=',
              $value: 'sequence',
            },
            {
              $field: 'startTime',
              $operator: '<',
              $value: 'now',
            },
          ],
        },
      },
    },
  },
  {
    $language: 'de',
    $id: target,
    id: true,
    children: {
      index: true,
      id: true,
      type: true,
      name: true,
      startTime: true,
      items: {
        type: true,
        id: true,
        name: true,
        index: true,
        icon: { $inherit: true },
        $list: {
          $sort: { $field: 'index', $order: 'asc' },
          $find: {
            $traverse: 'children',
          },
        },
      },
      $list: {
        $sort: { $field: 'index', $order: 'asc' },
        $find: {
          $filter: {
            $field: 'type',
            $operator: '=',
            $value: 'sequence',
          },
        },
      },
    },
  },
  {
    $language: 'de',
    $id: target,
    languages: true,
    id: true,
    children: {
      startTime: true,
      index: true,
      id: true,
      type: true,
      name: true,
      items: {
        disabled: true,
        type: true,
        id: true,
        name: true,
        index: true,
        icon: { $inherit: true },
        parents: true,
        $list: {
          $sort: { $field: 'index', $order: 'asc' },
          $find: {
            $traverse: 'children',
          },
        },
      },
      $list: {
        $sort: { $field: 'index', $order: 'asc' },
        $find: {
          $filter: {
            $field: 'type',
            $operator: '=',
            $value: 'sequence',
          },
        },
      },
    },
  },
  {
    $language: 'de',
    $id: target,
    id: true,
    children: {
      index: true,
      id: true,
      type: true,
      name: true,
      startTime: true,
      items: {
        id: true,
        name: true,
        index: true,
        icon: { $inherit: true },
        parents: true,
        $list: {
          $sort: { $field: 'index', $order: 'asc' },
          $find: {
            $traverse: 'children',
            $filter: {
              $field: 'type',
              $operator: '=',
              $value: 'page',
            },
          },
        },
      },
      $list: {
        $sort: { $field: 'index', $order: 'asc' },
        $find: {
          $filter: {
            $field: 'type',
            $operator: '=',
            $value: 'sequence',
          },
        },
      },
    },
  },
  {
    $id: target,
    $language: 'de',
    children: {
      // sequences
      index: true,
      name: true,
      id: true,
      $list: { $sort: { $field: 'index', $order: 'asc' } },
      children: {
        // pages
        id: true,
        index: true,
        name: true,
        $list: { $sort: { $field: 'index', $order: 'asc' } },
        children: {
          // items
          id: true,
          index: true,
          title: true,
          body: true,
          $list: { $sort: { $field: 'index', $order: 'asc' } },
        },
      },
    },
  },
  {
    $id: target,
    $language: 'en',
    children: {
      $list: {
        $sort: { $field: 'index', $order: 'asc' },
      },
      index: true,
      children: {
        $list: {
          $sort: { $field: 'index', $order: 'asc' },
        },
        id: true,
        index: true,
        name: true,
      },
    },
  },
]
