#!/usr/bin/env python3
"""E2E probe: drag a shell window's caption to its screen's top edge and
verify the system Aero snap maximizes it.

Covers the WS_MAXIMIZEBOX + WS_THICKFRAME snap contract (see ShellWindow's
ctor and WM_NCCALCSIZE handling): the system move loop only snaps windows
that are BOTH maximizable and resizable, and only for real HTCAPTION drags
(NC hit-test path). OLE tab drags are covered by the SnapZone unit tests.

Usage (either mode):
    python scripts/e2e_snap.py --launch <path-to-host-exe>
    python scripts/e2e_snap.py --attach <exe-name-substring>

Exits 0 when snap works, 1 otherwise. Windows only. Must run DPI-aware
(set up internally). The window is restored (SW_RESTORE) before exit; a
process started via --launch is terminated.
"""
import argparse
import ctypes
import ctypes.wintypes as wt
import subprocess
import sys
import time

user32 = ctypes.windll.user32
kernel32 = ctypes.windll.kernel32

GWL_STYLE = -16
WS_MAXIMIZEBOX = 0x00010000
WS_THICKFRAME = 0x00040000
WM_NCHITTEST = 0x0084
HTCAPTION = 2
SW_RESTORE = 9
MONITOR_DEFAULTTONEAREST = 2

INPUT_MOUSE = 0
MOUSEEVENTF_MOVE = 0x0001
MOUSEEVENTF_LEFTDOWN = 0x0002
MOUSEEVENTF_LEFTUP = 0x0004
MOUSEEVENTF_ABSOLUTE = 0x8000
MOUSEEVENTF_VIRTUALDESK = 0x4000
SM_XVIRTUALSCREEN = 76
SM_YVIRTUALSCREEN = 77
SM_CXVIRTUALSCREEN = 78
SM_CYVIRTUALSCREEN = 79


class MOUSEINPUT(ctypes.Structure):
    _fields_ = [
        ("dx", wt.LONG),
        ("dy", wt.LONG),
        ("mouseData", wt.DWORD),
        ("dwFlags", wt.DWORD),
        ("time", wt.DWORD),
        ("dwExtraInfo", ctypes.POINTER(wt.ULONG)),
    ]


class INPUT(ctypes.Structure):
    class _U(ctypes.Union):
        _fields_ = [("mi", MOUSEINPUT)]

    _anonymous_ = ("u",)
    _fields_ = [("type", wt.DWORD), ("u", _U)]


class MONITORINFO(ctypes.Structure):
    _fields_ = [
        ("cbSize", wt.DWORD),
        ("rcMonitor", wt.RECT),
        ("rcWork", wt.RECT),
        ("dwFlags", wt.DWORD),
    ]


def fail(msg):
    print(f"FAIL: {msg}")
    sys.exit(1)


def exe_path_of(pid):
    h = kernel32.OpenProcess(0x1000, False, pid)  # PROCESS_QUERY_LIMITED_INFORMATION
    if not h:
        return ""
    try:
        buf = ctypes.create_unicode_buffer(512)
        size = wt.DWORD(512)
        if kernel32.QueryFullProcessImageNameW(h, 0, buf, ctypes.byref(size)):
            return buf.value
    finally:
        kernel32.CloseHandle(h)
    return ""


def find_main_window(name_sub):
    """Visible top-level window with a title, owned by a process whose exe
    path contains name_sub. Returns (hwnd, pid)."""
    best = []

    @ctypes.WINFUNCTYPE(ctypes.c_bool, wt.HWND, wt.LPARAM)
    def cb(hwnd, _l):
        pid = wt.DWORD()
        user32.GetWindowThreadProcessId(hwnd, ctypes.byref(pid))
        if not user32.IsWindowVisible(hwnd):
            return True
        title = ctypes.create_unicode_buffer(256)
        user32.GetWindowTextW(hwnd, title, 256)
        if not title.value:
            return True  # skip empty tool windows
        path = exe_path_of(pid.value)
        if name_sub.lower() in path.lower():
            best.append((hwnd, pid.value))
        return True

    user32.EnumWindows(cb, 0)
    if not best:
        return None, 0
    return best[0]


def send_mouse(flags, dx=0, dy=0):
    inp = INPUT(type=INPUT_MOUSE)
    inp.mi = MOUSEINPUT(dx, dy, 0, flags, 0, None)
    if user32.SendInput(1, ctypes.byref(inp), ctypes.sizeof(INPUT)) != 1:
        fail("SendInput failed")


def move_to(x, y):
    """Absolute move in virtual-screen pixels. One SendInput per point generates WM_MOUSEMOVE."""
    vx = user32.GetSystemMetrics(SM_XVIRTUALSCREEN)
    vy = user32.GetSystemMetrics(SM_YVIRTUALSCREEN)
    vw = max(1, user32.GetSystemMetrics(SM_CXVIRTUALSCREEN))
    vh = max(1, user32.GetSystemMetrics(SM_CYVIRTUALSCREEN))
    nx = int((x - vx) * 65535 / vw)
    ny = int((y - vy) * 65535 / vh)
    send_mouse(MOUSEEVENTF_MOVE | MOUSEEVENTF_ABSOLUTE | MOUSEEVENTF_VIRTUALDESK, nx, ny)


