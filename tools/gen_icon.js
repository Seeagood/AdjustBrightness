// gen_icon.js - 生成 res/sun.ico: 单色小太阳 (16/20/24/32), 32bpp BGRA, 超采样抗锯齿
// 用法: node tools/gen_icon.js
const fs = require("fs");
const path = require("path");

const SIZES = [16, 20, 24, 32];
const SS = 8;                       // 超采样
const COLOR = [0xD4, 0x78, 0x00];   // #0078D4 -> BGR

// 几何 (16 网格, 与 wireframe 一致): 圆环 r=3 w=1.3; 8 条射线 r 5..7 w=1.3
function covered(x, y) {
  const cx = x - 8, cy = y - 8;
  const r = Math.hypot(cx, cy);
  const w = 1.3 / 2;
  if (Math.abs(r - 3) <= w) return true;                 // 圆环
  if (r >= 5 - w && r <= 7 + w) {                        // 射线带
    const ang = Math.atan2(cy, cx);
    const step = Math.PI / 4;
    const k = Math.round(ang / step);
    const da = Math.abs(ang - k * step);
    if (r * Math.sin(da) <= w && k % 1 === 0) {
      // 射线沿线段方向投影需在 [5,7] 内 (环形带已保证), 只需垂直距离 <= w
      return true;
    }
  }
  return false;
}

function renderIcon(N) {
  const px = Buffer.alloc(N * N * 4, 0);
  for (let y = 0; y < N; y++) {
    for (let x = 0; x < N; x++) {
      let cov = 0;
      for (let sy = 0; sy < SS; sy++) {
        for (let sx = 0; sx < SS; sx++) {
          const gx = ((x + (sx + 0.5) / SS) / N) * 16;
          const gy = ((y + (sy + 0.5) / SS) / N) * 16;
          if (covered(gx, gy)) cov++;
        }
      }
      const a = Math.round((cov / (SS * SS)) * 255);
      const o = (y * N + x) * 4;
      if (a > 0) {
        px[o] = COLOR[0]; px[o + 1] = COLOR[1]; px[o + 2] = COLOR[2]; px[o + 3] = a;
      }
    }
  }
  return px;
}

function icoImage(N) {
  const px = renderIcon(N);
  const header = Buffer.alloc(40);
  header.writeUInt32LE(40, 0);
  header.writeInt32LE(N, 4);
  header.writeInt32LE(N * 2, 8);      // 高度含 XOR + AND
  header.writeUInt16LE(1, 12);
  header.writeUInt16LE(32, 14);
  header.writeUInt32LE(0, 20);        // BI_RGB
  // XOR: 底向上 BGRA
  const xor = Buffer.alloc(N * N * 4);
  for (let y = 0; y < N; y++) {
    px.copy(xor, (N - 1 - y) * N * 4, y * N * 4, (y + 1) * N * 4);
  }
  const andRow = Math.ceil(N / 32) * 4;
  const and = Buffer.alloc(andRow * N, 0);   // 全不透明 (alpha 在 XOR 内)
  header.writeUInt32LE(xor.length + and.length, 20);
  return Buffer.concat([header, xor, and]);
}

const images = SIZES.map(icoImage);
const dirSize = 6 + 16 * SIZES.length;
const dir = Buffer.alloc(6);
dir.writeUInt16LE(0, 0); dir.writeUInt16LE(1, 2); dir.writeUInt16LE(SIZES.length, 4);
const entries = [];
let offset = dirSize;
SIZES.forEach((N, i) => {
  const e = Buffer.alloc(16);
  e.writeUInt8(N >= 256 ? 0 : N, 0);
  e.writeUInt8(N >= 256 ? 0 : N, 1);
  e.writeUInt8(0, 2); e.writeUInt8(0, 3);
  e.writeUInt16LE(1, 4); e.writeUInt16LE(32, 6);
  e.writeUInt32LE(images[i].length, 8);
  e.writeUInt32LE(offset, 12);
  offset += images[i].length;
  entries.push(e);
});

const out = Buffer.concat([dir, ...entries, ...images]);
const dst = path.join(__dirname, "..", "res", "sun.ico");
fs.mkdirSync(path.dirname(dst), { recursive: true });
fs.writeFileSync(dst, out);
console.log("sun.ico:", out.length, "bytes,", SIZES.join("/"));
