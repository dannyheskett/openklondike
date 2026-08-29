// Capture the Google Play and App Store screenshot sets from the built web
// bundle, at the exact pixel sizes each store requires.
//
// The web build is the right source: it compiles the same src/render*.c the
// phone builds do, and it selects the touch board at runtime from
// matchMedia('(pointer: coarse)') -- which a headless browser can simply be told
// is true. So these are real frames from the real renderer, not mockups, and
// they can be regenerated on any machine without a device or a Mac.
//
//   ./scripts/build_raylib_web.sh && make web
//   node scripts/gen_store_screenshots.mjs
//
// Requires playwright (the @playwright/mcp install ships one; PLAYWRIGHT_PATH
// overrides the module location, and CHROMIUM_PATH a browser binary, for the
// common case where the module and the downloaded browsers are different
// revisions).
//
// The game is WebGL, so the browser needs a GL backend. On a headless machine
// there is no GPU, hence the SwiftShader flags below -- without them the canvas
// comes back blank.
//
// Three traps that cost time before, all worked around below:
//   - The matchMedia shim must carry no-op addListener/addEventListener members.
//     A bare {matches: true} object throws inside emscripten's startup and the
//     WebGL context is lost.
//   - page.screenshot() freezes the page's requestAnimationFrame loop, and with
//     it the game. Frames are read back off the canvas instead.
//   - The touch layer samples input once per rendered frame, so an instant
//     click is missed entirely. Every press below is held for several frames.
import { createServer } from 'node:http';
import { readFile, writeFile, mkdir } from 'node:fs/promises';
import { extname, join } from 'node:path';
import { createRequire } from 'node:module';
import { inflateSync, deflateSync } from 'node:zlib';

const require = createRequire(import.meta.url);
const PLAYWRIGHT = process.env.PLAYWRIGHT_PATH || 'playwright';
const { chromium } = require(PLAYWRIGHT);

const WEB_DIR = 'build/web';
const PORT = 8123;

// One entry per store slot. Play wants a 9:16 phone and tablet set; Apple wants
// the 6.9" iPhone set, which it scales down for every smaller device.
const TARGETS = [
  { dir: 'android/play-assets/screenshots/phone', width: 1080, height: 1920 },
  { dir: 'android/play-assets/screenshots/tablet', width: 2160, height: 3840 },
  { dir: 'ios/app-store-assets/screenshots/iphone-6.9', width: 1290, height: 2796 },
  // Landscape sets. Both stores accept either orientation, but each slot wants
  // one consistent set -- so these get their own folders and you upload whichever
  // orientation you want that slot to show.
  { dir: 'android/play-assets/screenshots/phone-landscape', width: 1920, height: 1080 },
  { dir: 'android/play-assets/screenshots/tablet-landscape', width: 3840, height: 2160 },
  { dir: 'ios/app-store-assets/screenshots/iphone-6.9-landscape', width: 2796, height: 1290 },
];

const MIME = { '.html': 'text/html', '.js': 'text/javascript', '.wasm': 'application/wasm' };

function serve() {
  const server = createServer(async (req, res) => {
    const path = join(WEB_DIR, req.url === '/' ? 'openklondike.html' : req.url.slice(1));
    try {
      const body = await readFile(path);
      res.writeHead(200, { 'Content-Type': MIME[extname(path)] || 'application/octet-stream' });
      res.end(body);
    } catch {
      res.writeHead(404).end();
    }
  });
  return new Promise((ok) => server.listen(PORT, () => ok(server)));
}

// Tell the page it has a coarse pointer, so main.c selects the touch board.
const COARSE_POINTER = () => {
  const real = window.matchMedia.bind(window);
  window.matchMedia = (q) => (q.includes('coarse')
    ? {
      matches: true,
      media: q,
      onchange: null,
      addListener() {},
      removeListener() {},
      addEventListener() {},
      removeEventListener() {},
      dispatchEvent: () => false,
    }
    : real(q));
};

