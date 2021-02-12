const a = {
  children: [
    {
      id: 'se8c01f008',
      name: 'New Sequence 0',
      index: 0,
      items: [
        {
          id: 'pacb4dff8f',
          name: 'Welcome Screen',
          index: 0,
          config: {
            action: 'modal',
            props: {
              header: {
                title: '{{data.name}}',
                icon: 'welcomeScreen',
                framed: true,
              },
            },
            content: {
              component: 'group',
              direction: 'column',
              children: [
                { component: 'Title', children: 'General' },
                {
                  component: 'group',
                  direction: 'row',
                  alignItems: 'center',
                  children: [
                    {
                      component: 'text',
                      weight: 'semibold',
                      children: 'Title',
                    },
                    {
                      component: 'input',
                      value: '{{data.components.0.text}}',
                      onChange: {
                        action: 'api',
                        endpoint: 'based',
                        method: '@based/set',
                      },
                    },
                  ],
                },
                {
                  component: 'group',
                  direction: 'row',
                  alignItems: 'center',
                  children: [
                    {
                      component: 'text',
                      weight: 'semibold',
                      children: 'Body Text',
                    },
                    {
                      component: 'input',
                      value: '{{data.components.0.text}}',
                      onChange: {
                        action: 'api',
                        endpoint: 'based',
                        method: '@based/set',
                      },
                    },
                  ],
                },
                {
                  component: 'group',
                  direction: 'row',
                  alignItems: 'center',
                  children: [
                    {
                      component: 'text',
                      weight: 'semibold',
                      children: 'james!!!',
                    },
                    {
                      component: 'input',
                      value: '{{data.components.0.text}}',
                      onChange: {
                        action: 'api',
                        endpoint: 'based',
                        method: '@based/set',
                      },
                    },
                  ],
                },
                {
                  component: 'group',
                  direction: 'row',
                  alignItems: 'center',
                  children: [
                    {
                      component: 'text',
                      weight: 'semibold',
                      children: 'james!!!',
                    },
                    {
                      component: 'input',
                      value: '{{data.components.0.text}}',
                      onChange: {
                        action: 'api',
                        endpoint: 'based',
                        method: '@based/set',
                      },
                    },
                  ],
                },
              ],
            },
          },
          components: {
            '0': { text: '1434555' },
            '1': {
              component: 'Image',
              image:
                'https://i1.wp.com/psych2go.net/wp-content/uploads/2014/08/91df642880432da28c563dfc45fa57f5.jpg?fit=640%2C400&ssl=1',
            },
            '2': {
              component: 'List',
              items: {
                '0': {
                  image:
                    'https://i1.wp.com/psych2go.net/wp-content/uploads/2014/08/91df642880432da28c563dfc45fa57f5.jpg?fit=640%2C400&ssl=1',
                  title: { text: 'Efendi' },
                },
              },
            },
            '3': { component: 'Button', text: 'Go to that page!' },
          },
          parents: ['ptwelcome', 'se8c01f008'],
        },
        {
          id: 'pae49eba1b',
          name: 'Welcome Screen',
          index: 1,
          config: {
            action: 'modal',
            props: {
              header: {
                title: '{{data.name}}',
                icon: 'welcomeScreen',
                framed: true,
              },
            },
            content: {
              component: 'group',
              direction: 'column',
              children: [
                { component: 'Title', children: 'General' },
                {
                  component: 'group',
                  direction: 'row',
                  alignItems: 'center',
                  children: [
                    {
                      component: 'text',
                      weight: 'semibold',
                      children: 'Title',
                    },
                    {
                      component: 'input',
                      value: '{{data.components.0.text}}',
                      onChange: {
                        action: 'api',
                        endpoint: 'based',
                        method: '@based/set',
                      },
                    },
                  ],
                },
                {
                  component: 'group',
                  direction: 'row',
                  alignItems: 'center',
                  children: [
                    {
                      component: 'text',
                      weight: 'semibold',
                      children: 'Body Text',
                    },
                    {
                      component: 'input',
                      value: '{{data.components.0.text}}',
                      onChange: {
                        action: 'api',
                        endpoint: 'based',
                        method: '@based/set',
                      },
                    },
                  ],
                },
                {
                  component: 'group',
                  direction: 'row',
                  alignItems: 'center',
                  children: [
                    {
                      component: 'text',
                      weight: 'semibold',
                      children: 'james!!!',
                    },
                    {
                      component: 'input',
                      value: '{{data.components.0.text}}',
                      onChange: {
                        action: 'api',
                        endpoint: 'based',
                        method: '@based/set',
                      },
                    },
                  ],
                },
                {
                  component: 'group',
                  direction: 'row',
                  alignItems: 'center',
                  children: [
                    {
                      component: 'text',
                      weight: 'semibold',
                      children: 'james!!!',
                    },
                    {
                      component: 'input',
                      value: '{{data.components.0.text}}',
                      onChange: {
                        action: 'api',
                        endpoint: 'based',
                        method: '@based/set',
                      },
                    },
                  ],
                },
              ],
            },
          },
          components: {
            '0': { text: 'Voting round 1?ku' },
            '1': {
              component: 'Image',
              image:
                'https://i1.wp.com/psych2go.net/wp-content/uploads/2014/08/91df642880432da28c563dfc45fa57f5.jpg?fit=640%2C400&ssl=1',
            },
            '2': {
              component: 'List',
              items: {
                '0': {
                  image:
                    'https://i1.wp.com/psych2go.net/wp-content/uploads/2014/08/91df642880432da28c563dfc45fa57f5.jpg?fit=640%2C400&ssl=1',
                  title: { text: 'Efendi' },
                },
              },
            },
            '3': { component: 'Button', text: 'Go to that page!' },
          },
          parents: ['ptwelcome', 'se8c01f008'],
        },
      ],
    },
  ],
}

