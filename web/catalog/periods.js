// Trình sửa bảng tiết của một trường (TV2).
import { get } from "../core/api.js";
import { h } from "../core/ui.js";

export async function periodEditor(el, schoolId) {
  const schools = await get("/api/schools");
  const school = schools.find((s) => s.id === schoolId);
  if (!school) {
    el.append(h("p", {}, "Không có trường này"));
    return;
  }
  // TODO(TV2): bảng nhập (số tiết, giờ bắt đầu, giờ kết thúc), thêm / xoá dòng,
  //            xem trước dạng thanh thời gian, báo chồng tiết ngay khi nhập,
  //            lưu bằng PUT /api/schools/{id}/periods (server kiểm tra lại)
  el.append(
    h("div", { class: "card" },
      h("h2", {}, `Bảng tiết: ${school.name}`),
      h("table", { class: "cat-table" },
        h("tr", {}, h("th", {}, "Tiết"), h("th", {}, "Bắt đầu"), h("th", {}, "Kết thúc")),
        school.periods.map((p) => h("tr", {}, h("td", {}, p.no), h("td", {}, p.start), h("td", {}, p.end)))))
  );
}