const sleep = (ms) => new Promise((r) => setTimeout(r, ms));

// Press and hold long enough for the frame-sampled touch layer to see a contact,
// then release, without moving: that is a tap, which sends a card home.
async function tap(page, [x, y]) {
  await page.mouse.move(Math.round(x), Math.round(y));
  await page.mouse.down();
  await sleep(140);
  await page.mouse.up();
  await sleep(200);
}

// Where the piles are, so the taps below can aim at them. This mirrors
// layout_portrait() in src/render_portrait.c -- width pass, height pass and gap
// spread, all three, because in landscape it is the height that drives the card
// size. It exists only to point a mouse at the right card; nothing in the game
// reads it. If a tap starts landing on bare felt, this is what has drifted out
// of step with the C.
function boardGeometry(w, h) {
  const margin = Math.max(Math.floor(Math.min(w, h) / 28), 6);
  const titleFsOf = (cw) => Math.max(Math.floor((cw * 22) / 80), 10);

  // Width pass: 7 cards plus 6 gaps of a fifth of a card each span the usable width.
  let cardW = Math.floor(((w - 2 * margin) * 10) / 82);

  // Height pass: three card-heights of playable space in landscape, four in
  // portrait. This is what shrinks the cards when the short axis binds, and it
  // is why a landscape mirror cannot skip it.
  const wantHeights = w > h ? 3 : 4;
  for (let pass = 0; pass < 2; pass += 1) {
    const cardH = Math.floor((cardW * 112) / 80);
    const top = Math.max(2 * titleFsOf(cardW), 1);
    const statusH = Math.floor((Math.max(Math.floor((cardW * 18) / 80), 9) * 14) / 9);
    const rowGap = Math.floor((cardW * 28) / 80);
    const budget = Math.max(h - top - margin - rowGap - statusH, 4);
    if (wantHeights * cardH > budget) {
      cardW = Math.floor((Math.floor(budget / wantHeights) * 80) / 112);
    } else {
      break;
    }
  }

  const cardH = Math.floor((cardW * 112) / 80);
  const fanDown = Math.max(Math.floor((cardW * 12) / 80), 3);
  const topY = 2 * titleFsOf(cardW) + margin;
  const tabY = topY + cardH + Math.floor((cardW * 28) / 80);

  // Leftover width goes into the gaps, capped at half a card.
  let gap = Math.max(Math.floor((cardW * 16) / 80), 2);
  const leftover = w - 2 * margin - 7 * cardW - 6 * gap;
  if (leftover > 0) {
    gap += Math.min(Math.floor(leftover / 6), Math.max(Math.floor(cardW / 2) - gap, 0));
  }

  const left = Math.max(Math.floor((w - (7 * cardW + 6 * gap)) / 2), 0);
  const colX = (c) => left + c * (cardW + gap) + cardW / 2;
  return {
    stock: [colX(0), topY + cardH / 2],
    waste: [colX(1), topY + cardH / 2],
    // Column c holds c + 1 cards, all but the last face down, so its top card
    // sits c face-down fan steps below the start of the tableau.
    column: (c) => [colX(c), tabY + c * fanDown + cardH / 2],
  };
}

// Fingerprint of the current frame. Used to prove an action actually reached the
// game: a silent no-op here would otherwise ship a folder of identical menus.
const frameId = (page) => page.evaluate(
  () => document.getElementById('canvas').toDataURL().length);

// Focus the canvas and hold a key down for several frames.
//
// Both halves matter. raylib's GLFW port listens on the canvas, so a press
// delivered anywhere else is dropped -- re-focusing before every press survives
// anything having moved focus in between. And the game samples the keyboard once
// per rendered frame, so keyboard.press()'s zero-delay down/up pair usually lands
// entirely between two frames and is never seen at all. This is the same trap the
// touch presses hit, and it fails intermittently, which is why the result is
// asserted rather than assumed.
async function press(page, key, what) {
  const before = await frameId(page);
  await page.focus('#canvas');
  await page.keyboard.down(key);
  await sleep(150);
  await page.keyboard.up(key);
  await sleep(900);
  if (await frameId(page) === before) {
    throw new Error(`"${key}" did not reach the game (${what}); nothing changed on screen`);
  }
}

