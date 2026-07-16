#!/usr/bin/env python3
"""Configure and build MultiProcessShell (out-of-source).

Skips cmake reconfigure when the existing build tree already matches the
requested options, so FetchContent protobuf/abseil are not rebuilt needlessly.

On Windows with demos enabled, runs scripts/deploy_demo.py after a successful
build so dist/Demo/mps_demo_host.exe is double-clickable.
"""

from __future__ import annotations

import argparse
import os
import re
import shutil
import subprocess
import sys
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]


def which_or_die(name: str) -> str:
    path = shutil.which(name)
    if not path:
        raise SystemExit(f"error: '{name}' not found on PATH")
    return path


def run(cmd: list[str], cwd: Path | None = None) -> None:
    print("+", " ".join(cmd), flush=True)
    subprocess.check_call(cmd, cwd=cwd)


def _default_ninja() -> str | None:
    candidates = [
        Path(os.environ.get("ProgramFiles", r"C:\Program Files"))
        / "Microsoft Visual Studio/18/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/Ninja/ninja.exe",
        Path(os.environ.get("ProgramFiles", r"C:\Program Files"))
        / "Microsoft Visual Studio/2022/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/Ninja/ninja.exe",
    ]
    for c in candidates:
        if c.is_file():
            return str(c)
    return shutil.which("ninja")


def _find_cl() -> str | None:
    # Prefer bare name from vcvars PATH — avoids Hostx64/HostX64 absolute-path thrash
    # that forces CMake to wipe the cache and rebuild protobuf.
    if shutil.which("cl") or shutil.which("cl.exe"):
        return "cl"
    vswhere = (
        Path(os.environ.get("ProgramFiles(x86)", r"C:\Program Files (x86)"))
        / "Microsoft Visual Studio/Installer/vswhere.exe"
    )
    if not vswhere.is_file():
        return None
    try:
        root = subprocess.check_output(
            [
                str(vswhere),
                "-latest",
                "-products",
                "*",
                "-requires",
                "Microsoft.VisualStudio.Component.VC.Tools.x86.x64",
                "-property",
                "installationPath",
            ],
            text=True,
        ).strip()
    except (OSError, subprocess.CalledProcessError):
        return None
    if not root:
        return None
    msvc = Path(root) / "VC" / "Tools" / "MSVC"
    if not msvc.is_dir():
        return None
    versions = sorted([p for p in msvc.iterdir() if p.is_dir()], reverse=True)
    for ver in versions:
        candidate = ver / "bin" / "Hostx64" / "x64" / "cl.exe"
        if candidate.is_file():
            return candidate.resolve().as_posix()
    return None


def _read_cache(cache_file: Path) -> dict[str, str]:
    """Parse CMakeCache.txt into NAME -> VALUE (TYPE stripped)."""
    out: dict[str, str] = {}
    if not cache_file.is_file():
        return out
    line_re = re.compile(r"^([A-Za-z0-9_./:-]+):([A-Z]+)=(.*)$")
    for raw in cache_file.read_text(encoding="utf-8", errors="ignore").splitlines():
        if not raw or raw.startswith("//") or raw.startswith("#"):
            continue
        m = line_re.match(raw)
        if m:
            out[m.group(1)] = m.group(3)
    return out


def _bool_on(value: str | None) -> bool:
    return (value or "").upper() in {"ON", "TRUE", "1", "YES"}


def _norm_path(p: str | None) -> str:
    if not p:
        return ""
    try:
        return str(Path(p).resolve()).replace("\\", "/").rstrip("/").lower()
    except OSError:
        return p.replace("\\", "/").rstrip("/").lower()


