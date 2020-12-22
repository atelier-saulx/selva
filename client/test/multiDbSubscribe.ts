import test from 'ava'
import { connect } from '../src/index'
import { start, startOrigin } from '@saulx/selva-server'
import { wait } from './assertions'
import getPort from 'get-port'

let srv
let srv2
let port: number
test.before(async t => {
  port = await getPort()
  srv = await start({ port })

  srv2 = await startOrigin({ name: 'users', registry: { port } })

  const client = connect({ port })
  await client.updateSchema({
    languages: ['en', 'de'], //, 'nl', 'ru', 'sr', 'pl', 'fr', 'es'],
    types: {
      show: {
        prefix: 'sh',
        fields: {
          title: {
            type: 'text'
          }
          // logo: {
          //   type: 'url'
          // },
          // updatedAt: {
          //   type: 'timestamp'
          // }
        }
      }
      // edition: {
      //   prefix: 'ed',
      //   fields: {
      //     companyName: {
      //       type: 'string'
      //     },
      //     title: {
      //       type: 'text'
      //     },
      //     logo: {
      //       type: 'url'
      //     },
      //     ogTitle: {
      //       type: 'text'
      //     },
      //     ogDescription: {
      //       type: 'text'
      //     },
      //     ogImage: {
      //       type: 'url'
      //     },
      //     startTime: {
      //       type: 'timestamp'
      //     },
      //     acceptance: {
      //       type: 'object',
      //       properties: {
      //         terms: {
      //           type: 'object',
      //           properties: {
      //             enabled: {
      //               type: 'boolean'
      //             },
      //             url: {
      //               type: 'url'
      //             }
      //           }
      //         },
      //         privacy: {
      //           type: 'object',
      //           properties: {
      //             enabled: {
      //               type: 'boolean'
      //             },
      //             url: {
      //               type: 'url'
      //             }
      //           }
      //         },
      //         imprint: {
      //           type: 'object',
      //           properties: {
      //             enabled: {
      //               type: 'boolean'
      //             },
      //             url: {
      //               type: 'url'
      //             }
      //           }
      //         }
      //       }
      //     },

      //     config: {
      //       type: 'object',
      //       properties: {
      //         favicon: {
      //           type: 'url'
      //         },
      //         logo: {
      //           type: 'url'
      //         },
      //         logoLink: {
      //           type: 'url'
      //         },
      //         logoEnabled: {
      //           type: 'boolean'
      //         },
      //         borderRadius: {
      //           type: 'number'
      //         },
      //         borderWidth: {
      //           type: 'number'
      //         },
      //         roundId: {
      //           type: 'number'
      //         }
      //       }
      //     },

      //     theme: {
      //       type: 'object',
      //       properties: {
      //         highlight: {
      //           type: 'string'
      //         },
      //         background: {
      //           type: 'string'
      //         },
      //         backgroundImage: {
      //           type: 'url'
      //         },
      //         text: {
      //           type: 'string'
      //         },
      //         buttonText: {
      //           type: 'string'
      //         },
      //         itemText: {
      //           type: 'string'
      //         },
      //         itemBackground: {
      //           type: 'string'
      //         },
      //         barBackground: {
      //           type: 'string'
      //         }
      //       }
      //     },
      //     start: {
      //       type: 'timestamp',
      //       search: { type: ['NUMERIC', 'SORTABLE'] }
      //     },
      //     updatedAt: {
      //       type: 'timestamp'
      //     }
      //   }
      // },
      // welcomeScreen: {
      //   prefix: 'we',
      //   fields: {
      //     disabled: {
      //       type: 'boolean'
      //     },
      //     alias: {
      //       type: 'string'
      //     },
      //     title: {
      //       type: 'text'
      //     },
      //     description: {
      //       type: 'text'
      //     },
      //     buttonText: {
      //       type: 'text'
      //     },
      //     image: {
      //       type: 'url'
      //     },
      //     imageEnabled: {
      //       type: 'boolean'
      //     },
      //     index: {
      //       type: 'int',
      //       search: { type: ['NUMERIC', 'SORTABLE'] }
      //     },
      //     updatedAt: {
      //       type: 'timestamp'
      //     }
      //   }
      // },
      // verifyEmail: {
      //   prefix: 've',
      //   fields: {
      //     disabled: {
      //       type: 'boolean'
      //     },
      //     alias: {
      //       type: 'string'
      //     },
      //     title: {
      //       type: 'text'
      //     },
      //     description: {
      //       type: 'text'
      //     },
      //     buttonText: {
      //       type: 'text'
      //     },
      //     image: {
      //       type: 'url'
      //     },
      //     imageEnabled: {
      //       type: 'boolean'
      //     },
      //     index: {
      //       type: 'int',
      //       search: { type: ['NUMERIC', 'SORTABLE'] }
      //     },
      //     updatedAt: {
      //       type: 'timestamp'
      //     }
      //   }
      // },
      // overviewScreen: {
      //   prefix: 'ov',
      //   fields: {
      //     disabled: {
      //       type: 'boolean'
      //     },
      //     alias: {
      //       type: 'string'
      //     },
      //     title: {
      //       type: 'text'
      //     },
      //     description: {
      //       type: 'text'
      //     },
      //     buttonText: {
      //       type: 'text'
      //     },
      //     index: {
      //       type: 'int',
      //       search: { type: ['NUMERIC', 'SORTABLE'] }
      //     },
      //     image: {
      //       type: 'url'
      //     },
      //     imageEnabled: {
      //       type: 'boolean'
      //     },
      //     updatedAt: {
      //       type: 'timestamp'
      //     }
      //   }
      // },
      // videoScreen: {
      //   prefix: 'vi',
      //   fields: {
      //     disabled: {
      //       type: 'boolean'
      //     },
      //     alias: {
      //       type: 'string'
      //     },
      //     title: {
      //       type: 'text'
      //     },
      //     description: {
      //       type: 'text'
      //     },
      //     buttonText: {
      //       type: 'text'
      //     },
      //     image: {
      //       type: 'url'
      //     },
      //     video: {
      //       type: 'url'
      //     },
      //     videoMandatory: {
      //       type: 'boolean'
      //     },
      //     index: {
      //       type: 'int',
      //       search: { type: ['NUMERIC', 'SORTABLE'] }
      //     },
      //     updatedAt: {
      //       type: 'timestamp'
      //     }
      //   }
      // },
      // multipleChoice: {
      //   prefix: 'mu',
      //   fields: {
      //     disabled: {
      //       type: 'boolean'
      //     },
      //     alias: {
      //       type: 'string'
      //     },
      //     title: {
      //       // this is question
      //       type: 'text'
      //     },
      //     description: {
      //       type: 'text'
      //     },
      //     buttonText: {
      //       type: 'text'
      //     },
      //     image: {
      //       type: 'url'
      //     },
      //     imageEnabled: {
      //       type: 'boolean'
      //     },
      //     index: {
      //       type: 'int',
      //       search: { type: ['NUMERIC', 'SORTABLE'] }
      //     },
      //     settings: {
      //       type: 'object',
      //       properties: {
      //         rangeMin: {
      //           type: 'int'
      //         },
      //         rangeMax: {
      //           type: 'int'
      //         }
      //       }
      //     }
      //   }
      // },
      // openQuestion: {
      //   prefix: 'oq',
      //   fields: {
      //     disabled: {
      //       type: 'boolean'
      //     },
      //     alias: {
      //       type: 'string'
      //     },
      //     title: {
      //       // this is question
      //       type: 'text'
      //     },
      //     description: {
      //       type: 'text'
      //     },
      //     image: {
      //       type: 'url'
      //     },
      //     imageEnabled: {
      //       type: 'boolean'
      //     },
      //     buttonText: {
      //       type: 'text'
      //     },
      //     index: {
      //       type: 'int',
      //       search: { type: ['NUMERIC', 'SORTABLE'] }
      //     },
      //     settings: {
      //       type: 'object',
      //       properties: {
      //         maxChars: {
      //           type: 'int'
      //         }
      //       }
      //     }
      //   }
      // },
      // scale: {
      //   prefix: 'sc',
      //   fields: {
      //     disabled: {
      //       type: 'boolean'
      //     },
      //     alias: {
      //       type: 'string'
      //     },
      //     title: {
      //       // this is question
      //       type: 'text'
      //     },
      //     description: {
      //       type: 'text'
      //     },
      //     logo: {
      //       type: 'url'
      //     },
      //     image: {
      //       type: 'url'
      //     },
      //     imageEnabled: {
      //       type: 'boolean'
      //     },
      //     buttonText: {
      //       type: 'text'
      //     },
      //     index: {
      //       type: 'int',
      //       search: { type: ['NUMERIC', 'SORTABLE'] }
      //     }
      //   }
      // },
      // waitingScreen: {
      //   prefix: 'wa',
      //   fields: {
      //     disabled: {
      //       type: 'boolean'
      //     },
      //     alias: {
      //       type: 'string'
      //     },
      //     title: {
      //       type: 'text'
      //     },
      //     titleAlt: {
      //       type: 'text'
      //     },
      //     description: {
      //       type: 'text'
      //     },
      //     buttonText: {
      //       type: 'text'
      //     },
      //     image: {
      //       type: 'url'
      //     },
      //     imageEnabled: {
      //       type: 'boolean'
      //     },
      //     index: {
      //       type: 'int',
      //       search: { type: ['NUMERIC', 'SORTABLE'] }
      //     }
      //   }
      // },
      // thankYouScreen: {
      //   prefix: 'th',
      //   fields: {
      //     disabled: {
      //       type: 'boolean'
      //     },
      //     alias: {
      //       type: 'string'
      //     },
      //     title: {
      //       type: 'text'
      //     },
      //     description: {
      //       type: 'text'
      //     },
      //     image: {
      //       type: 'url'
      //     },
      //     imageEnabled: {
      //       type: 'boolean'
      //     },
      //     index: {
      //       type: 'int',
      //       search: { type: ['NUMERIC', 'SORTABLE'] }
      //     }
      //   }
      // },
      // registerScreen: {
      //   prefix: 're',
      //   fields: {
      //     disabled: {
      //       type: 'boolean'
      //     },
      //     alias: {
      //       type: 'string'
      //     },
      //     title: {
      //       type: 'text'
      //     },
      //     description: {
      //       type: 'text'
      //     },
      //     buttonText: {
      //       type: 'text'
      //     },
      //     index: {
      //       type: 'int',
      //       search: { type: ['NUMERIC', 'SORTABLE'] }
      //     },
      //     fields: {
      //       type: 'object',
      //       properties: {
      //         email: {
      //           type: 'object',
      //           properties: {
      //             enabled: {
      //               type: 'boolean'
      //             },
      //             required: {
      //               type: 'boolean'
      //             },
      //             verify: {
      //               type: 'boolean'
      //             }
      //           }
      //         },
      //         phone: {
      //           type: 'object',
      //           properties: {
      //             enabled: {
      //               type: 'boolean'
      //             },
      //             required: {
      //               type: 'boolean'
      //             }
      //           }
      //         },
      //         name: {
      //           type: 'object',
      //           properties: {
      //             enabled: {
      //               type: 'boolean'
      //             },
      //             required: {
      //               type: 'boolean'
      //             }
      //           }
      //         },
      //         birthday: {
      //           type: 'object',
      //           properties: {
      //             enabled: {
      //               type: 'boolean'
      //             },
      //             required: {
      //               type: 'boolean'
      //             }
      //           }
      //         },
      //         address: {
      //           type: 'object',
      //           properties: {
      //             enabled: {
      //               type: 'boolean'
      //             },
      //             required: {
      //               type: 'boolean'
      //             }
      //           }
      //         }
      //       }
      //     },
      //     acceptance: {
      //       type: 'object',
      //       properties: {
      //         terms: {
      //           type: 'object',
      //           properties: {
      //             enabled: {
      //               type: 'boolean'
      //             },
      //             url: {
      //               type: 'url'
      //             }
      //           }
      //         },
      //         privacy: {
      //           type: 'object',
      //           properties: {
      //             enabled: {
      //               type: 'boolean'
      //             },
      //             url: {
      //               type: 'url'
      //             }
      //           }
      //         }
      //       }
      //     },
      //     optIn: {
      //       type: 'object',
      //       properties: {
      //         enabled: {
      //           type: 'boolean'
      //         },
      //         title: {
      //           type: 'text'
      //         },
      //         fields: {
      //           type: 'object',
      //           properties: {
      //             email: {
      //               type: 'object',
      //               properties: {
      //                 enabled: {
      //                   type: 'boolean'
      //                 },
      //                 required: {
      //                   type: 'boolean'
      //                 },
      //                 verify: {
      //                   type: 'boolean'
      //                 }
      //               }
      //             },
      //             phone: {
      //               type: 'object',
      //               properties: {
      //                 enabled: {
      //                   type: 'boolean'
      //                 },
      //                 required: {
      //                   type: 'boolean'
      //                 }
      //               }
      //             },
      //             name: {
      //               type: 'object',
      //               properties: {
      //                 enabled: {
      //                   type: 'boolean'
      //                 },
      //                 required: {
      //                   type: 'boolean'
      //                 }
      //               }
      //             },
      //             birthday: {
      //               type: 'object',
      //               properties: {
      //                 enabled: {
      //                   type: 'boolean'
      //                 },
      //                 required: {
      //                   type: 'boolean'
      //                 }
      //               }
      //             },
      //             address: {
      //               type: 'object',
      //               properties: {
      //                 enabled: {
      //                   type: 'boolean'
      //                 },
      //                 required: {
      //                   type: 'boolean'
      //                 }
      //               }
      //             }
      //           }
      //         }
      //       }
      //     }
      //   }
      // },
      // option: {
      //   prefix: 'op',
      //   fields: {
      //     alias: {
      //       type: 'string'
      //     },
      //     ddi: {
      //       type: 'int'
      //     },
      //     title: {
      //       type: 'text'
      //     },
      //     value: {
      //       type: 'number'
      //     },
      //     subtitle: {
      //       type: 'text'
      //     },
      //     caption: {
      //       type: 'text'
      //     },
      //     description: {
      //       type: 'text'
      //     },
      //     image: {
      //       type: 'url'
      //     },
      //     video: {
      //       type: 'url'
      //     },
      //     ogTitle: {
      //       type: 'text'
      //     },
      //     ogDescription: {
      //       type: 'text'
      //     },
      //     ogImage: {
      //       type: 'url'
      //     },
      //     index: {
      //       type: 'int',
      //       search: { type: ['NUMERIC', 'SORTABLE'] }
      //     }
      //   }
      // }
    }
  })

  await client.updateSchema(
    {
      languages: ['en', 'de', 'nl'],
      types: {
        user: {
          prefix: 'us',
          fields: {
            title: { type: 'text' }
          }
        }
      }
    },
    'users'
  )

  await wait(500)

  await client.destroy()
})

test.after(async t => {
  const client = connect({ port })
  await client.delete('root')
  await client.destroy()
  await srv.destroy()
  await srv2.destroy()
  await t.connectionsAreEmpty()
})

test.serial('subscribe - should fire after creation', async t => {
  const client = connect({ port }, { loglevel: 'info' })
  const id = 'sh1'

  // await client.set({
  //   $id: id,
  //   $language: 'de',
  //   title: 'IF THIS IS UNCOMMENTED IT WORKS'
  // })

  let n = 5
  t.plan(n + 1)

  client
    .observe({
      $id: id,
      title: true,
      $language: 'de'
    })
    .subscribe(r => {
      console.log('fires!', r)
      t.pass()
    })

  await wait(2e3)

  while (n--) {
    console.log('setting it!', id)
    await client.set({
      $id: id,
      $language: 'de',
      title: 'test ' + n
    })
    await wait(2e3)
  }

  await client.destroy()
})
