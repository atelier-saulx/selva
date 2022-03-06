export let SCRIPTS: Record<string, string> = {
  // THIS PORTION IS AUTOGENERATED
  /* <BEGIN_INSERT_SCRIPTS */
  'update-schema': 'c2debcff74f391001e5e1bc171c9615178cfb985',
  /* <END_INSERT_SCRIPTS */
  // END OF THE AUTOGENERATED PORTION
}

export function getScriptSha(scriptName: string): string | undefined {
  return SCRIPTS[scriptName]
}