def _needs_configure(
    build_dir: Path,
    *,
    want_tests: bool,
    want_demos: bool,
    want_src: bool,
    build_type: str,
    qtdir: str | None,
    force: bool,
) -> tuple[bool, str]:
    if force:
        return True, "forced (--fresh/--reconfigure)"
    cache_file = build_dir / "CMakeCache.txt"
    build_ninja = build_dir / "build.ninja"
    if not cache_file.is_file():
        return True, "no CMakeCache.txt"
    if not build_ninja.is_file() and not (build_dir / "Makefile").is_file():
        # Multi-config generators may not have build.ninja; still require a generator stamp.
        if not any(build_dir.glob("CMakeFiles/*/CMakeCXXCompiler.cmake")):
            return True, "incomplete build tree"

    cache = _read_cache(cache_file)
    checks = [
        ("MPS_BUILD_TESTS", want_tests, _bool_on(cache.get("MPS_BUILD_TESTS"))),
        ("MPS_BUILD_DEMOS", want_demos, _bool_on(cache.get("MPS_BUILD_DEMOS"))),
        ("MPS_BUILD_SRC", want_src, _bool_on(cache.get("MPS_BUILD_SRC"))),
    ]
    for name, want, have in checks:
        if want != have:
            return True, f"{name} want={'ON' if want else 'OFF'} have={'ON' if have else 'OFF'}"

    have_type = (cache.get("CMAKE_BUILD_TYPE") or "").strip()
    # Multi-config generators leave CMAKE_BUILD_TYPE empty — ignore then.
    if have_type and have_type.lower() != build_type.lower():
        return True, f"CMAKE_BUILD_TYPE want={build_type} have={have_type}"

    if want_demos and qtdir:
        have_prefix = cache.get("CMAKE_PREFIX_PATH") or cache.get("Qt6_DIR") or ""
        # Qt6_DIR looks like <prefix>/lib/cmake/Qt6 — accept either prefix match.
        np_q = _norm_path(qtdir)
        np_p = _norm_path(have_prefix)
        if np_q and np_p and np_q not in np_p and np_p not in np_q:
            # Still OK if Qt was found via CMAKE_PREFIX_PATH containing qtdir as one entry
            entries = [_norm_path(x) for x in have_prefix.replace(";", os.pathsep).split(os.pathsep)]
            if not any(np_q == e or np_q in e or e in np_q for e in entries if e):
                # Don't force reconfigure solely on prefix formatting noise if demos already on
                # and Qt was previously found (MPS / Qt6 version present).
                if "Qt6_DIR" not in cache and "MPS_QT_VERSION" not in "".join(cache.keys()):
                    return True, "CMAKE_PREFIX_PATH / QTDIR mismatch"

    return False, "cache matches requested options"


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--build-dir",
        type=Path,
        default=ROOT / "build",
        help="Out-of-source build directory (default: ./build)",
    )
    parser.add_argument(
        "--config",
        default=os.environ.get("MPS_BUILD_CONFIG", "Release"),
        help="Build type / MSVC config (default: Release)",
    )
    parser.add_argument("-G", "--generator", default="Ninja", help="CMake generator")
    parser.add_argument(
        "--fresh",
        action="store_true",
        help="Wipe and reconfigure (cmake --fresh); rebuilds deps including protobuf",
    )
    parser.add_argument(
        "--reconfigure",
        action="store_true",
        help="Force cmake configure even if the cache already matches",
    )
    parser.add_argument(
        "-D",
        dest="defs",
        action="append",
        default=[],
        help="Extra -D CMake definitions (repeatable); implies reconfigure",
    )
    parser.add_argument(
        "--no-tests",
        action="store_true",
        help="Configure with MPS_BUILD_TESTS=OFF",
    )
    parser.add_argument(
        "--no-demos",
        action="store_true",
        help="Disable MPS_BUILD_DEMOS and MPS_BUILD_SRC (protocol tests only; no QTDIR)",
    )
    parser.add_argument(
        "--configure-only",
        action="store_true",
        help="Only run cmake configure",
    )
    parser.add_argument(
        "--test",
        action="store_true",
        help="Run ctest after build",
    )
    args = parser.parse_args()

    which_or_die("cmake")
    qtdir = os.environ.get("QTDIR")
    if qtdir:
        qtdir = str(Path(qtdir))

    build_dir: Path = args.build_dir
    build_dir.mkdir(parents=True, exist_ok=True)

    want_tests = not args.no_tests
    want_demos = not args.no_demos
    want_src = not args.no_demos

    if want_demos and not qtdir:
        raise SystemExit("error: QTDIR is required to build demos (or pass --no-demos)")

    force_cfg = bool(args.fresh or args.reconfigure or args.defs)
    need_cfg, reason = _needs_configure(
        build_dir,
        want_tests=want_tests,
        want_demos=want_demos,
        want_src=want_src,
        build_type=args.config,
        qtdir=qtdir,
        force=force_cfg,
    )

    if need_cfg:
        print(f"cmake configure: {reason}", flush=True)
        cmake_cmd = [
            "cmake",
            "-S",
            str(ROOT),
            "-B",
            str(build_dir),
            "-G",
            args.generator,
            f"-DCMAKE_BUILD_TYPE={args.config}",
            "-DMPS_FETCH_PROTOBUF=ON",
            "-DFETCHCONTENT_UPDATES_DISCONNECTED=ON",
            f"-DMPS_BUILD_TESTS={'ON' if want_tests else 'OFF'}",
            f"-DMPS_BUILD_DEMOS={'ON' if want_demos else 'OFF'}",
            f"-DMPS_BUILD_SRC={'ON' if want_src else 'OFF'}",
        ]

        if args.generator.lower() == "ninja":
            ninja = _default_ninja()
            if not ninja:
                raise SystemExit("error: ninja not found (required for -G Ninja)")
            cmake_cmd.append(f"-DCMAKE_MAKE_PROGRAM={Path(ninja).as_posix()}")

        if os.name == "nt":
            cl = _find_cl()
            if not cl:
                raise SystemExit(
                    "error: MSVC cl.exe not found. Run from an x64 Native Tools / vcvars shell."
                )
            # Bare "cl" keeps the cache stable across runs; absolute only as fallback.
            if cl == "cl":
                cmake_cmd.extend(
                    [
                        "-DCMAKE_C_COMPILER=cl",
                        "-DCMAKE_CXX_COMPILER=cl",
                    ]
                )
                os.environ["CC"] = "cl"
                os.environ["CXX"] = "cl"
            else:
                cmake_cmd.extend(
                    [
                        f"-DCMAKE_C_COMPILER={cl}",
                        f"-DCMAKE_CXX_COMPILER={cl}",
                    ]
                )
                os.environ["CC"] = cl
                os.environ["CXX"] = cl
                os.environ["PATH"] = (
                    str(Path(cl).parent) + os.pathsep + os.environ.get("PATH", "")
                )

            rc = shutil.which("rc") or shutil.which("rc.exe")
            mt = shutil.which("mt") or shutil.which("mt.exe")
            if rc:
                cmake_cmd.append(f"-DCMAKE_RC_COMPILER={Path(rc).resolve().as_posix()}")
            if mt:
                cmake_cmd.append(f"-DCMAKE_MT={Path(mt).resolve().as_posix()}")

        if qtdir:
            cmake_cmd.append(f"-DCMAKE_PREFIX_PATH={Path(qtdir).as_posix()}")
        if args.fresh:
            cmake_cmd.append("--fresh")
        for d in args.defs:
            cmake_cmd.append(f"-D{d}")

        run(cmake_cmd)
    else:
        print(f"cmake configure: skipped ({reason})", flush=True)

    if args.configure_only:
        return 0

    build_cmd = ["cmake", "--build", str(build_dir), "--config", args.config]
    run(build_cmd)

    if want_demos and os.name == "nt":
        deploy = ROOT / "scripts" / "deploy_demo.py"
        if deploy.is_file():
            run(
                [
                    sys.executable,
                    str(deploy),
                    "--build-dir",
                    str(build_dir),
                    "--config",
                    args.config,
                ]
            )

    if args.test and want_tests:
        which_or_die("ctest")
        run(
            [
                "ctest",
                "--test-dir",
                str(build_dir),
                "--output-on-failure",
                "-C",
                args.config,
            ]
        )

    print("OK: build finished", flush=True)
    return 0


if __name__ == "__main__":
    sys.exit(main())
