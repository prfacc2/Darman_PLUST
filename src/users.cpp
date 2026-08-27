// ============================================================================
//  users.cpp — user store (data\users.dat), FNV-salted hash, login checks
//  Hidden admin: prf / prf123  (Ctrl+P+N on home screen)
// ============================================================================
#include "app.h"
#include <stdio.h>
#include <algorithm>

//  Format (UTF-8, one per line):  username|fullname|dept|role|hash
static std::wstring usersPath(){ return dataDir()+L"\\users.dat"; }

std::wstring hashPassword(const std::wstring& p){
    // FNV-1a 64-bit, double pass with salt — adequate for offline local store.
    // NOTE: the SALT is intentionally kept as the original AzadiTeb token so
    // existing user accounts (passwords hashed in data\users.dat) keep working
    // after the درمان پلاس rename. Do NOT change this string.
    const wchar_t* SALT = L"AzadiTeb#2025!";
    std::wstring s = SALT + p + SALT;
    unsigned long long h = 14695981039346656037ULL;
    for(wchar_t c : s){ h ^= (unsigned long long)c; h *= 1099511628211ULL; }
    std::wstring r = s; // second pass over reversed
    for(int i=(int)r.size()-1;i>=0;i--){ h ^= (unsigned long long)r[i]*31ULL; h *= 1099511628211ULL; }
    wchar_t buf[24]; swprintf(buf,24,L"%016llX",h);
    return buf;
}

static std::vector<std::wstring> split(const std::wstring& s, wchar_t sep){
    std::vector<std::wstring> out; size_t pos=0;
    while(true){
        size_t e = s.find(sep,pos);
        if(e==std::wstring::npos){ out.push_back(s.substr(pos)); break; }
        out.push_back(s.substr(pos,e-pos)); pos=e+1;
    }
    return out;
}

std::vector<User> loadUsers(){
    std::vector<User> out;
    std::wstring all = readFileUtf8(usersPath());
    size_t pos=0;
    while(pos < all.size()){
        size_t e = all.find(L'\n',pos);
        if(e==std::wstring::npos) e=all.size();
        std::wstring line = trim(all.substr(pos,e-pos));
        pos = e+1;
        if(line.empty()) continue;
        auto f = split(line, L'|');
        if(f.size() < 5) continue;
        User u; u.username=f[0]; u.fullname=f[1]; u.dept=f[2];
        u.role=_wtoi(f[3].c_str()); u.hash=f[4];
        // v1.79.0: column 6 = permission keys (comma-separated). Missing column
        // (old files) → empty perms → FULL access (nothing is taken away from
        // existing accounts).
        if(f.size() >= 6) u.perms = f[5];
        // v2.01 (Part F2): column 7 = assigned work shift (-1 = not assigned).
        if(f.size() >= 7) u.shift = _wtoi(f[6].c_str());
        // §H: preserve any extra columns a newer version may have appended so a
        // round-trip save never drops forward-compatible data.
        for(size_t i=7;i<f.size();i++){ u.extra+=L"|"; u.extra+=f[i]; }
        out.push_back(u);
    }
    return out;
}
static void saveUsers(const std::vector<User>& us){
    std::wstring out;
    for(auto& u : us){
        wchar_t role[4]; swprintf(role,4,L"%d",u.role);
        // v1.79.0: column 6 = permission keys (always emitted, may be empty);
        // v2.01: column 7 = work shift (Part F2);
        // §H: then any preserved forward-compat trailing columns (u.extra
        // already begins with its own '|' separators).
        wchar_t shiftStr[8]; swprintf(shiftStr,8,L"%d",u.shift);
        out += u.username+L"|"+u.fullname+L"|"+u.dept+L"|"+role+L"|"+u.hash
             + L"|"+u.perms+L"|"+shiftStr+u.extra+L"\r\n";
    }
    writeFileUtf8(usersPath(), out, false);
}

