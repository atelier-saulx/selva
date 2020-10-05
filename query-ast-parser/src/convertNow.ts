export default function convertNow(x: string): number {
  const unitLetters = {
    s: true,
    m: true,
    h: true,
    d: true
  }

  if (!x.startsWith('now')) {
    return 0
  }

  if (x.length === 3) {
    // just 'now'
    return Date.now()
  }

  const op = x[3]
  if (op === '+' || op === '-') {
    let offsetStr = x.substr(3)
    let unit = offsetStr[offsetStr.length - 1]
    if (!unitLetters[unit]) {
      offsetStr += unit
    } else {
      offsetStr = offsetStr.substr(0, offsetStr.length - 1)
    }

    let offset = Number(offsetStr)
    if (!offset) {
      return 0
    }

    // convert unit to ms
    if (unit === 's') {
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

    return Date.now() + offset
  } else {
    return 0
  }
}
