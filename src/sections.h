// ============================================================================
//  sections.h — clinic Sections / Departments registry (release 1.4.0, §2)
//  A single source of truth every other module reads from (print designer,
//  appointment counters, settings). File-backed (data\sections.dat) so the
//  program stays a single static EXE, ready to migrate to a real DB later.
// ============================================================================
#pragma once
#include <string>
#include <vector>

struct Section {
    int          id;
    std::wstring code;       // short stable code, e.g. "REC01"
    std::wstring name_fa;
    std::wstring kind;       // reception|injection|lab|radiology|physio|other
    int          is_active;
    std::wstring created_at, updated_at;
    // §7 (1.14.0): stable, durable identity + network metadata. `net_meta` is an
    // opaque, serializable string reserved for future network sync (node id,
    // remote mapping key, …). It is optional in storage so older data files load
    // unchanged. Routing/binding (print designer, future sync) must key off the
    // STABLE category prefix + id/code, never the display name (`name_fa`).
    std::wstring net_meta;
    // v1.74: subsection support. parent_id > 0 means this row is a زیربخش that
    // belongs to the section whose id == parent_id; 0 means it is a top-level
    // section. Stored as an optional 9th column so older files load unchanged.
    int          parent_id;
    // v1.93: «ثبت پذیرش در صندوق». cashier_tab=1 means this section appears as a
    // tab in the cashier (صندوق) page; default 0/OFF so existing sections do not
    // surface as cashier tabs until the manager enables it. Stored as an optional
    // 10th column so older files load unchanged (cashier_tab stays 0).
    int          cashier_tab;
    Section():id(0),is_active(1),parent_id(0),cashier_tab(0){}
};

// Stable durable CATEGORY codes (§7). Section display names may change; these
// short prefixes never do. The full per-section `code` is "<CAT><nn>"
// (e.g. REC01). These are the canonical keys for routing + network sync.
//   REC reception · APR appointment · LAB laboratory · INJ injection
//   PHR pharmacy   · BIL billing     · RAD radiology  · PHY physiotherapy
// Map a section `kind` to its stable 3-letter category code.
const wchar_t* Sections_CategoryCode(const std::wstring& kind);
// Extract the stable category prefix (leading alpha run) from a section code,
// e.g. "REC01" -> "REC". Falls back to deriving it from `kind` when the code
// has no alpha prefix. Always returns a stable, durable key (never a name).
std::wstring   Sections_CodePrefix(const Section& s);

// Ensure the store exists and is seeded on first run (idempotent).
void Sections_Init();

// All sections (active + inactive), id-ascending.
int  Sections_All(std::vector<Section>& out);

// Find by code (case-insensitive) OR name_fa (Persian-normalized): prefix or
// substring match. Empty query returns all.
int  Sections_Find(const std::wstring& query, std::vector<Section>& out);

// Insert (id<=0) or update (id>0). Returns the row id (>0) or 0 on failure.
int  Sections_Upsert(const Section& s);

// Delete by id. Returns 1 on success.
int  Sections_Delete(int id);

// Localized label for a kind code.
const wchar_t* Sections_KindLabel(const std::wstring& kind);