const b = {
  children: [
    {
      id: 'se8c01f008',
      name: 'New Sequence 0',
      index: 0,
      items: [
        {
          id: 'pacb4dff8f',
          name: 'Welcome Screen',
          index: 0,
          config: {
            action: 'modal',
            props: {
              header: {
                title: '{{data.name}}',
                icon: 'welcomeScreen',
                framed: true,
              },
            },
            content: {
              component: 'group',
              direction: 'column',
              children: [
                { component: 'Title', children: 'General' },
                {
                  component: 'group',
                  direction: 'row',
                  alignItems: 'center',
                  children: [
                    {
                      component: 'text',
                      weight: 'semibold',
                      children: 'Title',
                    },
                    {
                      component: 'input',
                      value: '{{data.components.0.text}}',
                      onChange: {
                        action: 'api',
                        endpoint: 'based',
                        method: '@based/set',
                      },
                    },
                  ],
                },
                {
                  component: 'group',
                  direction: 'row',
                  alignItems: 'center',
                  children: [
                    {
                      component: 'text',
                      weight: 'semibold',
                      children: 'Body Text',
                    },
                    {
                      component: 'input',
                      value: '{{data.components.0.text}}',
                      onChange: {
                        action: 'api',
                        endpoint: 'based',
                        method: '@based/set',
                      },
                    },
                  ],
                },
                {
                  component: 'group',
                  direction: 'row',
                  alignItems: 'center',
                  children: [
                    {
                      component: 'text',
                      weight: 'semibold',
                      children: 'james!!!',
                    },
                    {
                      component: 'input',
                      value: '{{data.components.0.text}}',
                      onChange: {
                        action: 'api',
                        endpoint: 'based',
                        method: '@based/set',
                      },
                    },
                  ],
                },
                {
                  component: 'group',
                  direction: 'row',
                  alignItems: 'center',
                  children: [
                    {
                      component: 'text',
                      weight: 'semibold',
                      children: 'she hulk',
                    },
                    {
                      component: 'input',
                      value: '{{data.components.0.text}}',
                      onChange: {
                        action: 'api',
                        endpoint: 'based',
                        method: '@based/set',
                      },
                    },
                  ],
                },
              ],
            },
          },
          components: {
            '0': { text: '1434555' },
            '1': {
              component: 'Image',
              image:
                'https://i1.wp.com/psych2go.net/wp-content/uploads/2014/08/91df642880432da28c563dfc45fa57f5.jpg?fit=640%2C400&ssl=1',
            },
            '2': {
              component: 'List',
              items: {
                '0': {
                  image:
                    'https://i1.wp.com/psych2go.net/wp-content/uploads/2014/08/91df642880432da28c563dfc45fa57f5.jpg?fit=640%2C400&ssl=1',
                  title: { text: 'Efendi' },
                },
              },
            },
            '3': { component: 'Button', text: 'Go to that page!' },
          },
          parents: ['ptwelcome', 'se8c01f008'],
        },
        {
          id: 'pae49eba1b',
          name: 'Welcome Screen',
          index: 1,
          config: {
            action: 'modal',
            props: {
              header: {
                title: '{{data.name}}',
                icon: 'welcomeScreen',
                framed: true,
              },
            },
            content: {
              component: 'group',
              direction: 'column',
              children: [
                { component: 'Title', children: 'General' },
                {
                  component: 'group',
                  direction: 'row',
                  alignItems: 'center',
                  children: [
                    {
                      component: 'text',
                      weight: 'semibold',
                      children: 'Title',
                    },
                    {
                      component: 'input',
                      value: '{{data.components.0.text}}',
                      onChange: {
                        action: 'api',
                        endpoint: 'based',
                        method: '@based/set',
                      },
                    },
                  ],
                },
                {
                  component: 'group',
                  direction: 'row',
                  alignItems: 'center',
                  children: [
                    {
                      component: 'text',
                      weight: 'semibold',
                      children: 'Body Text',
                    },
                    {
                      component: 'input',
                      value: '{{data.components.0.text}}',
                      onChange: {
                        action: 'api',
                        endpoint: 'based',
                        method: '@based/set',
                      },
                    },
                  ],
                },
                {
                  component: 'group',
                  direction: 'row',
                  alignItems: 'center',
                  children: [
                    {
                      component: 'text',
                      weight: 'semibold',
                      children: 'james!!!',
                    },
                    {
                      component: 'input',
                      value: '{{data.components.0.text}}',
                      onChange: {
                        action: 'api',
                        endpoint: 'based',
                        method: '@based/set',
                      },
                    },
                  ],
                },
                {
                  component: 'group',
                  direction: 'row',
                  alignItems: 'center',
                  children: [
                    {
                      component: 'text',
                      weight: 'semibold',
                      children: 'she hulk',
                    },
                    {
                      component: 'input',
                      value: '{{data.components.0.text}}',
                      onChange: {
                        action: 'api',
                        endpoint: 'based',
                        method: '@based/set',
                      },
                    },
                  ],
                },
              ],
            },
          },
          components: {
            '0': { text: 'Voting round 1?ku' },
            '1': {
              component: 'Image',
              image:
                'https://i1.wp.com/psych2go.net/wp-content/uploads/2014/08/91df642880432da28c563dfc45fa57f5.jpg?fit=640%2C400&ssl=1',
            },
            '2': {
              component: 'List',
              items: {
                '0': {
                  image:
                    'https://i1.wp.com/psych2go.net/wp-content/uploads/2014/08/91df642880432da28c563dfc45fa57f5.jpg?fit=640%2C400&ssl=1',
                  title: { text: 'Efendi' },
                },
              },
            },
            '3': { component: 'Button', text: 'Go to that page!' },
          },
          parents: ['ptwelcome', 'se8c01f008'],
        },
      ],
    },
  ],
}

export { a, b }
