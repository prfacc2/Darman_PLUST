// ============================================================================
//  clinic_ops.h — cashier tickets, shifts, work calendar, HTML backup (v1.82)
// ============================================================================
#pragma once
#include "app.h"
#include "sections.h"
#include <string>
#include <vector>

struct CashTicket {
    std::wstring id, barcode, nid, first, last, doctor;
    int sectionId, subId;
    std::wstring sectionName, subName;
    long long payable, paid;
    std::wstring paidAt, user, jdate, time;
    long long epochMin;
    std::wstring servicesJson;
    std::wstring paidUser;
    std::wstring mobile, fileNo, archiveNo, insBase, insSupp, receiptNo;
    std::wstring apptDate, turn, shift, status;
    std::wstring cancelReason, cancelUser, cancelAt;
    CashTicket():sectionId(0),subId(0),payable(0),paid(0),epochMin(0){}
};

struct CashShift {
    std::wstring id, username, fullname;
    int sectionId, subId;
    std::wstring sectionName, subName;
    std::wstring startJalali, startTime, endJalali, endTime;
    long long income;
    std::wstring status;          // open | closed
    long long startEpoch, endEpoch;
    std::wstring jdate;           // accumulation day (Jalali) the income belongs to
    long long carryIncome;        // income carried over from a previous same-day shift
    CashShift():sectionId(0),subId(0),income(0),startEpoch(0),endEpoch(0),
                carryIncome(0){}
};

struct CashScope {
    bool supervisor;
    int  homeSectionId;
    int  homeSubId;
    std::wstring homeSectionName, homeSubName;
    bool canView;
    CashScope():supervisor(false),homeSectionId(0),homeSubId(0),
                canView(false){}
};

// Reception kind: section.kind=="reception" OR name_fa contains «پذیرش».
// Never hard-codes a clinic department name.
bool Ops_IsReception(const Section& s);

CashScope Cash_ResolveScope();

bool Cash_CreateFromReception(const ReceptionRecord& r, std::wstring& err);
bool Cash_Pay(const std::wstring& id, std::wstring& err);
bool Cash_Manual(const std::wstring& nid, const std::wstring& first,
                 const std::wstring& last, const std::wstring& doctor,
                 long long amount, std::wstring& err);

// q filters barcode/name/doctor/section inside the active tab only.
// tabSectionId==0 → «صندوق نرفته‌ها» (unpaid). Otherwise a top-level section id.
std::string Cash_PageJson(const std::wstring& q, int tabSectionId);
std::string Cash_GetJson(const std::wstring& id);
std::wstring Cash_LookupId(const std::wstring& nid, const std::wstring& barcode, bool unpaidOnly);

bool Shift_Start(std::wstring& err);
bool Shift_End(std::wstring& err);
std::string Shift_StatusJson();
std::string Calendar_ListJson(const std::wstring& fromJalali,
                              const std::wstring& toJalali,
                              const std::wstring& username);

// HTML backup — self-contained AZTBKP01 walker. Does NOT touch the native
// backup-manager modal workers (s_bs / s_bk).
std::string OpsBackup_PickSave();
std::string OpsBackup_PickOpen();
std::string OpsBackup_Create(const std::wstring& destPath);
std::string OpsBackup_Analyze(const std::wstring& srcPath);
std::string OpsBackup_Restore(const std::wstring& srcPath);
std::string OpsBackup_ProgressJson();

struct ReceiptQuery {
    std::wstring q, from, to, first, last, nid, mobile;
    std::wstring fileNo, archive, barcode, doctor;
    int sectionId;
    bool onlyUser, byAppt;
    ReceiptQuery():sectionId(0),onlyUser(false),byAppt(false){}
};
std::string Receipt_SearchJson(const ReceiptQuery& q);
bool Receipt_DeleteMany(const std::vector<std::wstring>& ids, std::wstring& err);
bool Receipt_BuildRecord(const std::wstring& id, ReceptionRecord& out);
bool Receipt_Cancel(const std::wstring& id, const std::wstring& reason, std::wstring& err);
std::string Receipt_SectionsJson();
