#ifndef ALIOTH_PANEL_UI_RUNTIME_H
#define ALIOTH_PANEL_UI_RUNTIME_H

struct ui_theme {
	uint32_t bg;
	uint32_t header;
	uint32_t panel;
	uint32_t panel_alt;
	uint32_t line;
	uint32_t text;
	uint32_t muted;
	uint32_t faint;
	uint32_t accent;
	uint32_t green;
	uint32_t amber;
	uint32_t red;
	uint32_t blue;
	uint32_t purple;
};

static const struct ui_theme UI = {
	.bg = 0x000b0f14,
	.header = 0x0011161e,
	.panel = 0x00172029,
	.panel_alt = 0x00212b36,
	.line = 0x00344556,
	.text = 0x00eef4ff,
	.muted = 0x0097a7ba,
	.faint = 0x006a7788,
	.accent = 0x0000d1a7,
	.green = 0x004bd17a,
	.amber = 0x00f4b84a,
	.red = 0x00ff6170,
	.blue = 0x005aa8ff,
	.purple = 0x00b184ff,
};

static void ui_ellipsize_copy(char *dst, size_t dst_len, const char *value, int max_chars);

static void ui_outline(struct canvas *c, int x, int y, int w, int h, uint32_t color)
{
	rect(c, x, y, w, 3, color);
	rect(c, x, y + h - 3, w, 3, color);
	rect(c, x, y, 3, h, color);
	rect(c, x + w - 3, y, 3, h, color);
}

static void ui_page_header(struct canvas *c, const char *title, const char *subtitle)
{
	rect(c, 0, 0, c->width, c->height, UI.bg);
	rect(c, 0, 0, c->width, 232, UI.header);
	rect(c, 0, 228, c->width, 4, UI.line);
	text(c, 48, 54, title, 6, UI.text);
	text(c, 52, 136, subtitle, 3, UI.muted);
	draw_top_status(c, UI.muted);
}

static void ui_panel(struct canvas *c, int x, int y, int w, int h, const char *title,
	uint32_t accent)
{
	rect(c, x, y, w, h, UI.panel);
	rect(c, x, y, w, 6, accent);
	ui_outline(c, x, y, w, h, UI.line);
	if (title && title[0])
		text(c, x + 28, y + 26, title, 3, UI.muted);
}

static void ui_metric_card(struct canvas *c, int x, int y, int w, int h,
	const char *label, const char *value, const char *detail, uint32_t accent, double pct)
{
	int value_scale = text_width(c, value, 5) > w - 56 ? 4 : 5;
	int detail_y = pct >= 0.0 ? y + h - 88 : y + 156;
	char detail_clip[96];
	int detail_max_chars = 42;

	ui_panel(c, x, y, w, h, label, accent);
	text(c, x + 28, y + 78, value, value_scale, UI.text);
	if (detail && detail[0]) {
		ui_ellipsize_copy(detail_clip, sizeof(detail_clip), detail, detail_max_chars);
		while (detail_max_chars > 6 && text_width(c, detail_clip, 3) > w - 56) {
			detail_max_chars -= 2;
			ui_ellipsize_copy(detail_clip, sizeof(detail_clip), detail, detail_max_chars);
		}
		text(c, x + 30, detail_y, detail_clip, 3, UI.muted);
	}
	if (pct >= 0.0)
		bar(c, x + 28, y + h - 52, w - 56, 18, pct, 0x002a3541, accent);
}

static void ui_info_row(struct canvas *c, int x, int y, const char *label, const char *value,
	uint32_t value_color)
{
	char clipped[112];

	short_copy(clipped, sizeof(clipped), value, 42);
	text(c, x, y + 4, label, 3, UI.faint);
	text(c, x + 128, y, clipped, 3, value_color);
}

static void ui_ellipsize_copy(char *dst, size_t dst_len, const char *value, int max_chars)
{
	size_t n;

	if (dst_len == 0)
		return;
	if (!value)
		value = "-";
	if (max_chars < 4) {
		short_copy(dst, dst_len, value, max_chars > 0 ? (size_t)max_chars : 0);
		return;
	}
	n = strlen(value);
	if (n <= (size_t)max_chars) {
		short_copy(dst, dst_len, value, (size_t)max_chars);
		return;
	}
	short_copy(dst, dst_len, value, (size_t)(max_chars - 3));
	if (strlen(dst) + 3 < dst_len)
		strcat(dst, "...");
}

static void ui_info_row_fit(struct canvas *c, int x, int y, const char *label, const char *value,
	uint32_t value_color, int value_max_w)
{
	char clipped[112];
	int scale = 4;
	int max_chars = 52;

	if (value_max_w <= 0)
		value_max_w = c->width - (x + 128) - 48;
	ui_ellipsize_copy(clipped, sizeof(clipped), value, max_chars);
	while (max_chars > 6 && text_width(c, clipped, scale) > value_max_w) {
		max_chars -= 2;
		ui_ellipsize_copy(clipped, sizeof(clipped), value, max_chars);
	}
	text(c, x, y + 8, label, 3, UI.faint);
	text(c, x + 128, y, clipped, scale, value_color);
}

static void ui_chip(struct canvas *c, int x, int y, int w, int h, const char *label,
	bool active, uint32_t accent)
{
	uint32_t bg = active ? accent : UI.panel_alt;
	uint32_t fg = active ? UI.bg : UI.text;
	int tw = text_width(c, label, 3);

	rect(c, x, y, w, h, bg);
	ui_outline(c, x, y, w, h, active ? accent : UI.line);
	text(c, x + (w - tw) / 2, y + (h - 27) / 2, label, 3, fg);
}

#endif