// Save the current frame by reading the canvas back, rather than through
// page.screenshot(): a CDP screenshot freezes this page's requestAnimationFrame
// loop, so the very first shot would stop the game and every later one would be
// the same still menu. The shell sets preserveDrawingBuffer, so toDataURL()
// returns the exact presented frame, already at the canvas's native size.
// --- PNG: drop the alpha channel ------------------------------------------
// Apple rejects a screenshot that carries an alpha channel, and a canvas always
// exports one: toDataURL has no opaque mode, and neither does drawing through a
// {alpha:false} 2D context first. So the RGBA PNG the browser produces is
// rewritten here as a plain RGB one. Done with node's own zlib rather than an
// image library, so the script has no dependency beyond playwright.
//
// The frames are fully opaque to begin with (the board fills the canvas), so
// there is nothing to composite -- the alpha byte of each pixel is simply
// dropped. Chromium always emits 8-bit, non-interlaced RGBA, which is the only
// input shape handled below; anything else is passed through untouched.
const PNG_SIGNATURE = Buffer.from([0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a]);

const CRC_TABLE = (() => {
  const t = new Int32Array(256);
  for (let n = 0; n < 256; n += 1) {
    let c = n;
    for (let k = 0; k < 8; k += 1) c = c & 1 ? 0xedb88320 ^ (c >>> 1) : c >>> 1;
    t[n] = c;
  }
  return t;
})();

function crc32(buf) {
  let c = ~0;
  for (let i = 0; i < buf.length; i += 1) c = CRC_TABLE[(c ^ buf[i]) & 0xff] ^ (c >>> 8);
  return ~c >>> 0;
}

function chunk(type, data) {
  const out = Buffer.alloc(data.length + 12);
  out.writeUInt32BE(data.length, 0);
  out.write(type, 4, 'ascii');
  data.copy(out, 8);
  out.writeUInt32BE(crc32(out.subarray(4, 8 + data.length)), 8 + data.length);
  return out;
}

const paeth = (a, b, c) => {
  const p = a + b - c;
  const pa = Math.abs(p - a);
  const pb = Math.abs(p - b);
  const pc = Math.abs(p - c);
  if (pa <= pb && pa <= pc) return a;
  return pb <= pc ? b : c;
};

// Reverse the per-scanline filter PNG applies before compression, in place.
function unfilter(raw, width, height, bpp) {
  const stride = width * bpp;
  const out = Buffer.alloc(height * stride);
  let pos = 0;
  for (let y = 0; y < height; y += 1) {
    const type = raw[pos];
    pos += 1;
    const line = out.subarray(y * stride, (y + 1) * stride);
    const prev = y > 0 ? out.subarray((y - 1) * stride, y * stride) : null;
    for (let x = 0; x < stride; x += 1) {
      const cur = raw[pos + x];
      const a = x >= bpp ? line[x - bpp] : 0;
      const b = prev ? prev[x] : 0;
      const c = prev && x >= bpp ? prev[x - bpp] : 0;
      let v;
      switch (type) {
        case 0: v = cur; break;
        case 1: v = cur + a; break;
        case 2: v = cur + b; break;
        case 3: v = cur + ((a + b) >> 1); break;
        case 4: v = cur + paeth(a, b, c); break;
        default: throw new Error(`unsupported PNG filter ${type}`);
      }
      line[x] = v & 0xff;
    }
    pos += stride;
  }
  return out;
}

