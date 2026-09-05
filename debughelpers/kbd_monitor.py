"""
键盘底层监听工具 (Windows WH_KEYBOARD_LL 钩子)

用途:
    持续监听全局键盘事件, 打印每个事件的原始字段,
    用于检查按键的虚拟键码、扫描码、标志与注入来源。

关键点:
    - 使用 Win32 底层键盘钩子, 无需第三方库 (纯 ctypes)。
    - 能拿到虚拟键码(VK)、扫描码(scancode)、扩展键标志, 以及
      "注入标志(injected)": 经 SendInput 注入的按键会带 LLKHF_INJECTED
      标志, 真实物理按键不带。部分程序通过 RawInput / DirectInput 读键,
      只认物理按键或特定扫描码, 可据此排查按键为何不被识别。

运行 (cmd, 需要管理员权限才能捕获管理员窗口的按键):
    python kbd_monitor.py

退出:
    连按两次 ESC, 或直接 Ctrl+C / 关闭窗口。
"""

import ctypes
import ctypes.wintypes as wt
import sys
import time
import atexit

user32 = ctypes.WinDLL("user32", use_last_error=True)
kernel32 = ctypes.WinDLL("kernel32", use_last_error=True)

# ---- 常量 ----
WH_KEYBOARD_LL = 13
WM_KEYDOWN = 0x0100
WM_KEYUP = 0x0101
WM_SYSKEYDOWN = 0x0104
WM_SYSKEYUP = 0x0105

MSG_NAMES = {
    WM_KEYDOWN: "KEYDOWN",
    WM_KEYUP: "KEYUP",
    WM_SYSKEYDOWN: "SYSKEYDOWN",  # 通常伴随 Alt
    WM_SYSKEYUP: "SYSKEYUP",
}

# KBDLLHOOKSTRUCT.flags 位标志
LLKHF_EXTENDED = 0x01        # 扩展键 (方向键、右 Ctrl/Alt、小键盘等)
LLKHF_LOWER_IL_INJECTED = 0x02  # 由较低完整性级别进程注入
LLKHF_INJECTED = 0x10        # 注入的按键 (SendInput 等)
LLKHF_ALTDOWN = 0x20         # Alt 键按下
LLKHF_UP = 0x80              # 松开事件

HC_ACTION = 0

# ---- 虚拟键码名称表 (常见键) ----
VK_NAMES = {
    0x01: "LBUTTON", 0x02: "RBUTTON", 0x04: "MBUTTON",
    0x05: "XBUTTON1", 0x06: "XBUTTON2",
    0x08: "BACKSPACE", 0x09: "TAB", 0x0D: "ENTER",
    0x10: "SHIFT", 0x11: "CTRL", 0x12: "ALT", 0x13: "PAUSE",
    0x14: "CAPSLOCK", 0x1B: "ESC", 0x20: "SPACE",
    0x21: "PAGEUP", 0x22: "PAGEDOWN", 0x23: "END", 0x24: "HOME",
    0x25: "LEFT", 0x26: "UP", 0x27: "RIGHT", 0x28: "DOWN",
    0x2C: "PRINTSCREEN", 0x2D: "INSERT", 0x2E: "DELETE",
    0x5B: "LWIN", 0x5C: "RWIN", 0x5D: "APPS",
    0x60: "NUM0", 0x61: "NUM1", 0x62: "NUM2", 0x63: "NUM3",
    0x64: "NUM4", 0x65: "NUM5", 0x66: "NUM6", 0x67: "NUM7",
    0x68: "NUM8", 0x69: "NUM9",
    0x6A: "NUM*", 0x6B: "NUM+", 0x6D: "NUM-", 0x6E: "NUM.", 0x6F: "NUM/",
    0x70: "F1", 0x71: "F2", 0x72: "F3", 0x73: "F4", 0x74: "F5",
    0x75: "F6", 0x76: "F7", 0x77: "F8", 0x78: "F9", 0x79: "F10",
    0x7A: "F11", 0x7B: "F12",
    0x90: "NUMLOCK", 0x91: "SCROLLLOCK",
    0xA0: "LSHIFT", 0xA1: "RSHIFT", 0xA2: "LCTRL", 0xA3: "RCTRL",
    0xA4: "LALT", 0xA5: "RALT",
    0xBA: ";", 0xBB: "=", 0xBC: ",", 0xBD: "-", 0xBE: ".", 0xBF: "/",
    0xC0: "`", 0xDB: "[", 0xDC: "\\", 0xDD: "]", 0xDE: "'",
}


def vk_name(vk):
    """返回虚拟键码的可读名称。"""
    if vk in VK_NAMES:
        return VK_NAMES[vk]
    # 数字 0-9 和字母 A-Z 的 VK 与 ASCII 一致
    if 0x30 <= vk <= 0x39 or 0x41 <= vk <= 0x5A:
        return chr(vk)
    return f"VK_0x{vk:02X}"


class KBDLLHOOKSTRUCT(ctypes.Structure):
    _fields_ = [
        ("vkCode", wt.DWORD),
        ("scanCode", wt.DWORD),
        ("flags", wt.DWORD),
        ("time", wt.DWORD),
        # dwExtraInfo 是 ULONG_PTR（指针宽度整数），用 c_size_t 而非指针类型
        ("dwExtraInfo", ctypes.c_size_t),
    ]


