// =============================================================================
// === ZONE BOUNDARIES — must match OE channel start indexes ===================
// =============================================================================
// OE CONFIG (must match exactly)
// Ch 0: 100 (Z8 strings, start 1908) | Ch 1: 208 (Z1) | Ch 2: 325 (Z2)
// Ch 3: 400 (Z3) | Ch 4: 300 (Z4) | Ch 5: 300 (Z5) | Ch 6: 300 (Z6)
// Ch 7: 75 (Z7 parallel)
// Total: 2,008 pixels (Z8 occupies the formerly unused 1908–2007 tail)

var ZONE_OFFSETS = array(8)
ZONE_OFFSETS[0] = 0       // Z1 start
ZONE_OFFSETS[1] = 208     // Z2 start
ZONE_OFFSETS[2] = 533     // Z3 start
ZONE_OFFSETS[3] = 933     // Z4 start
ZONE_OFFSETS[4] = 1233    // Z5 start
ZONE_OFFSETS[5] = 1533    // Z6 start
ZONE_OFFSETS[6] = 1833    // Z7 start
ZONE_OFFSETS[7] = 1908    // Z8 start (button-station strings, OE Ch 0)
// total pixelCount = 2008