function dropAlpha(png) {
  if (!png.subarray(0, 8).equals(PNG_SIGNATURE)) return png;

  let pos = 8;
  let ihdr = null;
  const idat = [];
  while (pos < png.length) {
    const len = png.readUInt32BE(pos);
    const type = png.toString('ascii', pos + 4, pos + 8);
    const data = png.subarray(pos + 8, pos + 8 + len);
    if (type === 'IHDR') ihdr = data;
    else if (type === 'IDAT') idat.push(data);
    else if (type === 'IEND') break;
    pos += len + 12;
  }
  if (!ihdr) return png;

  const width = ihdr.readUInt32BE(0);
  const height = ihdr.readUInt32BE(4);
  const [depth, colorType, , , interlace] = ihdr.subarray(8, 13);
  if (depth !== 8 || colorType !== 6 || interlace !== 0) return png; // not RGBA/8/plain

  const pixels = unfilter(inflateSync(Buffer.concat(idat)), width, height, 4);

  // Re-emit as RGB with filter type 0 on every scanline: the images are small
  // and flat, so the compression a smarter filter would buy is not worth it.
  const rgb = Buffer.alloc(height * (1 + width * 3));
  for (let y = 0; y < height; y += 1) {
    const src = y * width * 4;
    const dst = y * (1 + width * 3) + 1;
    for (let x = 0; x < width; x += 1) {
      rgb[dst + x * 3] = pixels[src + x * 4];
      rgb[dst + x * 3 + 1] = pixels[src + x * 4 + 1];
      rgb[dst + x * 3 + 2] = pixels[src + x * 4 + 2];
    }
  }

  const header = Buffer.alloc(13);
  header.writeUInt32BE(width, 0);
  header.writeUInt32BE(height, 4);
  header[8] = 8;   // bit depth
  header[9] = 2;   // colour type: truecolour, no alpha
  return Buffer.concat([
    PNG_SIGNATURE,
    chunk('IHDR', header),
    chunk('IDAT', deflateSync(rgb, { level: 9 })),
    chunk('IEND', Buffer.alloc(0)),
  ]);
}

async function shoot(page, target, name) {
  await sleep(500);
  const url = await page.evaluate(
    () => document.getElementById('canvas').toDataURL('image/png'));
  const png = dropAlpha(Buffer.from(url.slice('data:image/png;base64,'.length), 'base64'));
  await writeFile(join(target.dir, name), png);
  console.log('gen_store_screenshots: wrote %s/%s', target.dir, name);
}

async function main() {
  const server = await serve();
  const browser = await chromium.launch({
    executablePath: process.env.CHROMIUM_PATH || undefined,
    args: ['--use-gl=angle', '--use-angle=swiftshader', '--enable-unsafe-swiftshader'],
  });

  for (const target of TARGETS) {
    await mkdir(target.dir, { recursive: true });
    const page = await browser.newPage({
      viewport: { width: target.width, height: target.height },
      deviceScaleFactor: 1,
    });
    await page.addInitScript(COARSE_POINTER);
    await page.goto(`http://localhost:${PORT}/openklondike.html`);
    await sleep(2500);

    const board = boardGeometry(target.width, target.height);

    await shoot(page, target, '01-menu.png');

    // "New Game" is the highlighted row on a fresh launch. Selected with the
    // keyboard rather than a tap: the web build polls keys whichever board it
    // is showing, and a key press cannot miss a row by a few pixels.
    await press(page, 'Enter', 'start a new game');
    await shoot(page, target, '02-deal.png');

    // Tap each tableau's top card: any Ace flies to a foundation, and the rest
    // settle onto whatever they legally build on.
    for (let c = 0; c < 7; c += 1) await tap(page, board.column(c));
    await shoot(page, target, '03-play.png');

    // Work the stock so the waste is showing, then try the turned card.
    for (let i = 0; i < 4; i += 1) await tap(page, board.stock);
    await tap(page, board.waste);
    for (let c = 0; c < 7; c += 1) await tap(page, board.column(c));
    await tap(page, board.stock);
    await shoot(page, target, '04-stock.png');

    await page.close();
  }

  await browser.close();
  server.close();
}

main().catch((e) => { console.error(e); process.exit(1); });
