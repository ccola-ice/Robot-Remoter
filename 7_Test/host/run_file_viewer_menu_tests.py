"""抽取生产菜单流程，验证文件浏览与预览页面的事件、重试和刷新。"""
from pathlib import Path
import re
import subprocess
import sys
import tempfile

host = Path(__file__).resolve().parent
root = host.parents[1]
compiler = sys.argv[1] if len(sys.argv) > 1 else "gcc"
source = (root / "1_App/menu.c").read_text(encoding="latin1")


def match(pattern):
    results = re.findall(pattern, source, re.M | re.S)
    if len(results) != 1:
        raise RuntimeError("Expected one production match: " + pattern)
    return results[0]


names = [
    "menu_get_key", "menu_post_key", "menu_set_background_service",
    "menu_file_service", "menu_file_begin", "menu_browser_load_drives",
    "menu_file_extension_is", "menu_file_kind", "menu_file_text_status",
    "menu_file_return", "menu_handle_browser_key", "menu_handle_file_viewer_key",
    "menu_draw_browser", "menu_image_error", "menu_gif_tick", "menu_draw_gif", "menu_draw_file_viewer", "menu_handle_page_key",
    "menu_refresh_dynamic_page", "menu_tick_10ms", "menu_process",
]
functions = "\n".join(match(
    r"(^" + r"(?:static )?(?:void|uint8_t|uint32_t) " + name +
    r"\([^;]*?\)\s*\{.*?^\})") for name in names)
state = "\n".join([
    match(r"(^typedef enum\s*\{.*?\} MenuPage;)"),
    match(r"(^enum \{ FILE_VIEW_UNSUPPORTED.*?^static file_text_result_t file_text_result;)"),
    "\n".join(re.findall(r"^#define (?:MENU_EVENT_QUEUE_SIZE|MENU_REFRESH_TICKS|CLOCK_REFRESH_TICKS)[^\r\n]*", source, re.M)),
    "\n".join(match(r"(^static [^\r\n;]+\b" + name + r"(?:\[[^\r\n]*\])?;)")
              for name in ["repeat_key", "repeat_pending", "event_queue", "event_read_index", "event_write_index",
                           "selected_group", "monitor_output_view", "refresh_tick_count", "clock_refresh_tick_count",
                           "clock_refresh_due", "page_dirty", "page_changed", "refresh_due", "current_page", "robot_telemetry"]),
])

with tempfile.TemporaryDirectory(prefix="remoter-file-viewer-menu-") as folder:
    temp = Path(folder)
    (temp / "stm32f4xx.h").write_text("#include <stdint.h>\n")
    (temp / "file_viewer_state.inc").write_text(state, encoding="latin1")
    (temp / "file_viewer_functions.inc").write_text(functions, encoding="latin1")
    executable = temp / "test.exe"
    subprocess.run([
        compiler, "-std=c99", "-O2", "-Wall", "-Wextra", "-Werror",
        "-I" + str(temp), "-I" + str(root / "1_App"),
        "-I" + str(root / "3_Protocol/FatFs"), "-I" + str(root / "5_SystemDrivers"),
        str(host / "file_viewer_menu_test.c"), "-o", str(executable)
    ], check=True)
    subprocess.run([str(executable)], check=True)
