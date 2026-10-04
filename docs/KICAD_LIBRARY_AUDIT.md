<!-- KICAD_LIBRARY_AUDIT.md -->
# Local KiCad discovery and model availability audit

Observed on 2026-10-04 in the actual untracked `kicad/` tree. These are local-copy counts,
not promises about other installations. The source libraries and license files are read-only.

General discovery and on-demand loading were implemented, not a demonstration whitelist.
The resistor was a representative visual demonstration, not proof of universal import support.

| File type | Three main library folders | Entire copied tree |
| --- | ---: | ---: |
| Footprints (`.kicad_mod`) | 15,086 | 15,394 |
| Symbol-library files (`.kicad_sym`) | 225 | 255 |
| VRML models (`.wrl`) | 6,839 | 6,848 |
| STEP models (`.step`) | 6,936 | 6,945 |
| Indexed entries | 29,086 | 29,442 |

The default scan covers `footprints/`, `symbols/` and `3dmodels/`. The other 356 files are
under `demos/` and `template/`; previously they were not indexed by this root configuration.
**Include additional folders** now reaches all 29,442 files, including loose files outside
`.pretty` and `.3dshapes` directories. Main library source identifiers remain unchanged.
Symbol-library entries are whole files, not a count of individual symbols inside each file.

## What listing means

- **Not indexed:** outside the selected root/scan scope, or no completed scan. The optional
  additional-folder scan covers the whole copied tree. Geometry parsing does not gate discovery.
- **Not listed:** indexed but excluded by source type, library, search text or current page.
  There is no catalogue category filter in KiCad Libraries; catalogue filters do not cross tabs.
  The Component Catalogue tab lists component packages, not the external KiCad index. Demonstration
  launches pre-fill the resistor search; clear it to restore the complete selected-source list.
- **No associated model:** footprint metadata contains no model reference. This is distinct
  from a model reference whose target is missing. Hidden models also stay intentionally hidden.
- **Cannot be rendered:** a referenced file is absent, has an unresolved variable, is STEP-only,
  or contains unsupported VRML constructs. It remains indexed and its actual error is displayed.

The old list showed only the first 1,000 matches. Its footer said matches/showing but did not
provide paging or explain the cap prominently. Qualified search could narrow the full index,
but broad searches concealed later entries. The new list has 1,000 results **per page**, with
Previous/Next, displayed range, match count, active filters and complete per-type index counts.
The native application check reached all 15,086 main-folder footprints over 16 pages; Models
mode has 13,775 files over 14 pages. A narrow direct-model search displayed one actual WRL file.
Every entry remains searchable in its source type; search also accepts the complete source ID.

Models browsing already existed. Select **3D models** to browse files directly, independently
of footprint associations. Direct models have no footprint pads; symbol pin mapping is not inferred.
STEP rows and the always-visible format banner now explicitly say STEP is not parsed.

## Footprint model availability

All footprint files were read using the actual structural S-expression parser. All resolved,
unique referenced VRML files were then validated once with the actual CPU loader, one file at a
time. Geometry was discarded immediately; only status/diagnostic records were retained.
The additional-folder pass reused those results and validated only its one new resolved VRML.
The later license-comment extraction fix does not change parsing or tessellation support.

| Footprint outcome | Main folders | Entire tree with additional folders |
| --- | ---: | ---: |
| All visible referenced geometry supported by CPU loader | 6,875 | 6,912 |
| Missing visible model file/reference | 7,346 | 7,391 |
| Present STEP with no existing WRL companion | 96 | 96 |
| Unsupported referenced VRML feature | 211 | 211 |
| Unresolved model variable | 0 | 179 |
| No associated model reference | 553 | 600 |
| All model references hidden | 5 | 5 |
| Footprint metadata parse failures | 0 | 0 |

These outcome groups are disjoint for this copy and sum to the footprint counts. Hidden
optional models are excluded from visible-model compatibility. Unsupported variables in
additional demo files include older `KICAD6_3DMODEL_DIR`; only the configured
`${KICAD9_3DMODEL_DIR}` is resolved. No older-library identity or missing asset is invented.

Of 6,719 unique resolved referenced VRML files, 6,510 passed and 209 failed. The failures were
190 transparent-material files and 19 files with unsupported nodes: 14 `WorldInfo`, two
`PointSet`, two `Switch` and one `NavigationInfo`; shared files account for 211 affected footprints. The remaining 129 VRML files
in the copied tree are not referenced by these resolved visible footprint associations and
were not comprehensively validated. CPU acceptance establishes parser/tessellation coverage,
not correct visual appearance, physical accuracy or electrical/manufacturer certification.
Only representative native rendering was visually inspected.

