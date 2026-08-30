#pragma once
#include <cstddef>
#include <string>

enum PdContinuationItemKind {
    PDCI_OTHER=0,
    PDCI_FRAME,
    PDCI_LOGO,
    PDCI_PHOTO,
    PDCI_FIELD
};

// Explicit continuation-page whitelist. It intentionally ignores geometry:
// moving a barcode, QR, total, payment field, signature or footer above the
// service table must never make it repeat. Only page identity,
// patient/reception context and the page shell may repeat before the final page.
inline bool pdContinuationRepeatAllowed(PdContinuationItemKind itemKind,
                                        const std::wstring& normalizedField){
    if(itemKind==PDCI_FRAME||itemKind==PDCI_LOGO||itemKind==PDCI_PHOTO) return true;
    if(itemKind!=PDCI_FIELD) return false;
    static const wchar_t* const safe[]={
        L"{first}",L"{last}",L"{full}",L"{father}",L"{nid}",L"{birth}",
        L"{gender}",L"{mobile}",L"{landline}",L"{address}",L"{ptype}",
        L"{ins}",L"{supp}",L"{insno}",L"{insexp}",L"{queue}",L"{date}",
        L"{time}",L"{datetime}",L"{shift}",L"{dept}",L"{doctor}",L"{user}",
        L"{receiptNo}",L"{apptdate}",L"{appttime}",L"{appttype}",
        L"{regdate}",L"{regtime}",L"{nationalcard}",L"{refdoctor}",L"{room}",
        L"{doctorcode}",L"{performer}",L"{performercode}",L"{specialty}",
        L"{specialtycode}",L"{servicetype}",L"{eprescription}",L"{referralno}"
    };
    for(std::size_t i=0;i<sizeof(safe)/sizeof(safe[0]);++i)
        if(normalizedField==safe[i]) return true;
    return false;
}

// v2.07 §3.4 — LAYOUT-LEVEL BARCODE DE-DUPLICATION GUARD.
// The barcode value must appear EXACTLY ONCE per receipt: as the HRI text
// under the barcode graphic (PIT_BARCODE with hri == true). When a design
// contains such a barcode, any ADDITIONAL text item bound to the same value
// ({receiptbarcode}, or {receiptcode} when it repeats the same payload) is
// skipped at render time. This holds even for user designs saved earlier —
// without deleting the user's item type (RULE 6).
//   hriRendered     — true once a PIT_BARCODE with hri==true has been emitted
//   normalizedField — the item's field, already run through pdNormalizeField()
inline bool pdBarcodeValueAlreadyRendered(bool hriRendered,
                                          const std::wstring& normalizedField){
    if(!hriRendered) return false;
    return normalizedField==L"{receiptbarcode}" || normalizedField==L"{barcode}";
}