# 钩子回调签名: LRESULT CALLBACK(int nCode, WPARAM wParam, LPARAM lParam)
LRESULT = ctypes.c_ssize_t
HOOKPROC = ctypes.WINFUNCTYPE(
    LRESULT, ctypes.c_int, wt.WPARAM, wt.LPARAM
)

user32.SetWindowsHookExW.restype = wt.HHOOK
user32.SetWindowsHookExW.argtypes = [
    ctypes.c_int, HOOKPROC, wt.HINSTANCE, wt.DWORD
]
user32.CallNextHookEx.restype = LRESULT
user32.CallNextHookEx.argtypes = [
    wt.HHOOK, ctypes.c_int, wt.WPARAM, wt.LPARAM
]
user32.UnhookWindowsHookEx.restype = wt.BOOL
user32.UnhookWindowsHookEx.argtypes = [wt.HHOOK]

# 必须设置 restype, 否则 64 位下模块句柄会被按 c_int 截断 -> 报错 126
kernel32.GetModuleHandleW.restype = wt.HMODULE
kernel32.GetModuleHandleW.argtypes = [wt.LPCWSTR]

_hook_handle = None
_start_time = time.time()
_esc_count = 0  # 连按两次 ESC 退出


def _fmt_flags(flags):
    """把 flags 拆成可读标志列表。"""
    parts = []
    if flags & LLKHF_EXTENDED:
        parts.append("EXTENDED")
    if flags & LLKHF_INJECTED:
        parts.append("INJECTED")          # <-- SendInput 注入
    if flags & LLKHF_LOWER_IL_INJECTED:
        parts.append("LOWER_IL_INJECTED")
    if flags & LLKHF_ALTDOWN:
        parts.append("ALTDOWN")
    if flags & LLKHF_UP:
        parts.append("UP")
    return "|".join(parts) if parts else "-"


def _low_level_keyboard_proc(nCode, wParam, lParam):
    global _esc_count
    if nCode == HC_ACTION:
        kb = ctypes.cast(lParam, ctypes.POINTER(KBDLLHOOKSTRUCT)).contents
        msg = MSG_NAMES.get(wParam, f"MSG_0x{wParam:X}")
        vk = kb.vkCode
        injected = bool(kb.flags & LLKHF_INJECTED)
        # dwExtraInfo 现在是整数值，直接使用
        extra_val = kb.dwExtraInfo

        rel = time.time() - _start_time
        source = "INJECTED" if injected else "PHYSICAL"
        line = (
            f"[{rel:9.3f}s] {msg:<10} "
            f"src={source:<8} "
            f"vk=0x{vk:02X}({vk_name(vk):<9}) "
            f"scan=0x{kb.scanCode:02X} "
            f"flags=0x{kb.flags:02X}[{_fmt_flags(kb.flags)}] "
            f"extra=0x{extra_val:X}"
        )
        print(line, flush=True)

        # 连按两次 ESC 退出
        if wParam in (WM_KEYDOWN, WM_SYSKEYDOWN) and vk == 0x1B:
            _esc_count += 1
            if _esc_count >= 2:
                print(">> 检测到连按两次 ESC, 退出。", flush=True)
                user32.PostQuitMessage(0)
        elif wParam in (WM_KEYDOWN, WM_SYSKEYDOWN):
            _esc_count = 0

    return user32.CallNextHookEx(_hook_handle, nCode, wParam, lParam)


# 保持回调对象存活, 防止被 GC 回收导致钩子回调野指针
_callback = HOOKPROC(_low_level_keyboard_proc)


def _cleanup():
    global _hook_handle
    if _hook_handle:
        user32.UnhookWindowsHookEx(_hook_handle)
        _hook_handle = None


def main():
    global _hook_handle
    hmod = kernel32.GetModuleHandleW(None)
    _hook_handle = user32.SetWindowsHookExW(
        WH_KEYBOARD_LL, _callback, hmod, 0
    )
    if not _hook_handle:
        err = ctypes.get_last_error()
        print(f"安装键盘钩子失败, GetLastError={err}", file=sys.stderr)
        sys.exit(1)

    atexit.register(_cleanup)

    print("=" * 78)
    print(" 键盘底层监听已启动 (WH_KEYBOARD_LL)")
    print(" 列: 时间 / 消息类型 / 来源(PHYSICAL物理 或 INJECTED注入) /")
    print("     vk虚拟键码 / scan扫描码 / flags标志 / extra附加信息")
    print(" 提示: 经 SendInput 注入的按键来源显示 INJECTED,")
    print("       真实按键显示 PHYSICAL。重点关注 src / scan / extra。")
    print(" 退出: 连按两次 ESC, 或 Ctrl+C。")
    print("=" * 78, flush=True)

    # 消息循环 (钩子回调依赖当前线程有消息泵)
    msg = wt.MSG()
    try:
        while True:
            ret = user32.GetMessageW(ctypes.byref(msg), None, 0, 0)
            if ret == 0:      # WM_QUIT
                break
            if ret == -1:     # 错误
                break
            user32.TranslateMessage(ctypes.byref(msg))
            user32.DispatchMessageW(ctypes.byref(msg))
    except KeyboardInterrupt:
        print("\n>> Ctrl+C, 退出。", flush=True)
    finally:
        _cleanup()


if __name__ == "__main__":
    main()