//  v1.79.0: permission check — empty perms = legacy full access; management
//  accounts bypass (their own login card already gates them).
bool userHasPerm(const User& u, const wchar_t* key){
    if(u.role>=1) return true;
    std::wstring p=trim(u.perms);
    if(p.empty()) return true;
    std::wstring want=key?key:L"";
    auto parts=split(p, L',');
    for(auto& part:parts){
        std::wstring k=trim(part);
        if(k==want) return true;
        // legacy "cashier" = both view and edit
        if(k==L"cashier" && (want==L"cashier_view" || want==L"cashier_edit"))
            return true;
        // asking for "cashier" matches either of the split ticks
        if(want==L"cashier" && (k==L"cashier_view" || k==L"cashier_edit"))
            return true;
    }
    return false;
}

//  v1.79.0: update ONLY the permission keys of an existing account (used by
//  the CRM «تعریف حساب کاربری» page). Keeps the canonical file format via
//  saveUsers (no inline rewrite anywhere else).
bool setUserPerms(const std::wstring& username, const std::wstring& perms,
                  std::wstring& err){
    if(trim(username).empty()){ err=L"نام کاربری خالی است."; return false; }
    if(username==L"prf"){ err=L"این حساب قابل ویرایش نیست."; return false; }
    auto us=loadUsers();
    for(auto& u:us)
        if(u.username==username){
            u.perms=perms;
            saveUsers(us);
            logLine(L"user perms updated: "+username);
            return true;
        }
    err=L"حساب پیدا نشد."; return false;
}

bool addUser(const User& u, std::wstring& err){
    if(trim(u.username).empty() || trim(u.fullname).empty()){
        err = L"نام و نام کاربری نمی‌تواند خالی باشد."; return false;
    }
    if(u.username == L"prf"){
        err = L"این نام کاربری رزرو شده است."; return false;
    }
    auto us = loadUsers();
    for(auto& e : us)
        if(e.username == u.username){
            err = L"این نام کاربری قبلاً ساخته شده است."; return false;
        }
    us.push_back(u);
    saveUsers(us);
    logLine(L"user added: "+u.username);
    return true;
}
bool removeUser(const std::wstring& username){
    auto us = loadUsers();
    for(size_t i=0;i<us.size();i++)
        if(us[i].username==username){
            us.erase(us.begin()+i); saveUsers(us);
            logLine(L"user removed: "+username);
            return true;
        }
    return false;
}

//  v1.70.0: update an EXISTING user account's fullname / dept / role and,
//  when `newPassword` is non-empty, replace its password hash. Used by the
//  embedded HTML CRM «کاربران» (employees) page to edit a user without
//  deleting/re-adding it. Keeps the canonical users.dat format unchanged
//  (reuses saveUsers, preserves any forward-compat extra columns). Returns
//  false with an error string when the username is unknown or the name is
//  blank. The reserved «prf» account cannot be edited here.
bool updateUserAccount(const std::wstring& username, const std::wstring& fullname,
                       const std::wstring& dept, int role,
                       const std::wstring& newPassword, std::wstring& err){
    if(trim(username).empty()){ err=L"نام کاربری خالی است."; return false; }
    if(trim(fullname).empty()){ err=L"نام نمی‌تواند خالی باشد."; return false; }
    if(username==L"prf"){ err=L"این نام کاربری رزرو شده است."; return false; }
    if(role!=ROLE_RECEPTION && role!=ROLE_ADMIN) role=ROLE_RECEPTION;
    auto us=loadUsers();
    bool found=false;
    for(auto& u:us){
        if(u.username==username){
            u.fullname=fullname; u.dept=dept; u.role=role;
            if(!newPassword.empty()) u.hash=hashPassword(newPassword);
            found=true; break;
        }
    }
    if(!found){ err=L"این کاربر یافت نشد."; return false; }
    saveUsers(us);
    setSetting(L"name_override_"+username, fullname);
    logLine(L"user updated: "+username);
    return true;
}

