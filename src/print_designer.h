// ============================================================================
//  print_designer.h — full vector print-design editor
//  WYSIWYG canvas, draggable/resizable/rotatable items, 30 built-in templates,
//  per-section design binding, .aztpl export/import and undo/redo.
//  File-backed data model keeps the EXE a single static binary.
// ============================================================================
#pragma once
#include <string>
#include <vector>

// --------------------------------------------------------------- item types --
enum PrintItemType {
    PIT_LABEL = 0,     // free text / static label
    PIT_FIELD = 1,     // data-bound text (field name in `field`)
    PIT_HLINE = 2,     // horizontal line
    PIT_VLINE = 3,     // vertical line
    PIT_RECT = 4,      // rectangle box
    PIT_FRAME = 5,     // full-page border (special hit-test, §3.10)
    PIT_IMAGE = 6,     // custom image (path or embedded base64)
    PIT_LOGO = 7,      // clinic logo (image)
    PIT_QR = 8,        // QR / barcode (encodes the receipt number)
    PIT_PHOTO = 9,     // patient personal photo placeholder
    // Numeric value 10 belonged to the removed appointment counter and is
    // intentionally never reused, preserving old persisted type ids.
    PIT_TABLE = 11,    // §1.21.0: grid/table. Model stored as JSON in `text`:
                       //   {"cols":n,"rows":n,"header":bool,"widths":[..],
                       //    "cells":[[..],..]}  — round-trips through C++ as a
                       //    plain string so it never breaks the data model.
    PIT_BARCODE = 13,  // v1.55.0: REAL 1-D barcode (Code128-B / Code39 /
                       //   EAN-13). The payload comes from the bound `field`
                       //   token (default {receiptbarcode}), so it always
                       //   encodes live record data — never a random number.
                       //   `text` may carry a JSON model:
                       //   {"sym":"code128"|"code39"|"ean13","hri":bool,
                       //    "quiet":mm}
    PIT_SERVICES       // §1.51.0: dynamic services list rendered from the live
                       //   ReceptionRecord.services vector at print/preview time.
                       //   Model stored as JSON in `text`:
                       //   {"cols":n,"header":bool,"widths":[..],"labels":[..]}
                       //   cols/labels/widths describe the table header; rows are
                       //   filled dynamically from r.services (variable count).
                       //   Falls back gracefully to a sensible default when the
                       //   JSON model is empty or the record has no services.
};

// Each PrintItem lives in millimetre space (paper coordinates).
struct PrintItem {
    int          id;
    int          type;          // PrintItemType
    double       x, y, w, h;    // mm
    double       rot;           // degrees
    bool         locked;
    bool         is_frame;      // true for PIT_FRAME (hit-test special)
    int          z;             // z-index (paint order)

    // text
    std::wstring text;          // label content / prefix display
    std::wstring field;         // data binding key (nationalCode, firstName, …)
    std::wstring prefix, suffix;
    std::wstring fmt;           // number/date format hint
    std::wstring fontName;
    double       fontPt;
    bool         bold, italic;
    int          align;         // 0=right 1=center 2=left 3=justify (RTL)
    int          dir;           // text direction: 0=RTL 1=LTR 2=center(auto). v1.22.0
    int          valign;        // vertical align: 0=top 1=middle 2=bottom. v1.23.0
    double       lineSpacing;

    // appearance
    unsigned int textColor;     // 0x00RRGGBB
    unsigned int fillColor;
    bool         fillTransparent;
    unsigned int borderColor;
    double       borderWidth;   // px
    double       corner;        // mm
    double       padding;       // mm
    double       opacity;       // 0..1
    int          visibility;    // 0=always 1=when_field_not_empty

    // image
    std::wstring imgPath;       // for PIT_IMAGE / PIT_LOGO
    int          objectFit;     // 0=contain (aspect-fit, no crop) 1=cover (fill,
                                //   crop overflow) 2=fill (stretch). v1.23.0

    // v1.55.0 — TABLE / SERVICES row geometry (millimetres). 0 = automatic
    // (divide the item height evenly, the pre-1.55 behaviour), so old saved
    // designs and .aztpl files keep printing byte-identically. When > 0 the
    // print engine and the browser designer both honour the explicit height,
    // which is what makes the live «ارتفاع سطر / ارتفاع سرستون» controls work.
    double       rowH;      // data-row height in mm  (0 = auto)
    double       headerH;   // header-row height in mm (0 = auto)

    PrintItem();
};

struct PrintDesign {
    int          id;            // 0 = unsaved
    std::wstring name;
    std::wstring kind;          // "builtin" | "user"
    std::wstring paper;         // "A4","A5","A6","B5","Letter","R80","R58","A3","custom"
    double       paperW, paperH;// mm (for custom / resolved)
    int          orientation;   // 0=portrait 1=landscape
    std::vector<PrintItem> items;
    PrintDesign();
};

