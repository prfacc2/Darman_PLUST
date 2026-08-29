#!/bin/bash
# Build a DEBUG exe (AZ_DEBUG_BUILD) that can jump straight to a screen,
# run it under Wine + Xvfb, and grab a PNG screenshot.
# Usage: ./shot.sh [screen] [out.png] [WxH]
set -e
cd "$(dirname "$0")"
SCREEN="${1:-home}"     # home / reception / manage / settings
OUT="${2:-shots/shot_${SCREEN}.png}"
RESO="${3:-1600x900}"   # screen resolution for Xvfb
CXX=i686-w64-mingw32-g++
RES=i686-w64-mingw32-windres
mkdir -p obj shots
# NOTE: keep this source/lib list in sync with build.sh (v1.78.0: resynced —
# blacklist, web_admission, web_pages, web_thread_pool, web_ping_api, web_crm
# and insurance_defs were missing, so the debug exe could not link).
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
      src/web_crm.cpp src/insurance_defs.cpp src/clinic_ops.cpp \
      src/accounting.cpp"
if [ ! -f build/DarmanPlus_dbg.exe ] || [ -n "$FORCE_BUILD" ]; then
  $RES -O coff -i src/app.rc -o obj/app.res
  $CXX -std=c++17 -O2 -municode -mwindows -DAZ_DEBUG_BUILD -DAZ_DEBUG_LOGS=1 \
      -DUNICODE -D_UNICODE -D_WIN32_IE=0x0700 \
      -static -static-libgcc -static-libstdc++ \
      -Wall -Wno-unused-variable -Wno-unused-parameter \
      -Wno-misleading-indentation -Wno-unused-function \
      -Wno-missing-field-initializers \
      $SRCS obj/app.res \
      -o build/DarmanPlus_dbg.exe \
      -lcomctl32 -lcomdlg32 -lgdi32 -lgdiplus -lmsimg32 -ldwmapi -luxtheme \
      -luser32 -lshlwapi -lwininet -ladvapi32 -lshell32 -lwinspool \
      -lole32 -loleaut32 -luuid -lversion -lwinmm -ldbghelp \
      -lwinhttp -lurlmon -lcrypt32 -lwintrust -lwtsapi32 -lpsapi -lws2_32
  i686-w64-mingw32-strip build/DarmanPlus_dbg.exe
fi

export WINEDEBUG=-all
export AZ_DEBUG_SCREEN="$SCREEN"
export DISPLAY=:99
if ! pgrep -f "Xvfb :99" >/dev/null; then
  Xvfb :99 -screen 0 ${RESO}x24 >/tmp/xvfb.log 2>&1 &
  sleep 2
fi
wine build/DarmanPlus_dbg.exe >/dev/null 2>&1 &
WPID=$!
sleep "${SHOT_WAIT:-10}"
import -window root "$OUT" 2>/dev/null || xwd -root -silent | convert xwd:- "$OUT" 2>/dev/null || true
kill $WPID 2>/dev/null || true
wineserver -k 2>/dev/null || true
sleep 0.5
echo "Saved $OUT"
ls -la "$OUT" 2>/dev/null || echo "NO SCREENSHOT"
