#!/bin/bash
# ============================================================================
#  DarmanPlus build script (cross-compile from Linux with MinGW-w64)
#  Output: build/DarmanPlus.exe  — single 32-bit exe that runs on BOTH
#  x86 and x64 Windows (7 / 8 / 8.1 / 10 / 11+), fully static (no DLLs).
# ============================================================================
set -e
cd "$(dirname "$0")"

CXX=i686-w64-mingw32-g++
RES=i686-w64-mingw32-windres

mkdir -p build obj

# ----------------------------------------------------------------------------
#  v1.91.0: UI CONTRACT GUARD — runs BEFORE the compiler.
#  This program has many screens and is developed one release at a time, so
#  releases used to silently delete or break other screens' elements. The
#  contract is now executable: scripts/check_ui_contract.py asserts the embedded
#  asset registry agrees across app.rc / the inliner / index.html, that every
#  surface still has the element ids its JavaScript needs, that the stylesheets
#  stay renderable on BOTH engines (WebView2 + MSHTML/Trident), that the theme
#  layering is not restacked, and that the rejected colour families stay out.
#  A red contract is a red build — see AGENTS.md.
# ----------------------------------------------------------------------------
echo "[0/3] Checking UI contract..."
if command -v python3 >/dev/null 2>&1; then
    python3 scripts/check_ui_contract.py
else
    echo "[0/3] python3 not available — UI contract NOT verified." >&2
fi

echo "[1/3] Compiling resources..."
$RES -O coff -i src/app.rc -o obj/app.res

echo "[2/3] Compiling C++..."
# Release 1.4.0 sources (new: sections, print_designer*, user_settings,
# net_sync, profile_requests, backup_log_viewer).
# v1.17.0: the HTML/CSS/JS (MSHTML) presentation host has been RETIRED. The
# reception / appointment UI is now rendered 100% in native C++ (Win32/GDI), so
# src/webhost.cpp (and its webhost_*.inc includes + embedded HTML/CSS/JS in
# webhost_assets.inc) are no longer compiled. The print designer also uses its
# native GDI editor; src/web_designer.cpp now provides JSON compatibility only.
SRCS="src/main.cpp src/util.cpp src/handlers.cpp src/theme.cpp src/users.cpp \
      src/billing.cpp src/calculator.cpp src/dialogs.cpp src/update.cpp \
      src/admin.cpp src/reception.cpp src/gdiplus.cpp src/settings.cpp \
      src/printer.cpp src/employees.cpp src/data_ext.cpp \
      src/backup.cpp src/persons.cpp src/ui_kit.cpp src/backup_analyzer.cpp \
      src/backup_log.cpp src/sections.cpp src/print_designer.cpp \
      src/user_settings.cpp src/net_sync.cpp src/profile_requests.cpp \
      src/backup_log_viewer.cpp src/backup_mtf.cpp src/saved_messages.cpp \
      src/setup_splash.cpp src/web_designer.cpp src/services.cpp \
      src/blacklist.cpp src/web_admission.cpp \
      src/web_pages.cpp src/web_thread_pool.cpp src/web_ping_api.cpp \
      src/web_crm.cpp src/insurance_defs.cpp src/clinic_ops.cpp"

$CXX -std=c++17 -O2 -s -municode -mwindows \
    -DUNICODE -D_UNICODE -D_WIN32_IE=0x0700 \
    -static -static-libgcc -static-libstdc++ \
    -Wall -Wextra -Werror \
    -Wno-unused-variable -Wno-unused-parameter \
    -Wno-misleading-indentation -Wno-unused-function \
    -Wno-missing-field-initializers \
    $SRCS obj/app.res \
    -o build/DarmanPlus.exe \
    -lcomctl32 -lcomdlg32 -lgdi32 -lgdiplus -lmsimg32 -ldwmapi -luxtheme \
    -luser32 -lshlwapi -lwininet -ladvapi32 -lshell32 -lwinspool \
    -lole32 -loleaut32 -luuid -lversion -lwinmm -ldbghelp \
    -lwinhttp -lurlmon -lcrypt32 -lwintrust -lwtsapi32 -lpsapi -lws2_32

echo "[3/3] Stripping..."
i686-w64-mingw32-strip build/DarmanPlus.exe

# Drop a SHA-256 sidecar next to the exe (used by the in-app updater / verify).
if command -v sha256sum >/dev/null 2>&1; then
    ( cd build && printf '%s  DarmanPlus.exe\n' "$(sha256sum DarmanPlus.exe | awk '{print $1}')" > DarmanPlus.exe.sha256 )
    echo "SHA-256 -> build/DarmanPlus.exe.sha256"
fi

ls -lh build/DarmanPlus.exe
echo "Build OK -> build/DarmanPlus.exe"

# ----------------------------------------------------------------------------
# Optional bounded debug smokes. AZ_SMOKE=1 builds a separate AZ_DEBUG_BUILD
# binary, then exercises the print-designer path plus admission inline/view/JS
# readiness and admission keyboard routing under Wine. Production output above
# is unchanged; every run is timeout-bounded so CI cannot hang on a bad host.
# ----------------------------------------------------------------------------
if [ -n "$AZ_SMOKE" ]; then
    echo "[smoke] Building debug binary for print_designer smoke test..."
    mkdir -p obj
    $CXX -std=c++17 -O2 -municode -mwindows \
        -DUNICODE -D_UNICODE -D_WIN32_IE=0x0700 -DAZ_DEBUG_BUILD \
        -static -static-libgcc -static-libstdc++ \
        -Wall -Wextra -Werror \
        -Wno-unused-variable -Wno-unused-parameter \
        -Wno-misleading-indentation -Wno-unused-function \
        -Wno-missing-field-initializers \
        $SRCS obj/app.res \
        -o build/DarmanPlus_smoke.exe \
        -lcomctl32 -lcomdlg32 -lgdi32 -lgdiplus -lmsimg32 -ldwmapi -luxtheme \
        -luser32 -lshlwapi -lwininet -ladvapi32 -lshell32 -lwinspool \
        -lole32 -loleaut32 -luuid -lversion -lwinmm -ldbghelp \
        -lwinhttp -lurlmon -lcrypt32 -lwintrust -lwtsapi32 -lpsapi -lws2_32
    if command -v wine >/dev/null 2>&1 && command -v timeout >/dev/null 2>&1; then
        WINE_RUN=(wine)
        if command -v xvfb-run >/dev/null 2>&1; then WINE_RUN=(xvfb-run -a wine); fi
        run_smoke() {
            local screen="$1" label="$2"
            echo "[smoke] Running $label under Wine..."
            set +e
            AZ_DEBUG_SCREEN="$screen" timeout --signal=KILL 30s "${WINE_RUN[@]}" build/DarmanPlus_smoke.exe
            local rc=$?
            set -e
            if [ "$rc" -ne 0 ]; then
                echo "[smoke] FAILED: $label exited $rc" >&2
                exit "$rc"
            fi
            echo "[smoke] $label PASSED"
        }
        run_smoke print_designer "print_designer open/close path"
        run_smoke admission_probe "admission inline/view/bridge probe"
        run_smoke admission_keys "admission keyboard routing"
    else
        echo "[smoke] Wine/timeout not available — debug binary built but not executed."
    fi
    rm -f build/DarmanPlus_smoke.exe
fi
