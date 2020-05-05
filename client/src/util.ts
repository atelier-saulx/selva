export const wait = (t: number = 0): Promise<void> =>
  new Promise(r => setTimeout(r, t))
