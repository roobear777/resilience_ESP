#!/usr/bin/env node
// =============================================================================
// build.js — concatenates src/ modules into single-file PixelBlaze patterns.
//
// The PixelBlaze pattern language has no import/include, so deployable patterns
// must be single files. Source of truth lives in src/; dist/ is generated.
//
// Usage:
//   node build.js               build all variants
//   node build.js --full        v2.1-equivalent: buttons + state + fire + LEDs
//   node build.js --led-only    fire code stripped (buttons + LEDs remain)
//   node build.js --pressure    burn-in: all zones active, no buttons/fire
// =============================================================================

const fs = require("fs");
const path = require("path");

const SRC = path.join(__dirname, "src");
const DIST = path.join(__dirname, "dist");

const ZONES = [
  "zones/z1-mouth.js",
  "zones/z2-shoulder.js",
  "zones/z3-midbody.js",
  "zones/z4-rear.js",
  "zones/legs.js",
  "zones/z7-digestive.js",
  "zones/z8-string-lights.js",
];

const VARIANTS = {
  full: {
    out: "master-full.js",
    title: "TARDIGRADE MASTER PATTERN — FULL (buttons + fire + LEDs)",
    notes: [
      "v2.1-equivalent build. 7 buttons independently activate their zones;",
      "all 7 held = payoff. 9 poofer GPIOs with hard safety caps.",
      "Runtime gates: buttonsConnected, poofersArmed (see source comments).",
    ],
    files: [
      "config.js",
      "fire/config.js",
      "layout.js",
      "lib/animations.js",
      "buttons.js",
      "zone-state.js",
      ...ZONES,
      "fire/poofers.js",
      "main.js",
    ],
    stripFire: false,
  },
  "led-only": {
    out: "master-led-only.js",
    title: "TARDIGRADE MASTER PATTERN — LED-ONLY (no fire code)",
    notes: [
      "All poofer/fire code is REMOVED from this build — no poofer GPIO is ever",
      "configured or driven. Buttons and zone ambient/active behavior unchanged.",
      "Use for LED testing, bring-up, or running without fire hardware.",
    ],
    files: [
      "config.js",
      "layout.js",
      "lib/animations.js",
      "buttons.js",
      "zone-state.js",
      ...ZONES,
      "main.js",
    ],
    stripFire: true,
  },
  pressure: {
    out: "pressure-test.js",
    title: "TARDIGRADE PRESSURE TEST — all zones active continuously",
    notes: [
      "Burn-in test: every zone runs its ACTIVE animation continuously. No",
      "buttons, no state machine, no fire. Draws significantly more power than",
      "the interactive installation — bench/burn-in use only, never at an event.",
    ],
    files: ["config.js", "layout.js", "lib/animations.js", ...ZONES, "main-pressure.js"],
    stripFire: false,
  },
};

function banner(variant, v) {
  const lines = [
    "// =============================================================================",
    `// ${v.title}`,
    "// Resilience 2026 — Burning Man",
    "// =============================================================================",
    `// GENERATED FILE — DO NOT EDIT. Built by pixelblaze/build.js (variant: ${variant}).`,
    "// Edit the modules in pixelblaze/src/ and rebuild.",
    "//",
    ...v.notes.map((n) => `// ${n}`),
    "//",
    "// OE CONFIG (must match exactly)",
    "// Ch 0: 100 (Z8 strings, start 1908) | Ch 1: 208 (Z1) | Ch 2: 325 (Z2)",
    "// Ch 3: 400 (Z3) | Ch 4: 300 (Z4) | Ch 5: 300 (Z5) | Ch 6: 300 (Z6)",
    "// Ch 7: 75 (Z7 parallel)",
    "// Total: 2,008 pixels",
    "// =============================================================================",
  ];
  return lines.join("\n") + "\n";
}

function build(variant) {
  const v = VARIANTS[variant];
  const parts = [banner(variant, v)];

  for (const file of v.files) {
    let code = fs.readFileSync(path.join(SRC, file), "utf8");
    if (v.stripFire) {
      code = code
        .split("\n")
        .filter((line) => !line.includes("//#fire"))
        .join("\n");
    }
    parts.push(`\n// ─── src/${file} ${"─".repeat(Math.max(1, 60 - file.length))}\n`);
    parts.push(code);
  }

  const output = parts.join("\n");

  // Sanity checks
  if (v.stripFire) {
    for (const token of ["POOFER_PINS", "updatePoofers", "poofersArmed", "writePooferPins"]) {
      if (output.includes(token)) {
        throw new Error(`led-only build still contains fire token: ${token}`);
      }
    }
  }
  const exports = [...output.matchAll(/^export function (\w+)/gm)].map((m) => m[1]);
  for (const required of ["beforeRender", "render"]) {
    if (!exports.includes(required)) {
      throw new Error(`${variant} build is missing export: ${required}`);
    }
  }

  fs.mkdirSync(DIST, { recursive: true });
  const outPath = path.join(DIST, v.out);
  fs.writeFileSync(outPath, output);
  const lineCount = output.split("\n").length;
  console.log(`built dist/${v.out} (${lineCount} lines)`);
}

const arg = process.argv[2];
if (arg) {
  const variant = arg.replace(/^--/, "");
  if (!VARIANTS[variant]) {
    console.error(`unknown variant: ${arg}\nvalid: ${Object.keys(VARIANTS).map((k) => "--" + k).join(", ")}`);
    process.exit(1);
  }
  build(variant);
} else {
  for (const variant of Object.keys(VARIANTS)) build(variant);
}