def nc_hit_test(hwnd, x, y):
    lparam = (y & 0xFFFF) << 16 | (x & 0xFFFF)
    return user32.SendMessageW(hwnd, WM_NCHITTEST, 0, lparam) & 0xFFFF


def find_caption_point(hwnd, rect):
    """Scan the title bar for an HTCAPTION point: right side (clear of tabs),
    bottom-up (deeper points sit further from the top resize band — grabbing
    close to the band makes the snap-drag behave like an edge drag). Falls
    back to smaller x offsets for narrow windows."""
    for dx in (200, 120, 60):
        x = max(rect.left + 20, rect.right - dx)
        for dy in range(36, 5, -1):
            if nc_hit_test(hwnd, x, rect.top + dy) == HTCAPTION:
                return x, rect.top + dy
    return None


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    mode = parser.add_mutually_exclusive_group(required=True)
    mode.add_argument("--launch", metavar="EXE", help="start this host exe and test it")
    mode.add_argument("--attach", metavar="NAME", help="attach to a running host (exe name substring)")
    parser.add_argument("--startup-wait", type=float, default=6.0, help="seconds to wait after --launch (default 6)")
    args = parser.parse_args()

    # Per-monitor DPI awareness: all coordinates must be true pixels, else the
    # OS virtualizes GetWindowRect/SendInput and the probe misses.
    DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2 = ctypes.c_void_p(-4)
    if not user32.SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2):
        print("WARN: SetProcessDpiAwarenessContext failed; coordinates may be virtualized")

    proc = None
    name = args.attach
    if args.launch:
        proc = subprocess.Popen([args.launch])
        name = args.launch
        time.sleep(args.startup_wait)

    try:
        hwnd, pid = find_main_window(name)
        if not hwnd:
            fail(f"no visible main window found for {name!r}")
        if args.launch:
            pid = proc.pid

        rect = wt.RECT()
        user32.GetWindowRect(hwnd, ctypes.byref(rect))
        style = user32.GetWindowLongW(hwnd, GWL_STYLE)
        has_max = bool(style & WS_MAXIMIZEBOX)
        has_thick = bool(style & WS_THICKFRAME)
        print(f"window hwnd=0x{hwnd:X} rect=({rect.left},{rect.top},{rect.right},{rect.bottom})")
        print(f"styles: WS_MAXIMIZEBOX={'YES' if has_max else 'NO'} WS_THICKFRAME={'YES' if has_thick else 'NO'}")
        if not (has_max and has_thick):
            fail("snap contract styles missing (need WS_MAXIMIZEBOX + WS_THICKFRAME)")
        if user32.IsZoomed(hwnd):
            fail("window already maximized before the probe; restore it first")

        point = find_caption_point(hwnd, rect)
        if not point:
            fail("no HTCAPTION point found in the title bar (hit-test contract broken?)")
        px, py = point
        print(f"caption point ({px},{py}) confirmed HTCAPTION")

        # Target: the top edge of the monitor the window is on (works on any
        # monitor, not just the primary).
        mi = MONITORINFO()
        mi.cbSize = ctypes.sizeof(MONITORINFO)
        hmon = user32.MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST)
        user32.GetMonitorInfoW(hmon, ctypes.byref(mi))
        top_y = mi.rcMonitor.top
        drag_x = px  # keep the grab column; no horizontal movement during drag

        time.sleep(0.3)
        move_to(px, py)
        time.sleep(0.2)
        send_mouse(MOUSEEVENTF_LEFTDOWN)
        time.sleep(0.25)
        steps = 20
        for i in range(1, steps + 1):
            ty = max(top_y + 1, py - int((py - (top_y + 1)) * i / steps))
            move_to(drag_x, ty)
            time.sleep(0.02)
        time.sleep(0.4)
        send_mouse(MOUSEEVENTF_LEFTUP)
        time.sleep(0.8)

        zoomed = bool(user32.IsZoomed(hwnd))
        after = wt.RECT()
        user32.GetWindowRect(hwnd, ctypes.byref(after))
        print(f"after drag: zoomed={zoomed} rect=({after.left},{after.top},{after.right},{after.bottom})")
        print(f"monitor work area: ({mi.rcWork.left},{mi.rcWork.top},{mi.rcWork.right},{mi.rcWork.bottom})")

        # Restore so repeated runs / the user's desktop are unaffected.
        user32.ShowWindow(hwnd, SW_RESTORE)

        if not zoomed:
            fail("dragging the caption to the screen top did NOT maximize")
        if (after.left, after.top, after.right, after.bottom) != (
            mi.rcWork.left,
            mi.rcWork.top,
            mi.rcWork.right,
            mi.rcWork.bottom,
        ):
            print("WARN: maximized rect differs from the monitor work area")
        print("RESULT: SNAP WORKS")
        sys.exit(0)
    finally:
        if proc and proc.poll() is None:
            proc.terminate()


if __name__ == "__main__":
    main()