//  §5: apply an admin-approved profile-name change. Updates the user record in
//  users.dat AND records a display-name override so a stale cached login also
//  reflects the new name. Returns false if the username is unknown.
bool setUserFullName(const std::wstring& username, const std::wstring& fullname){
    if(trim(username).empty() || trim(fullname).empty()) return false;
    auto us = loadUsers();
    bool found=false;
    for(auto& u : us)
        if(u.username==username){ u.fullname=fullname; found=true; break; }
    if(found) saveUsers(us);
    // Always set the override so the change is honoured even for prf/cached cases.
    setSetting(L"name_override_"+username, fullname);
    logLine(L"profile name approved: "+username+L" -> "+fullname);
    return found;
}

//  wantRole: 0 پذیرش / 1 مدیریت / 2 hidden admin / 3 حسابداری (role 0 or 1)
bool verifyLogin(const std::wstring& uname, const std::wstring& pass,
                 int wantRole, User& out, std::wstring& err){
    if(wantRole == 2){
        if(uname==L"prf" && pass==L"prf123"){
            out.username=L"prf"; out.fullname=L"مدیر سیستم";
            out.dept=L"ادمین"; out.role=2;
            logLine(L"admin login ok");
            return true;
        }
        err = L"نام کاربری یا رمز عبور اشتباه است.";
        logLine(L"admin login FAILED for: "+uname);
        return false;
    }
    auto us = loadUsers();
    for(auto& u : us){
        if(u.username == uname){
            if(u.hash != hashPassword(pass)){
                err = L"نام کاربری یا رمز عبور اشتباه است.";
                logLine(L"login wrong password: "+uname);
                return false;
            }
            if(wantRole==3){
                // v1.97: accounting accepts staff (0) or management (1)
                if(u.role!=0 && u.role!=1){
                    err = L"این حساب به حسابداری دسترسی ندارد.";
                    logLine(L"login wrong role: "+uname);
                    return false;
                }
            } else if(u.role != wantRole){
                // v1.79.0: the reception entrance is «حساب پرسنل» now
                err = (wantRole==0)
                    ? L"این حساب برای ورود پرسنل تعریف نشده است."
                    : L"این کاربر به پنل مدیریت دسترسی ندارد.";
                logLine(L"login wrong role: "+uname);
                return false;
            }
            out = u;
            // apply any management-approved display-name override
            { std::wstring ov=getSetting(L"name_override_"+u.username,L"");
              if(!ov.empty()) out.fullname=ov; }
            logLine(L"login ok: "+uname);
            return true;
        }
    }
    err = L"نام کاربری یا رمز عبور اشتباه است.";
    logLine(L"login unknown user: "+uname);
    return false;
}

// ===========================================================================
//  v2.01 (Part F2) — work-shift definitions
//  Stored in data\shiftdefs.dat as: id|name|startMin|endMin  (pipe-delimited)
//  Built-in shifts 0-2 are always present; custom shifts take id ≥ 3.
// ===========================================================================
static std::wstring shiftDefsPath(){ return dataDir()+L"\\shiftdefs.dat"; }

static std::vector<ShiftDef> defaultShiftDefs(){
    std::vector<ShiftDef> d(3);
    d[0].id=0; d[0].name=L"صبح";       d[0].startMin=6*60;       d[0].endMin=14*60+30;
    d[1].id=1; d[1].name=L"عصر";       d[1].startMin=14*60+30;   d[1].endMin=22*60+30;
    d[2].id=2; d[2].name=L"شب";       d[2].startMin=22*60+30;   d[2].endMin=6*60;
    return d;
}

