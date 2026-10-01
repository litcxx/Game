// A share of the map as the HUD and the picker write it: to a tenth, with a
// decimal comma — "0,5%" (a capital's zone at the season's start), "25%".
export function percentText(percent: number): string {
  return `${String(percent).replace(".", ",")}%`;
}
