/* ===========================================================================
   templates.js — DarmanPlus ready-made print designs  (v2.00)

   ★ این فایل دقیقاً همان ۳۱ قالب داخلی C++ (src/print_designer_templates.inc)
     است تا گالری دیزاینر وب با آنچه موتور چاپ واقعاً seed می‌کند یکی باشد.
     هر تغییری در .inc باید عیناً اینجا هم اعمال شود.

   ایندکس ۰: «پیش‌فرض» — رسید حرارتی R80 درمانگاه شبانه روزی ثامن الائمه
   ایندکس ۱..۳۰: سی طرح اضافی، همه متفاوت، مخلوط R80/R58/A5/A4
   =========================================================================== */
(function () {
  "use strict";

  /* ==========================================================================
     v2.07.0 — The builtin template gallery.

     The AUTHORITATIVE source of every builtin design is the C++ composer
     بساز_طرح(طرح_پارامتر) in src/print_designer_templates.inc (T01..T30 + TB1).
     The engine seeds/repairs those designs on disk (Designs_Init), and the
     designer reads whatever is seeded — so this file no longer mirrors a
     hand-written copy of the layouts (that copy drifted from the engine in
     every release; see the v2.00 header note that used to live here).

     What remains here is the frozen display list (§4.2) used by the gallery
     chrome; the previews render from the seeded design files themselves.
     ========================================================================== */

  var TPL_NAMES = [
    "T01 \u0637\u0631\u062d \u067e\u06cc\u0634\u200c\u0641\u0631\u0636 \u062d\u0631\u0641\u0647\u200c\u0627\u06cc",
    "T02 \u0637\u0631\u062d \u0633\u0627\u062f\u0647 \u0648 \u0633\u0631\u06cc\u0639",
    "T03 \u0637\u0631\u062d \u0631\u0633\u0645\u06cc \u06a9\u0627\u062f\u0631\u062f\u0627\u0631",
    "T04 \u0637\u0631\u062d \u0641\u0634\u0631\u062f\u0647\u0654 A5",
    "T05 \u0637\u0631\u062d \u0633\u0631\u0628\u0631\u06af \u0631\u0646\u06af\u06cc \u0645\u062f\u0631\u0646",
    "T06 \u0637\u0631\u062d \u0634\u0645\u0627\u0631\u0647\u0654 \u0646\u0648\u0628\u062a \u062f\u0631\u0634\u062a",
    "T07 \u0637\u0631\u062d \u0631\u0633\u06cc\u062f \u067e\u0631\u062f\u0627\u062e\u062a",
    "T08 \u0637\u0631\u062d \u062f\u0648 \u0633\u062a\u0648\u0646\u0647\u0654 \u0634\u06cc\u06a9",
    "T09 \u0637\u0631\u062d \u0645\u06cc\u0646\u06cc\u0645\u0627\u0644 \u062e\u0637\u062f\u0627\u0631",
    "T10 \u0637\u0631\u062d \u06a9\u0627\u0631\u062a \u0628\u06cc\u0645\u0627\u0631",
    "T11 \u0637\u0631\u062d \u0646\u0648\u0627\u0631\u06cc \u06f8\u06f0 \u0645\u06cc\u0644\u06cc\u200c\u0645\u062a\u0631",
    "T12 \u0637\u0631\u062d \u0646\u0648\u0627\u0631\u06cc \u06f5\u06f8 \u0645\u06cc\u0644\u06cc\u200c\u0645\u062a\u0631",
    "T13 \u0637\u0631\u062d \u0627\u0641\u0642\u06cc A5",
    "T14 \u0637\u0631\u062d \u0627\u0641\u0642\u06cc A4",
    "T15 \u0637\u0631\u062d \u0628\u0627\u0631\u06a9\u062f\u0645\u062d\u0648\u0631",
    "T16 \u0637\u0631\u062d \u062c\u062f\u0648\u0644\u0645\u062d\u0648\u0631",
    "T17 \u0637\u0631\u062d \u062f\u0648 \u0633\u062a\u0648\u0646\u0647\u0654 \u0641\u0634\u0631\u062f\u0647",
    "T18 \u0637\u0631\u062d \u0633\u0631\u0628\u0631\u06af\u062f\u0627\u0631 \u0628\u0644\u0646\u062f",
    "T19 \u0637\u0631\u062d \u0631\u0633\u06cc\u062f \u0646\u0642\u062f\u06cc",
    "T20 \u0637\u0631\u062d \u0631\u0633\u06cc\u062f \u06a9\u0627\u0631\u062a\u062e\u0648\u0627\u0646",
    "T21 \u0637\u0631\u062d \u0628\u06cc\u0645\u0647\u0654 \u067e\u0627\u06cc\u0647",
    "T22 \u0637\u0631\u062d \u0628\u06cc\u0645\u0647\u0654 \u062a\u06a9\u0645\u06cc\u0644\u06cc",
    "T23 \u0637\u0631\u062d \u0622\u0632\u0645\u0627\u06cc\u0634\u06af\u0627\u0647",
    "T24 \u0637\u0631\u062d \u0631\u0627\u062f\u06cc\u0648\u0644\u0648\u0698\u06cc",
    "T25 \u0637\u0631\u062d \u062a\u0632\u0631\u06cc\u0642\u0627\u062a",
    "T26 \u0637\u0631\u062d \u062f\u0627\u0631\u0648\u062e\u0627\u0646\u0647",
    "T27 \u0637\u0631\u062d \u0641\u06cc\u0632\u06cc\u0648\u062a\u0631\u0627\u067e\u06cc",
    "T28 \u0637\u0631\u062d \u0646\u0633\u062e\u0647\u0654 \u067e\u0632\u0634\u06a9",
    "T29 \u0637\u0631\u062d \u0635\u0648\u0631\u062a\u062d\u0633\u0627\u0628 \u062a\u0641\u0635\u06cc\u0644\u06cc",
    "T30 \u0637\u0631\u062d \u062e\u0644\u0627\u0635\u0647\u0654 \u0645\u062f\u06cc\u0631\u06cc\u062a\u06cc"
  ];

  /* The gallery list. Items are deliberately LIGHT: the authoritative
     layouts live in the C++ composer (بساز_طرح) and are served through the
     designer bridge ("templates" request) from the seeded design files, so
     this file can never drift from what the engine actually prints. The
     paper field keeps the gallery grouping working before the bridge
     responds. */
  var PAPERS = ["A4","A5","A5","A5","A4","A5","A5","A4","A5","A6",
                "R80","R58","A5","A4","A6","A4","A5","A4","A5","A5",
                "A5","A5","A5","A5","A5","A5","A5","A5","A4","A4"];

  var ALL = [], i;
  for (i = 0; i < TPL_NAMES.length; i++) {
    ALL.push({ id: 0, name: TPL_NAMES[i], kind: "builtin", group: "reception",
               paper: PAPERS[i] || "A4", orientation: 0, items: [] });
  }

  window.AZ_TEMPLATES = ALL;
})();