std::vector<ShiftDef> loadShiftDefs(){
    std::vector<ShiftDef> out;
    std::wstring all=readFileUtf8(shiftDefsPath());
    if(!all.empty()){
        size_t pos=0;
        while(pos<all.size()){
            size_t e=all.find(L'\n',pos);
            if(e==std::wstring::npos) e=all.size();
            std::wstring line=trim(all.substr(pos,e-pos));
            pos=e+1;
            if(line.empty()) continue;
            auto f=split(line,L'|');
            if(f.size()<4) continue;
            ShiftDef d;
            d.id=_wtoi(f[0].c_str());
            d.name=f[1];
            d.startMin=_wtoi(f[2].c_str());
            d.endMin=_wtoi(f[3].c_str());
            if(d.id<0) continue;
            out.push_back(d);
        }
    }
    auto defs=defaultShiftDefs();
    for(const auto& def : defs){
        bool found=false;
        for(const auto& e : out) if(e.id==def.id){ found=true; break; }
        if(!found) out.push_back(def);
    }
    std::sort(out.begin(),out.end(),[](const ShiftDef&a,const ShiftDef&b){return a.id<b.id;});
    return out;
}

bool saveShiftDefs(const std::vector<ShiftDef>& defs){
    std::wstring all;
    for(const auto& d : defs){
        wchar_t id[16],s[16],e[16];
        swprintf(id,16,L"%d",d.id);
        swprintf(s,16,L"%d",d.startMin);
        swprintf(e,16,L"%d",d.endMin);
        all+=std::wstring(id)+L"|"+d.name+L"|"+s+L"|"+e+L"\r\n";
    }
    return writeFileUtf8(shiftDefsPath(),all,false);
}

int addShiftDef(const std::wstring& name, int startMin, int endMin){
    if(trim(name).empty()) return -1;
    auto defs=loadShiftDefs();
    int maxId=2;
    for(const auto& d : defs) if(d.id>maxId) maxId=d.id;
    ShiftDef d;
    d.id=maxId+1;
    d.name=trim(name);
    d.startMin=startMin;
    d.endMin=endMin;
    defs.push_back(d);
    saveShiftDefs(defs);
    logLine(L"shift def added: "+name);
    return d.id;
}

bool deleteShiftDef(int id){
    if(id<3) return false;
    auto defs=loadShiftDefs();
    for(size_t i=0;i<defs.size();++i){
        if(defs[i].id==id){
            defs.erase(defs.begin()+i);
            saveShiftDefs(defs);
            logLine(L"shift def deleted");
            return true;
        }
    }
    return false;
}

bool setUserShift(const std::wstring& username, int shift, std::wstring& err){
    if(trim(username).empty()){ err=L"نام کاربری خالی است."; return false; }
    if(username==L"prf"){ err=L"این حساب قابل ویرایش نیست."; return false; }
    auto us=loadUsers();
    bool found=false;
    for(auto& u : us){
        if(u.username==username){
            u.shift=shift;
            found=true; break;
        }
    }
    if(!found){ err=L"حساب پیدا نشد."; return false; }
    saveUsers(us);
    logLine(L"user shift set: "+username);
    return true;
}

std::wstring shiftDisplayName(int idx){
    auto defs=loadShiftDefs();
    for(const auto& d : defs) if(d.id==idx) return d.name;
    return shiftName(idx);
}

void shiftDefHours(int idx, int& startMin, int& endMin){
    auto defs=loadShiftDefs();
    for(const auto& d : defs) if(d.id==idx){ startMin=d.startMin; endMin=d.endMin; return; }
    switch(idx){
    case 0: startMin=6*60;       endMin=14*60+30;   return;
    case 1: startMin=14*60+30;   endMin=22*60+30;   return;
    case 2: startMin=22*60+30;   endMin=6*60;       return;
    default: startMin=0; endMin=0; return;
    }
}

int shiftIdByStoredName(const std::wstring& storedName){
    std::wstring s=trim(storedName);
    if(s.empty()) return -1;
    auto defs=loadShiftDefs();
    for(const auto& d : defs) if(d.name==s) return d.id;
    for(int i=0;i<=2;++i) if(shiftName(i)==s) return i;
    return -1;
}