// ---------------------------------------------------------------------------
//  v1.55.0 — DATA-BINDING TOKEN VOCABULARY (resolved in printer.cpp
//  pdFieldValue(); bare aliases normalised in pdNormalizeField()).
//  Every token below returns LIVE data from the ReceptionRecord / session /
//  settings store. None of them is ever randomised: when a value was not
//  captured at admission the token resolves to an EMPTY string so the design
//  prints cleanly (or hides the row when PrintItem::visibility == 1).
//
//   reception date / time
//     {date} {time} {datetime} {shift} {regdate} {regtime}
//     {reg_ts}        تاریخ و ساعت ثبت پذیرش
//   patient
//     {first} {last} {full} {father} {nid} {birth} {gender} {mobile}
//     {landline} {address} {ptype} {barcode} {nationalcard}
//     {age}           سن به‌صورت 10Y / 24Y (ارقام لاتین + Y)
//     {P-Name}        نام کامل بیمار (alias → {full})؛ نمایش دیزاینر [P-Name]
//     {receiptcode}   کد کوتاه رسید (deterministic از شمارهٔ قبض)
//   insurance
//     {ins} {supp} {insno} {insexp} {insidx}
//     {ins_percent}   درصد بیمهٔ پایه   (خالی وقتی نامعتبر)
//     {supp_percent}  درصد بیمهٔ مکمل  (خالی وقتی نامعتبر)
//     {ins_full}      بیمهٔ پایه + درصد
//     {supp_full}     بیمهٔ مکمل + درصد
//   doctor / service type
//     {doctor} {refdoctor} {dept} {room} {queue}
//     {doctorcode}    کد پزشک
//     {performer} {performercode}
//     {specialty}     شرح تخصص
//     {specialtycode} کد تخصص
//     {servicetype}   نوع خدمت (عمومی/تخصصی…)
//   money
//     {total} {insshare} {patientshare} {discount} {finaltotal} {paid}
//     {basepay}       سهم پایه
//     {supppay}       سهم مکمل
//     {cash} {pos} {discount_from}
//     {eprescription} کد رهگیری نسخهٔ الکترونیک
//     {referralno}    شماره معرفی‌نامه
//   reception / cashier
//     {user} {cashier} {issued} {shiftuser}
//     {receptionist}  نام پذیرش‌کننده
//     {cashier_name}  نام صندوق‌دار
//     {scnum}         ش.ص
//     {receiptNo}     شمارهٔ قبض
//     {receiptbarcode} بارکد اختصاصی رسید (deterministic)
// ---------------------------------------------------------------------------

// Resolve a paper preset name to mm dimensions (portrait). Returns false if
// the name is "custom" (caller keeps paperW/paperH).
bool Paper_Dims(const std::wstring& name, double& wmm, double& hmm);

// ---------------------------------------------------------- JSON (in-house) --
//  Minimal, self-contained serializer/parser for PrintDesign. Not a general
//  JSON library — only what the designer needs. Magic header "AZTEMPLATE/1".
std::string  Design_ToJson(const PrintDesign& d);
bool         Design_FromJson(const std::string& json, PrintDesign& out,
                             std::wstring& err);

// v1.21.1: the web designer (browser) downloads files in its own JS-shaped JSON
// (string item types, #rrggbb colours, no AZTEMPLATE magic). These let the
// native UI (Print Settings import) round-trip those same files.
std::string  Design_ToWebJson(const PrintDesign& d);
bool         Design_FromWebJson(const std::string& json, PrintDesign& out);

// ----------------------------------------------------------- design store ----
void Designs_Init();                       // seed the 30 built-ins on first run
// v1.65.0: build built-in template #idx (0..29) ENTIRELY IN MEMORY (no file
// I/O). Used by the print path as a services-capable last-resort fallback when
// the design store cannot be read/seeded, so receipts never degrade to the
// legacy label-only layout.
PrintDesign Design_BuiltinTemplate(int idx);
int  Designs_All(std::vector<PrintDesign>& out);
int  Designs_Builtins(std::vector<PrintDesign>& out);
int  Designs_User(std::vector<PrintDesign>& out);
int  Designs_Insert(const PrintDesign& d); // returns new id
bool Designs_Update(const PrintDesign& d);
bool Designs_Delete(int id);
bool Designs_Get(int id, PrintDesign& out);

// section <-> design binding
bool SectionDesign_Set(int sectionId, int designId);
int  SectionDesign_Get(int sectionId);     // designId or 0
// resolve the active design payload for a section (falls back to T01).
bool SectionDesign_Resolve(int sectionId, PrintDesign& out);
// §1.12.0 (§7): reconcile section<->design bindings with the live Sections
// registry — detach orphaned/stale mappings and archive orphaned design files.
// Returns the number of stale bindings/files removed.
int  SectionDesign_Cleanup();

// -------------------------------------------------------------- UI entries ---
//  Management → "دیزاین چاپگر": opens the section picker then the editor.
void PrintDesigner_Open(HWND hMain);
//  Management → "بازگردانی دیزاین چاپ": import an .aztpl and apply to sections.
void RestoreDesign_Open(HWND hMain);
//  §1.19.1 — «تنظیمات چاپ» (Print Settings): pick a section, preview its current
//  print design (enlargeable), download/upload an .aztpl, and apply it to the
//  section. Reachable from the management Settings menu.
void PrintCfg_Open(HWND hMain);
