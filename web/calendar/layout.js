// Xếp các khối chồng nhau trong một ngày thành các cột cạnh nhau (TV4).
//
// BẢN GIẢ: mỗi khối một cột riêng nếu có chồng, không gom nhóm.
// TODO(TV4): gom các khối chồng nhau thành cụm; trong mỗi cụm gán lane nhỏ nhất còn trống
//            (thuật toán interval partitioning), lanes = số lane của cụm đó.
const toMin = (hm) => Number(hm.slice(0, 2)) * 60 + Number(hm.slice(3, 5));

/** @returns {{item, lane:number, lanes:number}[]} */
export function layoutDay(items) {
  const sorted = [...items].sort((a, b) => toMin(a.start) - toMin(b.start));
  const overlap = sorted.some((a, i) => sorted.slice(i + 1).some((b) => toMin(b.start) < toMin(a.end)));
  return sorted.map((item, i) => ({ item, lane: overlap ? i : 0, lanes: overlap ? sorted.length : 1 }));
}
