const unitLetters = {
  s: true,
  m: true,
  h: true,
  d: true,
}

function offset2msec(offset: number, unit: null|string) {
   if (!unit) {
     return offset
   } else if (unit === 's') {
     offset *= 1000
   } else if (unit === 'm') {
     offset *= 1000
     offset *= 60
   } else if (unit === 'h') {
     offset *= 1000
     offset *= 60
     offset *= 60
   } else if (unit === 'd') {
     offset *= 1000
     offset *= 60
     offset *= 60
     offset *= 24
   }

   return offset
}

export default function convertNow(x: string, now?: number): number {
  if (!x.startsWith('now')) {
    return 0
  }

  if (x.length === 3) {
    // just 'now'
    return now || Date.now()
  }

  const op = x.substring(3).trim()[0]
  if (op === '+' || op === '-') {
    let offsetStr = x.substring(3).replace(/ /g,'')
    let unit = offsetStr[offsetStr.length - 1]
    if (unitLetters[unit]) {
      offsetStr = offsetStr.substring(0, offsetStr.length - 1)
    } else {
      unit = null
    }

    let offset = Number(offsetStr)
    if (!offset) {
      return now || Date.now()
    }

    return (now || Date.now()) + offset2msec(offset, unit)
  } else {
    return 0
  }
}

export function convertNowFilter(x: string): string {
  if (!x.startsWith('now')) {
    return '#0'
  }

  if (x.length === 3) {
    return 'n'
  }

  const op = x.substring(3).trim()[0]
  if (op === '+' || op === '-') {
    let offsetStr = x.substring(3).replace(/ /g,'')
    let unit = offsetStr[offsetStr.length - 1]
    if (unitLetters[unit]) {
      offsetStr = offsetStr.substring(0, offsetStr.length - 1)
    } else {
      unit = null
    }

    let offset = Number(offsetStr)

    return offset ? `#${offset2msec(offset, unit)} n A` : 'n'
  } else {
    return '#0'
  }
}
