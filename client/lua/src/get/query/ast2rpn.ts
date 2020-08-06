import { Fork, FilterAST as Filter } from './types'

function isFork(x: any): x is Fork {
    return x.isFork
}

const opMapString = {
    '=': 'c',
    '!=': 'G',
    'exists': 'h',
    'notExists': 'h',
};

const opMapNumber = {
    '=': 'F',
    '>': 'I',
    '<': 'H',
    '..': 'i',
    '!=': 'G',
    'exists': 'h',
    'notExists': 'h',
};

export default function ast2rpn(f: Fork): string[] {
  let out = '';
  let reg: string[] = [];
  let regIndex = 1;

  function ast2rpnFilter(f: Filter) {
    const fieldId = regIndex
    reg[regIndex++] = f.$field

      const vType = type(f.$value);

    if (vType == 'string' || vType == 'number') {
      const valueId = regIndex
      reg[regIndex++] = `${f.$value}`

      const op = vType == 'string' ? opMapString[f.$operator] : opMapNumber[f.$operator]
      if (!op) {
          // TODO error
      }

      if (f.$operator == 'notExists') {
        out += ` $${valueId} $${fieldId} f h L`
      } else {
        out += ` $${valueId} $${fieldId} f ${op}`
      }
    } else if (f.$operator == '..') {
      const valueId1 = regIndex
      reg[regIndex++] = `${f.$value[0]}`
      const valueId2 = regIndex
      reg[regIndex++] = `${f.$value[1]}`

      out += ` $${valueId2} $${fieldId} f ${valueId1} i`
    } else {
        // TODO error
    }
  }

  function ast2rpnFork(expr: Fork) {
      const lop: ' M'|' N' = expr.$and ? ' M' : ' N'
      const arr = expr.$and || expr.$or || []

      for (let i = 0; i < arr.length; i++) {
          const el = arr[i]

          if (isFork(el)) {
              ast2rpnFork(el);
          } else {
              ast2rpnFilter(el);
          }

          if (i > 0) {
              out += lop;
          }
      }
  }

  ast2rpnFork(f)
  return [out, ...reg]
}