## Representative exact causes

Paths below are relative to `kicad/`.

| Item | Exact cause |
| --- | --- |
| `Module:RaspberryPi_Pico_SMD` | Indexed footprint; references `${KICAD9_3DMODEL_DIR}/Module.3dshapes/RaspberryPi_Pico.step`. Both that STEP and its same-name WRL are absent from this copy. |
| `Audio_Module:Reverb_BTDR-1H` | Indexed footprint; both `3dmodels/Audio_Module.3dshapes/Reverb_BTDR-1H.step` and `.wrl` are absent. |
| `Button_Switch_SMD:Panasonic_EVQPUJ_EVQPUA` | `3dmodels/Button_Switch_SMD.3dshapes/Panasonic_EVQPUJ_EVQPUA.step` exists; its WRL companion does not. STEP cannot be rendered by this backend. |
| `Calibration_Scale:Gauge_100mm_Grid_Type1_CopperTop` | Indexed footprint with no model reference at all. |
| `Button_Switch_SMD:SW_SPST_PTS647_Sx70` | Its model reference is explicitly hidden in the footprint. |
| `Button_Switch_SMD:SW_Push_1TS009xxxx-xxxx-xxxx_6x6x5mm` | Existing WRL companion contains unsupported `PointSet`: `3dmodels/Button_Switch_SMD.3dshapes/SW_Push_1TS009xxxx-xxxx-xxxx_6x6x5mm.wrl`. |
| `Button_Switch_SMD:SW_SPST_FSMSM` | Existing `3dmodels/Button_Switch_SMD.3dshapes/SW_SPST_FSMSM.wrl` contains unsupported `Switch`. |
| `Connector_RJ:RJ45_Amphenol_RJHSE538X` | Existing `3dmodels/Connector_RJ.3dshapes/RJ45_Amphenol_RJHSE538X.wrl` contains transparent materials, which the current renderer rejects. |
| `demos/complex_hierarchy/complex_hierarchy:Altech_AK300_1x02_P5.00mm_45-Degree` | Previously not indexed under the default scope. Now reachable with additional folders; its `${KICAD6_3DMODEL_DIR}` reference remains unresolved. |
| `template/Edgeberry_Cartridge/faceplate:Edgeberry_Hardware_Cartridge_faceplate.step` | Previously outside the default index and outside `.3dshapes`; now directly listed with additional folders, but still a STEP file, not parsed CAD geometry. |

## Scan state, fixes and verification

Scans enumerate metadata asynchronously. Completed indexes are held only in memory and reused
until Rescan or root change; there is no on-disk cache and no filesystem watcher. After library
changes use Rescan. Cancelled scans are discarded, not published as silently partial indexes.
A prior complete index can remain visible and is now labelled as retained. Root changes clear
old entries and geometry. First-scan cancellation now permits activation to restart scanning.
Selection-generation checks prevent old model results from replacing the current preview.
Footprint/model parser failures appear in details and clear the preview; they never remove
metadata entries from the index.

Fixes: real pagination and qualified-ID search; visible scan scope, state, active filters and
per-type counts; optional whole-tree metadata discovery with stable main identities; clear
present-but-unsupported STEP diagnostics and no-association state; license extraction that
retains copyright/license header comments across blank lines. Source notices are unchanged.

Validation: affected Debug targets build successfully; focused KiCad tests pass including
pagination past 1,000, every fixture ID reachable, source filters, direct WRL, STEP-only errors,
additional loose files, stable identities, first-scan restart and license extraction. Final
CTest passed all four suites in 8.85 seconds. Two earlier native regression runs failed with
empty console output; explicit-log native suites and the final full run passed. Those failures
are retained in canonical ARAMF history; their cause is not established or claimed fixed.
Actual MainWindow/browser native inspection verified main/additional counts and direct WRL
rendering. Generated screenshots and detailed audit output remain local under ARAMF's policy.

Rendering remains the documented VRML97 triangle subset. STEP/F3D, transparency, textures,
unsupported nodes and symbol parsing/pin-to-pad mapping remain unsupported. A STEP reference
may use an explicitly reported existing WRL companion. No procedural substitute is used on error.
