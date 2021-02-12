import { Fork, FilterAST } from './types'
import isFork from './isFork'

// use chalk maybe...
const colors =
  typeof window === 'undefined'
    ? {
        red: '\u001b[31m',
        white: '\u001b[37;1m',
        blue: '\u001b[34m',
        reset: '\u001b[0m',
      }
    : {
        red: '',
        white: '',
        blue: '',
        reset: '',
      }

const addIndent = (number: number, last?: boolean): string => {
  let str = colors.reset
  for (let i = 0; i < number; i++) {
    if (i === number - 1) {
      // if last └
      str += last ? ' └-' : ' ├-'
    } else {
      str += '   '
    }
  }
  return str
}

const log = (str: string, number: number, last?: boolean) => {
  console.info(`${colors.white}${addIndent(number, last)}${str}`)
}

const logFilter = (filter: FilterAST, number: number, last?: boolean) => {
  const str = `${filter.$field} ${filter.$operator} ${
    filter.$value === undefined
      ? ''
      : Array.isArray(filter.$value)
      ? `[${filter.$value.join(',')}]`
      : filter.$value
  }`
  log(str, number, last)
}

const forkLogger = (fork: Fork, indent: number = 0, last?: boolean) => {
  let target: (FilterAST | Fork)[]
  const forks: Fork[] = []

  if (fork.$and) {
    target = fork.$and
    log(`${colors.blue}$and${colors.reset}`, indent, last)
  } else if (fork.$or) {
    target = fork.$or
    log(`${colors.blue}$or${colors.reset}`, indent, last)
  } else {
    log(`${colors.red}no $or or $and in fork${colors.reset}`, indent, last)
    return
  }

  if (target.length === 0) {
    log(`${colors.red}empty fork${colors.reset}`, indent + 1, true)
  }

  for (let i = 0; i < target.length; i++) {
    const entry = target[i]
    if (isFork(entry)) {
      forks[forks.length] = entry
    } else {
      logFilter(
        entry,
        indent + 1,
        forks.length === 0 && i === target.length - 1
      )
    }
  }

  for (let i = 0; i < forks.length; i++) {
    forkLogger(forks[i], indent + 1, i === forks.length - 1)
  }
}

export default function (fork: Fork | void, ...args: any[]) {
  if (args.length) {
    console.info(...args)
  }
  if (!fork) {
    console.info('No ast passed to printAst')
    return
  }

  forkLogger(fork)
  console.info(`${colors.reset}`)
}
