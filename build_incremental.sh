#!/bin/bash
# ============================================================================
#  DarmanPlus INCREMENTAL build (low-memory, per-file compilation)
#  Compiles each .cpp -> obj/*.o one at a time so peak RAM stays low on
#  memory-constrained build hosts, then links to build/DarmanPlus.exe.
#  Functionally identical output to build.sh (same flags/libs).
# ============================================================================
set -e
cd "$(dirname "$0")"

CXX=i686-w64-mingw32-g++
RES=i686-w64-mingw32-windres

mkdir -p build obj

CXXFLAGS="-std=c++17 -O2 -municode -mwindows \
    -DUNICODE -D_UNICODE -D_WIN32_IE=0x0700 \
    -Wall -Wextra -Werror \
    -Wno-unused-variable -Wno-unused-parameter \
    -Wno-misleading-indentation -Wno-unused-function \
    -Wno-missing-field-initializers"

LIBS="-lcomctl32 -lcomdlg32 -lgdi32 -lgdiplus -lmsimg32 -ldwmapi -luxtheme \
    -luser32 -lshlwapi -lwininet -ladvapi32 -lshell32 -lwinspool \
    -lole32 -loleaut32 -luuid -lversion -lwinmm -ldbghelp \
    -lwinhttp -lurlmon -lcrypt32 -lwintrust -lwtsapi32 -lpsapi"

SRCS="main util handlers theme users billing calculator dialogs update \
      admin reception gdiplus settings printer employees data_ext appointment \
      backup ui_kit backup_analyzer backup_log sections print_designer \
      user_settings net_sync profile_requests backup_log_viewer backup_mtf \
      saved_messages setup_splash web_designer services clinic_ops accounting"

echo "[1/3] Compiling resources..."
$RES -O coff -i src/app.rc -o obj/app.res

echo "[2/3] Compiling C++ (per-file, low-mem)..."
OBJS=""
for s in $SRCS; do
    src="src/$s.cpp"
    obj="obj/$s.o"
    # Recompile only when source is newer than object (incremental).
    if [ ! -f "$obj" ] || [ "$src" -nt "$obj" ]; then
        echo "    cc $src"
        $CXX $CXXFLAGS -c "$src" -o "$obj"
    else
        echo "    -- $obj up-to-date"
    fi
    OBJS="$OBJS $obj"
done

echo "[3/3] Linking..."
$CXX -municode -mwindows \
    -static -static-libgcc -static-libstdc++ \
    $OBJS obj/app.res \
    -o build/DarmanPlus.exe \
    $LIBS

i686-w64-mingw32-strip build/DarmanPlus.exe

if command -v sha256sum >/dev/null 2>&1; then
    ( cd build && printf '%s  DarmanPlus.exe\n' "$(sha256sum DarmanPlus.exe | awk '{print $1}')" > DarmanPlus.exe.sha256 )
    echo "SHA-256 -> build/DarmanPlus.exe.sha256"
fi

ls -lh build/DarmanPlus.exe
echo "Build OK -> build/DarmanPlus.exe"
