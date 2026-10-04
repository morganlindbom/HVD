<!-- KICAD_FULL_ASSESSMENT.md -->
# Full copied KiCad assessment

Assessment date: 2026-10-04. Application baseline: HVD `main`, commit
`8cf1a7e7d02de2572455f16b93cdf99bc4e86082`. This is a read-only investigation
and planning report for the standalone Component Manager. No application feature
was implemented. Paths are project-relative unless a section declares a shorter
prefix. Counts describe this local copy, not every KiCad distribution.

## Executive findings

1. **General discovery and on-demand model loading already exist; there is no
   demonstration whitelist.** Source type, library, search, scan scope and paging
   determine what is listed. Parser acceptance and successful rendering are
   separate from discovery.
2. The copied root is now **`kicad/9.0/`**, a Windows installation layout. Its
   actual library-data root is **`kicad/9.0/share/kicad/`**. The whole copy contains
   **36,616 files, 1,094 directories including the `kicad/` root, and
   6,062,825,447 bytes**: 6.063 decimal GB / 5.646 GiB. Enumeration had no errors.
3. The library subtree has the **same 29,907 relative file paths** as the earlier
   entire `kicad/` copy. The extra **6,709 files** are outside that relocated
   subtree, principally runtime dependencies. Main library and whole-data counts
   have not increased. Fifteen selected old/new file hashes match; a separate
   existing demo Pico model also matches its old hash. This is sampled content
   continuity, not an exhaustive new checksum assertion.
4. **Selecting the outer `kicad/` root breaks model-variable resolution.** The
   unchanged scanner recursively discovers 29,428 entries there, but the resolver
   maps `${KICAD9_3DMODEL_DIR}` to `kicad/`, not the nested `3dmodels/`. A fresh
   resistor probe returns zero vertices at the outer root and 10,548 vertices
   using the actual data root. The existing root-selection action or
   `--kicad-root` can select the correct folder without changing code.
5. **The globally referenced Raspberry Pi Pico/Pico W models remain missing.**
   An existing, differently located `demos/.../symbols/Pico.wrl` is real VRML, but
   it neither satisfies those references nor passes the current loader:
   `Unsupported VRML node: IndexedLineSet`. It is not newly supplied geometry.
6. The full copy supplies **KiCad 9.0.3 runtime and Open CASCADE 7.9.1 DLLs**, not
   a usable KiCad/CAD development SDK. Genuine C/C++ support/test sources and
   headers exist in Python packages, but there are no KiCad/OCCT headers or
   matching OCCT import libraries. The copied CAD DLL uses MSVC runtime imports;
   the application uses MSYS2 UCRT64 GCC. Direct C++ linkage is not established.
7. **Priority:** select/recognize the correct data root first; then make missing
   references and explicit variable/table mappings diagnosable. Extend the
   existing VRML subset for a bounded set of useful scenes before undertaking
   a separate, toolchain-compatible STEP importer. Missing models require source
   acquisition independently of either importer. Symbol mapping is a subsequent
   explicit data-model milestone, not a consequence of matching names.

## Method, evidence and preservation boundary

Every directory and file was enumerated once for the initial inventory, including
runtime folders and previously unindexed demo/template resources. File records
retain relative path, byte length and modification time; the full directory list,
type totals and subtree totals are retained in local ARAMF assessment evidence.
A final metadata comparison checks preservation without rehashing the 6 GB tree.
Access times are excluded because reading files can update them.

Representative text, S-expressions, version resources and PE import metadata were
read. The unchanged application CPU services performed fresh filename indexing,
footprint metadata parsing and model-path resolution. Geometry was loaded only
for the resistor and loose demo Pico probe; no complete geometry validation was
repeated. Assessment helpers ran outside the copied tree. **No copied executable,
script, plugin or binary was executed or loaded as a runtime dependency.** No
dependency was installed, library was modified, application setting was changed,
or copied file was staged.

The previous audit's model-validation results are historical evidence, explicitly
identified below. Current metadata was compared with that evidence; matching
sample hashes permit representative reuse. Unchecked files are not certified
unchanged merely because their names/counts match. This assessment does not make
new visual-rendering, electrical-compatibility or manufacturer-verification claims.

Canonical local evidence is under
`ARAMF_WORKER_HVD_COMPONENTS/verification/tasks/full-assessment-20261004/`.
The existing ARAMF managed ignore policy keeps worker evidence local. The only
tracked deliverable for this task is this report.

## Inventory and directory relevance

Directory byte totals are recursive and overlapping rows must not be summed.
`K` below means `kicad/9.0/share/kicad/`; `I` means `kicad/9.0/`.

| Directory | Files | Bytes | Contents and possible HVD use | Current use / required work |
| --- | ---: | ---: | --- | --- |
| `kicad/` | 36,616 | 6,062,825,447 | One installation directory, `9.0/` | External read-only source; select its data root |
| `I/bin/` | 6,574 | 524,809,006 | KiCad executables, DLLs, Python runtime, packages, plugins | Unrelated runtime for this application; not linked or executed |
| `I/bin/Lib/` | 6,309 | 257,055,056 | Python standard library, packages, tests and caches | Reference/support material; no current HVD dependency |
| `I/bin/Lib/site-packages/` | 3,765 | 210,598,109 | NumPy, wxPython, Pillow, pip, HTTP packages | Runtime support, not an importer SDK |
| `I/bin/Lib/site-packages/numpy/` | 1,528 | 36,846,037 | Numerical runtime, C headers, support sources and tests | No present use; does not provide STEP parsing |
| `I/bin/Lib/site-packages/wx/` | 416 | 85,599,299 | wxPython runtime and two binding headers | KiCad's GUI/scripting support; Component Manager uses Qt |
| `I/bin/Lib/test/` | 1,321 | 23,061,122 | Python tests, fixtures and small native sources | Unrelated test resources; some extensions resemble CAD formats |
| `I/bin/DLLs/` | 28 | 11,082,624 | Python extension modules | Unrelated runtime |
| `I/bin/plugins/3d/` | 3 | 894,304 | `s3d_plugin_idf.dll`, `s3d_plugin_oce.dll`, `s3d_plugin_vrml.dll` | Importer existence evidence; no compatible public SDK established |
| `I/bin/scripting/plugins/` | 10 | 57,626 | Installed footprint-wizard scripts | Reference algorithms only; not executed or embedded |
| `I/bin/Scripts/` | 5 | 566,214 | Python utility launchers/scripts | Unrelated runtime |
| `I/etc/fonts/` | 23 | 70,714 | Fontconfig settings and configuration fragments | Unrelated GUI runtime; not KiCad library path configuration |
| `I/lib/` | 18 | 29,078,730 | Crashpad static libraries, two Sentry CMake exports, ngspice modules | Runtime/build support for other tools, not a CAD SDK |
| `I/lib/ngspice/` | 6 | 1,325,760 | Simulation code-model modules | Future simulation would be separate scope; no current use |
| `I/share/` | 29,999 | 5,508,729,655 | Libraries, localization and empty documentation directories | Parent of usable library-data root |
| `K` | 29,907 | 5,496,872,175 | Same path set as previous complete data copy | Primary external data source |
| `K/footprints/` | 15,086 | 167,138,653 | 152 `.pretty` libraries, each containing footprints | Importable data; metadata/pads/model references already parsed |
| `K/symbols/` | 226 | 234,745,456 | 225 `.kicad_sym` files plus one other file | Whole-library filenames indexed; individual symbol parsing absent |
| `K/3dmodels/` | 13,775 | 4,875,612,925 | 104 `.3dshapes` libraries: 6,839 WRL and 6,936 STEP | Supported WRL loaded on demand; STEP only uses existing WRL companion |
| `K/demos/` | 594 | 189,231,813 | 17 demo groups, projects, local libraries, models, simulation, manufacturing output | Import fixtures/reference projects; additional-folder discovery only at the proper data root |
| `K/template/` | 180 | 5,501,149 | 19 template directories, global tables, board/project files, HTML/images/PDFs | Future board/template and resolver reference; not a project importer today |
| `K/internat/` | 23 | 19,689,880 | Compiled language resources | Unrelated runtime; not component metadata |
| `K/resources/` | 1 | 4,688,146 | `images.tar.gz` | KiCad UI artwork archive; not component model assets |
| `K/schemas/` | 2 | 18,737 | `api.v1.schema.json`, `pcm.v1.schema.json` | IPC/Package Content Manager reference; not HVD's package schema |
| `K/scripting/` | 20 | 245,416 | Footprint wizards, shell/editor support | Reference material requiring API adaptation; no scripts executed |
| `I/share/locale/` | 92 | 11,857,480 | 46 locale directories containing catalogs | Unrelated runtime |
| `I/share/doc/kicad/tutorials/` | 0 | 0 | Empty directory | No copied KiCad tutorial/manual content here |

The installation root additionally has `I/COPYRIGHT.txt` (2,474 bytes) and
`I/uninstall.exe` (134,868 bytes). There is only one primary `symbols/`,
`footprints/` and `3dmodels/` tree. Across demos and primary data there are 173
`.pretty` and 106 `.3dshapes` directories. File-extension totals for **all 127
extension groups** appear in the appendix; none was excluded from enumeration.

### Significant examples and templates

| Demo directory under `K/demos/` | Files | Bytes | Useful evidence / limitation |
| --- | ---: | ---: | --- |
| `complex_hierarchy/` | 17 | 1,197,030 | Explicit project-local tables, schematic UUIDs, board pads and older model variables; traced below |
| `custom_pads_test/` | 11 | 190,411 | Custom pad parsing/marker fixtures; outlines are not fully rendered |
| `ecc83/` | 20 | 882,801 | Project plus loose `3d_shapes/ecc83.wrl`; special hardware, not a generic verified part |
| `flat_hierarchy/` | 29 | 1,669,899 | Local libraries and legacy path conventions |
| `interf_u/` | 23 | 1,530,544 | Legacy project-local symbol/footprint references |
| `kit-dev-coldfire-xilinx_5213/` | 49 | 4,532,901 | Board, local models and Wings authoring files |
| `microwave/` | 3 | 122,853 | RF board example, reference only |
| `pic_programmer/` | 30 | 1,523,266 | Local libraries and loose WRL assets |
| `python_scripts_examples/` | 5 | 25,590 | Scripting examples tied to KiCad APIs; inspected as data only |
| `simulation/` | 98 | 1,803,550 | Schematic examples, SPICE model text and IBIS; no HVD simulation integration |
| `sonde xilinx/` | 14 | 701,596 | Project-local library-table fixture |
| `stickhub/` | 32 | 2,992,110 | Compact USB hub, README/license and loose STEP assets; unsupported CAD import |
| `test_pads_inside_pads/` | 8 | 44,054 | Overlapping/repeated pad fixtures |
| `test_xil_95108/` | 25 | 1,321,318 | Project-local tables and schematic hierarchy |
| `tiny_tapeout/` | 55 | 32,624,726 | Board, PDF, local symbol/footprint libraries, `.stp` CAD and loose Pico WRL |
| `video/` | 47 | 8,577,475 | Larger legacy library set, Wings files and older variables |
| `vme-wren/` | 128 | 129,491,689 | Large board/schematic example, PDF and manufacturing outputs; unsuitable first import fixture |

The 19 template groups include Arduino Mega/Micro/Nano/Pro Mini/Uno,
BeagleBone cape, Raspberry Pi HAT/uHAT, STM32 boards, TI BoosterPacks,
EuroCard and enclosure examples. `template/Edgeberry_Cartridge/` has 12 files,
674,900 bytes, a project, design guide, HTML metadata, and faceplate STEP/FreeCAD
assets. `template/STM32H7_DevEBox/` has seven files, 1,087,114 bytes, including
a schematic PDF. HAT HTML explicitly describes an expansion board for Raspberry
Pi SBCs; it does not describe a Pico or ROBO-PICO carrier. These are potential
board-outline/reference resources, not automatically reusable component definitions.

There are **54 `.kicad_pro`, 104 `.kicad_sch`, 35 `.kicad_pcb`, 38 `.kicad_wks`**,
one custom-rule file, 12 Gerbers, two drill files, one Gerber-job file and one
placement file. Projects often have several schematic sheets; these file counts
are not counts of independent designs or verified components.

## Version, configuration, licenses and source evidence

### Verified versions versus assumptions

| Evidence | Verified fact | What it does not establish |
| --- | --- | --- |
| `I/bin/kicad.exe`, `kicad-cli.exe`, `pcbnew.exe` Windows resources | Product version **9.0.3**, file version **9.0.3.42748** | No copied binary was executed; not proof of exact library release/commit |
| `I/bin/TKernel.dll`, `TKDESTEP.dll` resources | Open CASCADE **7.9.1** | Not evidence of usable headers/import libraries or ABI compatibility |
| `I/bin/python.exe`, `python311.dll` resources | Python **3.11.5** | Runtime version, not the application's compiler/runtime |
| `I/bin/wx...vc_x64...dll` resources | wxWidgets **3.2.8**, MSVC-oriented names | Does not turn wxPython bindings into a Qt dependency |
| `footprints/Module.pretty/RaspberryPi_Pico_SMD.kicad_mod` | `generator_version "9.0"`, format `version 20241229` | Format date is not product version 20,241,229 or a library tag |
| `symbols/MCU_RaspberryPi.kicad_sym` | Generator `9.0`, format `20241209` | Does not establish all symbols/models share one release |
| `demos/complex_hierarchy/*.kicad_sch` / board | Generator `9.0`, schematic format `20250114`, board `20241229` | Old model-variable strings remain despite resaving in newer KiCad |

The `9.0` folder name alone would be a version hint; the file resources verify
9.0.3. No library Git history/release manifest was found that ties every data
file to an exact package commit. Legacy references are verified configuration
inconsistencies, not proof that all demo geometry is obsolete.

### Library tables and variables

There are **19 `fp-lib-table` and 25 `sym-lib-table` files**. The global templates
have 152 footprint-library and 225 symbol-library entries. Every target exists
when `${KICAD9_FOOTPRINT_DIR}` is mapped to `K/footprints/` and
`${KICAD9_SYMBOL_DIR}` to `K/symbols/`. These tables were already in the previous
copy; they are useful newly inspected evidence, not newly supplied files.

Representative exact paths:

- `kicad/9.0/share/kicad/template/fp-lib-table`
- `kicad/9.0/share/kicad/template/sym-lib-table`
- `kicad/9.0/share/kicad/demos/complex_hierarchy/fp-lib-table`
- `kicad/9.0/share/kicad/demos/complex_hierarchy/sym-lib-table`
- `kicad/9.0/share/kicad/demos/tiny_tapeout/fp-lib-table`

Tables **reference** variables; they do not define their machine-specific values.
No `kicad_common.json` or copied global environment-variable configuration was
found. `complex_hierarchy.kicad_pro` has an empty `text_variables` object; its BOM
text substitutions are not model-path definitions. Fontconfig files are unrelated.

A read-only scan of footprint/project/table text found model/library variables
`KICAD9_3DMODEL_DIR`, `KICAD6_3DMODEL_DIR`, `KICAD8_3DMODEL_DIR`,
`KICAD7_3DMODEL_DIR`, `KISYS3DMOD`, `KIPRJMOD`, `KICAD6_FOOTPRINT_DIR`,
`KICAD9_FOOTPRINT_DIR` and `KICAD9_SYMBOL_DIR`. These occur alongside ordinary
text fields such as `REFERENCE`, `QUANTITY` and `PROJECTNAME`; not every `${...}`
is a filesystem variable. Only `KICAD9_3DMODEL_DIR` is presently resolved by HVD's
model resolver. Project-local mapping must use the actual owning project,
not the footprint file's directory by guesswork. Older version aliases should
be explicit, validated configuration rather than silently redirected.

### License and documentation evidence

**21 files have license/copying/copyright names.** Nineteen are outside the
relocated library subtree: the installation copyright notice and 18 Python-package
license files. The other two are the existing demo licenses. Embedded model and
script notices are additional evidence and are not included in that filename count.

- `kicad/9.0/COPYRIGHT.txt` states the KiCad source is mainly GPLv3-or-later,
  identifies third-party notices, and lists the demo files under CC BY-SA 4.0.
  Paths named in that notice describe upstream sources; those source directories
  are not necessarily installed here.
- `.../demos/stickhub/LICENSE.md` and `.../demos/tiny_tapeout/LICENSE.txt` remain
  present and match their earlier content hashes. Their per-project notices must
  accompany any later selected-asset redistribution.
- The resistor WRL header and Panasonic STEP header contain CC BY-SA 4.0 notices
  with the stated electronic-design exception. This is read source evidence,
  not permission inferred from file extensions or the installation notice.
- `.../bin/Lib/site-packages/numpy-2.3.0.dist-info/LICENSE.txt` and
  `.../wx/lib/pubsub/LICENSE_BSD_Simple.txt` document other bundled dependencies.
- `.../share/kicad/scripting/plugins/bga_wizard.py` has its own GPLv2-or-later
  header. Copying its algorithms/source is a separate decision from reading a
  footprint data file.
- No dedicated OCCT SDK license/exception file was discovered in this copy.
  Before any future SDK reuse/distribution, obtain the matching complete SDK and
  notices. Upstream's [7.9.1 exception text](https://github.com/Open-Cascade-SAS/OCCT/blob/V7_9_1/OCCT_LGPL_EXCEPTION.txt)
  is external reference, not proof those notices are bundled locally.

`share/doc/kicad/tutorials/` is empty. Documentation that actually exists includes
demo README files, 23 HTML files, three reStructuredText files, seven Markdown
files, and these four PDFs:

| Exact path under `K` | Bytes | Relevance |
| --- | ---: | --- |
| `demos/tiny_tapeout/doc/demoboard-prelim-v1-0-3.pdf` | 4,118,331 | Demo-board reference; no specifications extracted or certified here |
| `demos/vme-wren/vme-wren.pdf` | 17,104,916 | Large example documentation |
| `template/Edgeberry_Cartridge/Hardware_Cartridge_Design_Guide.pdf` | 291,679 | Future template/connector reference |
| `template/STM32H7_DevEBox/meta/STM32H7XX_M_schematics.pdf` | 663,333 | Board schematic reference |

These PDFs were inventoried, not used to assert manufacturer electrical limits.
No unavailable attachment or external datasheet was treated as inspected.

### Genuine source/build files and dependency suitability

The copy contains **two `.cpp`, 55 `.c`, 29 `.h`, 52 `.lib`, two `.cmake` and
three `meson.build` files**. There are no `.hxx`, `.hpp`, `.cxx`, `.a`,
`CMakeLists.txt` or `Makefile` files. It is inaccurate to say there is no native
source at all, but it is also inaccurate to call this the KiCad source checkout.

The two actual C++ sources are:

- `kicad/9.0/bin/Lib/test/test_cppext/extension.cpp`: CPython C++ compatibility test.
- `kicad/9.0/bin/Lib/site-packages/numpy/_core/tests/data/generate_umath_validation_data.cpp`:
  NumPy numerical validation-data generator.

Headers belong to NumPy/F2Py and wxPython bindings. The Meson files are NumPy
examples/tests. The two CMake files in `lib/cmake/sentry/` export Crashpad targets;
one references an `include/crashpad/mini_chromium` directory that is not in the
inventory. Of the 52 `.lib` files, 21 are SPICE model **text**, not linker inputs;
the others are NumPy-related or Crashpad/mini_chromium libraries. No OCCT `TK*.lib`
or `STEPCAFControl_Reader.hxx`/`BRepMesh_IncrementalMesh.hxx` was found.

Read-only PE inspection of `kicad/9.0/bin/TKDESTEP.dll` identifies x86-64 and
imports `MSVCP140.dll`, `VCRUNTIME140.dll` and `VCRUNTIME140_1.dll`. HVD's configured
toolchain is x86-64 GCC 16.2 / Qt 6.11.2 / UCRT64. Architecture agreement does
not imply compatible C++ ABI, name mangling, object layouts or runtime ownership.
**Do not propose linking these copied DLLs as if they were a supported dependency.**
A future native OCCT backend needs an independently obtained/built compatible
SDK; a separate process with a documented interchange protocol is another
architectural option, not something demonstrated by this assessment.

## Reconciliation with the previous audit

The [previous audit](KICAD_LIBRARY_AUDIT.md) covered the earlier root layout. Its
entire data copy is now nested inside `9.0/share/kicad/`. No data-subtree file path
has been added or removed after normalizing that prefix. The previous audit
already included demo/template metadata; do not describe those files as newly
copied simply because their folders were outside its default index.

| Resource | Previous main libraries | Current main libraries | Previous whole data | Current whole data |
| --- | ---: | ---: | ---: | ---: |
| `.kicad_mod` | 15,086 | 15,086 | 15,394 | 15,394 |
| `.kicad_sym` library files | 225 | 225 | 255 | 255 |
| `.wrl` | 6,839 | 6,839 | 6,848 | 6,848 |
| `.step` | 6,936 | 6,936 | 6,945 | 6,945 |
| Supported filename types, fully indexed with additional folders | 29,086 | 29,086 | 29,442 | 29,442 |

The 356 additional entries comprise 308 footprints, 30 symbol-library files,
nine WRL and nine STEP files. In the current entire installation, totals for
those four file types remain exactly the whole-data column. Native runtime files
account for the overall growth to 36,616 files.

Fresh parsing of all **15,394** footprints had **zero metadata parser failures**.
Normalized comparison with earlier metadata found **zero differences** in pad
counts, model counts, model-reference strings, hidden flags, resolution states
or selected WRL candidates using the proper data root. This comparison does not
claim to compare every coordinate, text property or byte of every file.

### Missing models and additional formats

| Representative exact path under `K` | Current finding | Relation to previous audit |
| --- | --- | --- |
| `footprints/Module.pretty/RaspberryPi_Pico_SMD.kicad_mod` | References `${KICAD9_3DMODEL_DIR}/Module.3dshapes/RaspberryPi_Pico.step`; both corresponding STEP and WRL are absent | Same footprint hash; absence freshly verified |
| `3dmodels/Module.3dshapes/RaspberryPi_Pico_W.step` and `.wrl` | Both absent | Previously missing; not supplied now |
| `demos/tiny_tapeout/tinytapeout-kicad-libs/symbols/Pico.wrl` | 5,972,138-byte real VRML; current CPU loader rejects `IndexedLineSet` | Already present; old hash matches; new on-demand support check |
| `demos/tiny_tapeout/tinytapeout-kicad-libs/footprints/MCU_RaspberryPi_and_Boards.pretty/RPi_Pico_SMD.kicad_mod` | References `${KIPRJMOD}/pico/Pico.wrl`, not the actual loose `symbols/Pico.wrl` | Existing project-local variable/path problem, not resolved by recopying |
| `.../MCU_RaspberryPi_and_Boards.pretty/RPi_Pico_SMD_TH.kicad_mod` | Contains an absolute path from another machine | Nonportable source reference; preserve the source and diagnose it |
| `3dmodels/Audio_Module.3dshapes/Reverb_BTDR-1H.step` and `.wrl` | Both absent | Previous missing case remains |
| `3dmodels/Button_Switch_SMD.3dshapes/Panasonic_EVQPUJ_EVQPUA.step` | STEP exists; matching WRL absent | Selected STEP hash matches; genuine STEP importer needed |
| `demos/tiny_tapeout/tinytapeout-kicad-libs/3dmodels/418121270808.stp` | Header `ISO-10303-21`, AP214 schema; genuine STEP with `.stp` suffix | Already present but not counted in old four-type table; never indexed by current scanner |
| `template/Edgeberry_Cartridge/faceplate/Edgeberry_Hardware_Cartridge_faceplate.FCStd` | Native FreeCAD document exists with companion `.step` | Already present; neither native FreeCAD nor STEP parsed |
| `demos/kit-dev-coldfire-xilinx_5213/prj.3dshapes/Jack.wings` | Wings authoring file | Already present; four `.wings` files in whole data, unsupported |

There are four `.stp` files in the entire installation, but **only one is CAD**:
the Tiny Tapeout file above. The other three under
`bin/Lib/test/dtracedata/` are SystemTap scripts; `assert_usable.stp` begins with
`probe begin`, not a STEP header. Extension-based future discovery must not
misclassify those as CAD. No STL, OBJ, glTF/GLB, IGES, STEPZ or F3D model files
were found. DLL names for additional CAD exporters/importers are not model data.

### Duplicate/overlapping trees and mixed references

There is no second complete primary library tree. The 308 extra footprint files
have 247 distinct basenames, and 138 have a basename also found in the primary
footprints. These are **overlapping names**, not certified byte-identical copies
or interchangeable parts. Qualified source IDs and owning project tables remain
necessary. STEP/WRL companions intentionally represent different formats of
the same package and should not be discarded as duplicates.

Demo footprints contain KiCad 6, 7 and 8 model variables and legacy `KISYS3DMOD`
while main tables use KiCad 9 names. The hierarchy example was saved by KiCad 9
yet retains KiCad 6 model references. Its schematic and PCB can also reference
different qualified footprint libraries. Preserve these differences as source
facts instead of guessing a global filename mapping.

### Reused geometry evidence and fresh availability

The earlier exhaustive CPU pass checked 6,719 unique resolved referenced WRL
files: **6,510 accepted, 209 rejected**. Rejections were 190 transparent-material
files, 14 `WorldInfo`, two `PointSet`, two `Switch`, one `NavigationInfo`; shared
files affected 211 footprints. Its 129 unreferenced WRL files were not
comprehensively checked. The loose Pico is one newly probed representative from
outside that resolved-reference set; its `IndexedLineSet` failure must not be
folded into the old 209 total as a new exhaustive census.

| Availability at the correct data root | Fresh metadata result | Earlier loader evidence, not rerun exhaustively |
| --- | ---: | --- |
| All visible references resolve to WRL candidates | 7,123 footprints | 6,912 had all geometry accepted; 211 used unsupported VRML |
| Missing visible model references | 7,391 | Same metadata outcome |
| Present STEP without matching WRL | 96 | STEP parsing remains absent |
| Unresolved variables | 179 | Includes older version variables, `KISYS3DMOD` and project-local paths |
| No associated model reference | 600 | No importer can create nonexistent associations |
| All model references hidden | 5 | Intentionally skipped by current loader |
| Metadata parser failures | 0 | Fresh full footprint check |

These disjoint rows sum to 15,394. **The 7,123 figure is resolution coverage,
not renderability.** The previous 6,912/211 split is retained historical evidence
supported by unchanged code, matching metadata and sampled file hashes, not a
new assertion that every scene was visually inspected or fully revalidated.

## Current application support against the new copy

Inspected implementation files: `src/storage/KiCadLibrary.h/.cpp`,
`src/storage/VrmlGeometry.cpp`, `src/ui/KiCadBrowser.h/.cpp`,
`src/ui/ModelPreview.cpp`, `src/app/main.cpp`, `CMakeLists.txt` and
`tests/KiCadTests.cpp`. Catalogue/persistence remain independent of these external
sources; package containment has not been weakened.

### Discovery, configuration and reachability

| Root / scope | Footprints | Symbol libraries | WRL | STEP | Total indexed |
| --- | ---: | ---: | ---: | ---: | ---: |
| `kicad/`, default recursive fallback; freshly executed CPU scan | 15,394 | 255 | 6,843 | 6,936 | 29,428 |
| `K`, normal three main roots; follows inspected scanner rules and verified primary counts | 15,086 | 225 | 6,839 | 6,936 | 29,086 |
| `K`, Include additional folders; freshly executed CPU scan | 15,394 | 255 | 6,848 | 6,945 | 29,442 |

At the outer root no recognized main children exist, so the default falls back
to recursive enumeration. It admits only conventional `.pretty` footprints and
`.3dshapes` models, plus all `.kicad_sym` files. **Fourteen loose WRL/STEP files
are omitted** from that default fallback. Additional-folder mode reaches them;
the genuine `.stp` remains unsupported/unindexed even in that mode.

At the outer root, every visible footprint model reference fails to resolve:
14,610 footprints have missing-reference outcomes and 179 unresolved-variable
outcomes; the remaining 600/5 are no-association/hidden cases. This is freshly
measured path resolution, not evidence that all target files disappeared.
Direct absolute WRL entries under the root can still resolve, whereas their
footprint variable references fail. IDs also acquire the `9.0/share/kicad/...`
prefix, which can prevent an older qualified demonstration search from matching.

The UI has source choices **Footprints / Symbol libraries / 3D models**, a library
filter and qualified-ID substring search. There is no cross-tab catalogue-category
filter. The page size is **1,000**, with Previous/Next, displayed range and total;
this is no longer a hard result ceiling. In whole-data mode, 15,394 footprints
occupy 16 pages, 13,793 model files 14 pages, and 255 library files one page.
All indexed entries are searchable/reachable in their source type. Listing a
STEP or unsupported WRL does not promise a renderable preview.

Indexing is metadata-only, asynchronous and cancellable. Completed indexes are
in memory, reused until Rescan/root change; there is no disk cache or file watcher.
Cancelled work is not published as a silently partial index; a previous completed
index can remain visibly labelled. Parse/loading errors clear geometry and appear
in details, rather than removing indexed entries. Generation checks reject stale
selection results. These implementation facts were inspected; the native UI
regression campaign was not repeated for this documentation-only assessment.

The root is stored by `QSettings` under `HVD/ComponentManager`, `kicad/libraryRoot`.
The default is `<executable-directory>/kicad`, with explicit CMake development
override `HVD_KICAD_DEVELOPMENT_ROOT`; `--kicad-root` overrides for a launch.
Nothing in current discovery recognizes the installation's `9.0/share/kicad/`
layout automatically. This report does not assume a particular user's live
setting or that a running window has refreshed its index.

Existing workaround, without relocating or modifying the copy:

```powershell
./build/hvd_component_manager.exe --kicad-root ./kicad/9.0/share/kicad
```

For a persistent choice, use **KiCad Libraries > Choose library root** and select
that same folder, then Rescan after library changes. For future builds, point the
existing development option at the actual data root. The historical README's
flat-root examples no longer describe the new copy; this task deliberately edits
only this report.

The existing `tests/KiCadTests.cpp::actualLibrary()` fixture also constructs the
old `kicad/footprints/...` path from its compiled `KICAD_ROOT`. In the new layout
that file is absent and the test's existing `QSKIP` branch would skip the real
library alignment check. A later root-configuration milestone should repair the
fixture path and distinguish a skipped source-dependent check from fresh coverage.
That test was not run or edited in this assessment.

### Parsing, coordinates and rendering

| Capability | Actual support | Remaining work / risk |
| --- | --- | --- |
| Footprint metadata | Bounded structural S-expression parser; quoted strings, nested expressions, pad number/type/shape/XY/rotation/size, description, models/offset/rotation/scale/hide | Full footprint artwork, custom/trapezoid outlines and project PCB import are absent; complex pads are connection markers |
| Repeated/unnumbered pads | Kept as distinct pad-array entries | Must remain distinct from catalogue definition pin IDs |
| Model paths | Only `${KICAD9_3DMODEL_DIR}`, canonical containment within chosen root, relative source paths | Installation-root normalization, project ownership, library tables and explicit older aliases absent |
| VRML | VRML97 `Shape`, `Appearance`, `Material`, `Coordinate`, `Normal`, triangle `IndexedFaceSet`, `Transform`, `Group`, acyclic DEF/USE | Unsupported node/field, nontriangle face, VRML1 or invalid data rejects the scene |
| Materials | Diffuse color retained; geometric face normals; preview lighting | Transparency rejected; textures rejected; emissive/specular/ambient/shininess/crease-angle and explicit normals are absent or approximated |
| Known unsupported nodes | Previous `WorldInfo`, `NavigationInfo`, `PointSet`, `Switch`; fresh loose Pico `IndexedLineSet` | Structural support and defined visibility semantics needed; silently ignoring geometry is unsafe |
| STEP | A `.step` reference can use its existing same-name `.wrl`, visibly reported | No STEP reader/tessellator; no `.stp` discovery; no F3D/FreeCAD/Wings support |
| Viewport | QOpenGLWidget / OpenGL 3.3, perspective/light, axes, rotate/zoom/reset/fit, selectable numbered markers | Renderer remains opaque triangle preview; CPU parsing is not proof of useful framing or correct physical geometry |
| Loading/threading | QtConcurrent CPU import; generation/cancellation flags; GL upload in owning context | Maintain these boundaries for future CAD import; bound memory and expensive work |
| Symbols/mappings | Whole `.kicad_sym` library filenames indexed | No symbol item/pin parsing, inheritance resolution, schematic import or explicit symbol-to-pad mapping |

Current footprint XY markers map `(x,y)` to `(x,-y,0)` in the right-handed,
millimetre, +Z-up viewer. KiCad library WRL coordinates use 2.54 mm per model unit:
`T(offset_mm) * Rz(-rz) * Ry(-ry) * Rx(-rx) * S(scale * 2.54)`; internal VRML
transforms are applied within that placement. Model offsets already use 3D axes.
A future STEP importer must normalize its actual declared units and must **not**
blindly reuse WRL's 2.54 factor. Assemblies, model-local placements and board-local
positions are separate transforms. Loose/demo geometry requires its own unit and
alignment evidence; matching a file name is insufficient.

### STEP requirements and a plausible dependency boundary

A STEP implementation needs a genuine CAD reader, transfer of B-rep/assembly
data, unit normalization, face triangulation with bounded tolerances, placements,
color extraction, orientation/normals, error reporting and asynchronous lifecycle
handling. The existing CPU-geometry/OpenGL split can accept resulting triangles;
catalogue code does not need a CAD dependency.

Open CASCADE is a plausible candidate because its documented
[STEPCAFControl_Reader](https://occt3d.com/dev/doc/refman/html/class_s_t_e_p_c_a_f_control___reader.html)
handles STEP shapes, assemblies, colors and names, and
[BRepMesh_IncrementalMesh](https://occt3d.com/dev/doc/refman/html/class_b_rep_mesh___incremental_mesh.html)
provides meshing. Those are upstream API references, not functions exercised by
this application. Current online documentation redirects to 8.0.1, whereas the
local DLLs are 7.9.1; select a supported SDK version and its matching documentation
before implementation. No copied plugin/DLL is approved or established as a
linkable backend by this report.

## Explicit project relationships: facts, not filename guesses

### Complex hierarchy: symbol, instance, footprint, pads and model

All following paths start with
`kicad/9.0/share/kicad/demos/complex_hierarchy/`.

1. `sym-lib-table` explicitly maps library `complex_hierarchy` to
   `${KIPRJMOD}/complex_hierarchy.kicad_sym`; `fp-lib-table` maps the same library
   name to `${KIPRJMOD}/complex_hierarchy.pretty`. These are distinct symbol and
   footprint resources despite the shared library name.
2. In `complex_hierarchy.kicad_sym`, symbol `CP` contains pins `1` and `2`, both
   `passive`/`line`, with names `~`. Its numbers do not intrinsically mean power
   or ground; the project supplies the electrical connections.
3. `complex_hierarchy.kicad_sch` instance **C104** has `lib_id
   "complex_hierarchy:CP"`, value `47uF/20V`, explicit Footprint property
   `Capacitor_THT:CP_Axial_L10.0mm_D4.5mm_P15.00mm_Horizontal`, and UUID
   `00000000-0000-0000-0000-00004ae173cf`. The value is project annotation,
   not a manufacturer-verified rating discovered by HVD.
4. `complex_hierarchy.kicad_pcb` C104 explicitly names the **local** footprint
   `complex_hierarchy:CP_Axial_L10.0mm_D4.5mm_P15.00mm_Horizontal`. Its `path`
   contains that same schematic UUID. This is an explicit instance relationship;
   the schematic Footprint property and board library namespace differ and must
   not be conflated by basename matching.
5. Board pad `1` is at local `(0,0)` mm and stores net `40`, `+12V`; pad `2` is
   at `(15,0)` mm and stores net `11`, `GND`. Pin IDs `1`/`2` and the linked
   instance provide an explicit numbered relationship for this example. Board
   net names are actual stored assignments, not electrical functions inferred
   from the pad numbers. General import still needs explicit, auditable mapping
   and schematic connectivity handling.
6. The board's model references
   `${KICAD6_3DMODEL_DIR}/Capacitor_THT.3dshapes/CP_Axial_L10.0mm_D4.5mm_P15.00mm_Horizontal.wrl`,
   with offset/rotation zero and scale `(1,1,1)`. HVD currently rejects the
   variable before loading geometry. This shows why a resaved KiCad 9 project
   still needs older-version path configuration.

The reused subsheet `ampli_ht.kicad_sch` also stores multiple instance paths:
the R203 symbol appears as R303 in a second hierarchy instance. A future importer
must use full instance paths and units; a Reference string alone is insufficient.
This assessment did not reconstruct or certify an entire circuit netlist.

### Tiny Tapeout: why Pico and `.stp` are separate issues

Under `K/demos/tiny_tapeout/`, `fp-lib-table` explicitly maps `TinyTapeout` and
`ttlib` to their `.pretty` directories using `${KIPRJMOD}`. The local
`TinyTapeout.pretty/418121270808.kicad_mod` explicitly references
`${KIPRJMOD}/tinytapeout-kicad-libs/3dmodels/418121270808.stp`; that CAD file is
present. HVD cannot resolve `KIPRJMOD` or import `.stp`, so this is present data
behind two unsupported integration boundaries.

The local `MCU_RaspberryPi_and_Boards.kicad_sym` contains symbol `Pico` with
Footprint property `RPi_Pico:RPi_Pico_SMD_TH`; the copied `.pretty` name and
actual project table namespace are different. Its `RPi_Pico_SMD` footprint
references `${KIPRJMOD}/pico/Pico.wrl`, while the existing file is in
`tinytapeout-kicad-libs/symbols/Pico.wrl`. The `_TH` variant has a nonportable
absolute reference. **Do not silently associate the loose WRL with the primary
Module footprint or assert it accurately represents Pico 2 W.** Its identity,
scale, source/license and explicit pad mapping would require separate review;
the current loader already rejects its line geometry.

## Confirmed facts and unresolved questions

**Confirmed:** full metadata inventory, installation nesting, version resources,
unchanged data path set, matching selected hashes, unchanged normalized footprint
availability, missing global Pico models, existing but unsupported loose Pico,
table targets, mixed variables, native source ownership and MSVC import evidence.
General metadata discovery is implemented and Models browsing/pagination exist.

**Unresolved:** exact library release commits; byte continuity of unsampled files;
visual accuracy of every model; usable physical dimensions/identity of the loose
Pico; complete per-asset license provenance; correspondence of older model aliases
to a selected version; desired first manufacturer component; and whether a future
STEP backend should remain GCC-native or use an isolated process. No live-window
state or new native visual campaign is asserted by this assessment.

## Prioritized roadmap for a decision

Estimates are rough engineering time for one developer familiar with this code,
including focused tests. They are not promises; SDK availability, legacy scenes
and visual/alignment defects can increase them. No milestone below was implemented.

| Priority / milestone | User-visible benefit | Supporting files/evidence | Dependencies | Rough effort / uncertainty | Verification before completion |
| --- | --- | --- | --- | --- | --- |
| 0. Select the correct data root now | Existing resistor/other supported previews work again | `K/footprints`, `K/3dmodels`; fresh two-root resistor probe | Existing folder action or CLI only | Minutes; no implementation | Real window selection, expected WRL path, model/pad framing |
| 1. Recognize installation layouts and expose resolver context | Copying an installation no longer makes present files appear missing | `9.0/share/kicad/`, fresh 29,428 vs 29,442 scans; global tables | Preserve containment; normalize selected roots and stable IDs | 0.5–2 days; nested versions and multiple installations add uncertainty | Flat/nested fixtures, IDs, root changes, cancellation, portable paths, native smoke check |
| 2. Add explicit library tables/project variables and bounded availability diagnostics | Clear distinction between absent assets, older variables, project-local references and unsupported formats | 44 tables; hierarchy/Tiny Tapeout fixtures; 179 unresolved footprint cases | Owning-project context, explicit alias policy; milestone 1 | 2–5 days; hierarchy/version conflicts can enlarge scope | Table/URI containment tests, unresolved-variable tests, per-reference diagnostics, known fixtures |
| 3a. Acquire only the first required missing component's source assets | A chosen Pico/component can actually have a preview | Missing `Module.3dshapes/RaspberryPi_Pico*.step/.wrl`; existing footprint | User's target choice, inspect reliable source geometry and license | Hours–several days; externally dependent | Provenance, units, dimensions/pad alignment, license metadata and actual visual check; no invented ratings |
| 3b. Extend the bounded VRML path for useful library scenes | More existing assets render with no CAD SDK | Historical 190 transparency / 19 node failures; fresh Pico `IndexedLineSet` | Decide line/point/Switch semantics; transparent renderer/material data | 2–6 days; transparency sorting and mixed scenes are the largest uncertainty | Bounded parser fixtures, DEF/USE/transforms, missing/unsupported clearing, visual opaque/transparent/line/marker alignment |
| 4. Implement genuine STEP/`.stp` import behind a CPU backend boundary | STEP-only parts and better CAD-derived geometry become visible | 6,945 STEP + one genuine `.stp`; Panasonic and Edgeberry fixtures; OCCT runtime evidence | Compatible full SDK and matching licenses, units/colors/assembly design; do not link copied DLLs blindly | 1–3 weeks; dependency packaging and tessellation quality may exceed range | CAD unit/assembly/color fixtures, malformed inputs, bounds/cancellation/stale selection, placement dimensions, native screenshots and supported-Windows packaging |
| 5. Parse individual symbols and explicit symbol-to-footprint/pin-to-pad links | Search named symbols and inspect sourced pin functions alongside physical pads | 255 library files; C104 UUID/table/assignment trace; hierarchy units | Library-table/project context; inheritance, alternate units and mapping model | 1–3 weeks; complete schematic connectivity/import is additional scope | Symbol inheritance/units, explicit links, repeated/mechanical pads, mismatched namespaces, unresolved mappings; no inferred GPIO ownership |
| 6. Selected-asset component packaging with provenance | Export a portable reviewed component rather than a 6 GB installation | Existing HVD directory packages; source/license notices | Approved asset selection/mapping, model descriptor and license metadata | 2–5 days after required import milestones | Round-trip, atomic failure, package containment, copied-asset notices and revision independence |

**Ordering decision:** path configuration comes first because it presently hides
otherwise usable geometry. Wholesale recopying has not completed the missing model
set; acquire a small chosen component's assets if that component is the immediate
goal. VRML work offers a lower-dependency improvement to existing copied data,
including transparent parts and the loose Pico's line nodes. STEP is the larger
next rendering capability and should be selected deliberately, not inferred from
the available DLLs. Symbol mapping follows resolver context and a defined mapping
model. It can proceed independently of STEP if pin inspection becomes the higher
priority. None of these changes should create a second HVD hardware resource
ownership authority or treat a generic package as a verified manufacturer part.

## Assessment validation and limitations

Fresh checks cover inventory arithmetic, path-set reconciliation, 15 selected
hash comparisons plus the loose Pico hash, all footprint metadata parsing and
normalized comparison, actual CPU discovery/resolution, representative geometry
probes, table-target existence and report-only Git boundaries. Existing generated
governance was refreshed through its canonical producer; scoped ARAMF preflight
was READY. Final evidence/postflight and preservation results are recorded through
the existing worker, not a new status/certification system.

Final assessment validation: **14 focused assertions PASS; canonical memory and
worker verification PASS; scoped ARAMF postflight PASS / VERIFIED**. The full
before/after metadata inventory has zero file or directory-set changes. An initial
assessment-check helper misinterpreted Git's added-file diff exit status and
failed its assertion; the corrected rerun passed with no whitespace diagnostics.
That failure and its recording correction remain in canonical history.

No application feature changed, so no new application build or CTest/GUI/visual
regression campaign was required or represented as fresh. The previous audit's
four-suite CTest PASS and native resistor/battery images remain historical. The
current loose Pico test establishes a CPU rejection, and the current resistor
test establishes CPU geometry acceptance at the correct root. **Neither is a
new native rendering screenshot or a physical accuracy certificate.** Copied
scripts/binaries were not run, and no SDK linkage experiment was attempted.

## Decision brief for Master Lindbom and the reviewing assistant

The copy is now a KiCad 9.0.3 Windows installation under `kicad/9.0/`. Its usable
data root is `kicad/9.0/share/kicad/`. There are 36,616 files / 6.06 GB overall,
but the relocated library subtree has the same 29,907 file paths and the same
15,394 footprints, 255 symbol-library files, 6,848 WRL and 6,945 STEP as before.
The additional installation material does not complete the missing global Pico
models or provide a linkable CAD SDK.

General discovery, Models browsing, on-demand WRL loading and paging are already
implemented. The outer-root configuration now causes false missing-model errors;
selecting the real data root is the immediate workaround. At that root, 7,123
footprints resolve visible WRL candidates, 7,391 still have missing references,
96 are STEP-only, 179 have unresolved variables, 600 have no model and five have
only hidden models. Earlier CPU evidence splits the 7,123 into 6,912 accepted and
211 unsupported footprints; this full geometry pass was not repeated.

A loose Tiny Tapeout `symbols/Pico.wrl` already existed, is not the referenced
Module asset, and currently fails on `IndexedLineSet`. Genuine `.stp`, FreeCAD
and Wings files also already existed outside the previously reported format
columns; they are unsupported. KiCad/Open CASCADE DLLs use MSVC, while HVD uses
GCC, and their SDK headers/import libraries are missing.

Recommended decision: first normalize root/resolver configuration and explicit
table/project variables, then choose one target component and validate its actual
source assets. Extend useful VRML nodes/materials as a bounded milestone; implement
STEP with a separately provisioned compatible SDK when CAD coverage is prioritized.
Add explicit symbol/pin/pad mapping afterward or independently when pin inspection
is the priority. Preserve copied sources, keep the application standalone, and
do not infer electrical functions, manufacturer accuracy or resource ownership
from library names, model appearance or pad numbers. No roadmap feature was
implemented in this assessment.

## Appendix: full file-type inventory

Extension matching is case-insensitive. `[none]` includes extensionless tables,
license files and other resources. Extension alone does not establish content:
the `.lib` and `.stp` distinctions above are important. Sizes are bytes; totals
across all rows equal the complete 36,616-file inventory.

<!-- The table below is generated from the read-only metadata inventory. -->

| File type | Files | Bytes |
| --- | ---: | ---: |
| `.0` | 6 | 9,954 |
| `.1` | 1 | 67 |
| `.2` | 1 | 19,472 |
| `.3` | 1 | 1,587 |
| `.8svx` | 1 | 110 |
| `.aif` | 1 | 61,696 |
| `.aifc` | 4 | 14,032 |
| `.aiff` | 6 | 67,468 |
| `.apache` | 1 | 10,174 |
| `.au` | 8 | 129,226 |
| `.bat` | 5 | 4,346 |
| `.bmp` | 3 | 188,138 |
| `.bsd` | 1 | 1,344 |
| `.build` | 3 | 4,744 |
| `.c` | 55 | 1,523,381 |
| `.cfg` | 2 | 210 |
| `.cir` | 1 | 1,275 |
| `.cm` | 6 | 1,325,760 |
| `.cmake` | 2 | 14,514 |
| `.conf` | 22 | 69,699 |
| `.cpp` | 2 | 12,816 |
| `.crl` | 1 | 800 |
| `.csh` | 1 | 935 |
| `.css` | 1 | 1,325 |
| `.csv` | 30 | 1,391,679 |
| `.ctypes` | 1 | 296 |
| `.d` | 4 | 1,188 |
| `.dectest` | 143 | 4,421,731 |
| `.def` | 4 | 19,208 |
| `.dll` | 213 | 266,368,296 |
| `.drl` | 2 | 10,198 |
| `.dtd` | 1 | 68 |
| `.egg` | 1 | 1,497 |
| `.exe` | 40 | 21,557,162 |
| `.expected` | 5 | 2,085 |
| `.exr` | 2 | 5,270 |
| `.f` | 24 | 11,332 |
| `.f90` | 61 | 30,581 |
| `.f95` | 1 | 144 |
| `.fcstd` | 1 | 33,981 |
| `.file` | 7 | 140 |
| `.fish` | 1 | 2,215 |
| `.fits` | 1 | 8,640 |
| `.gbr` | 12 | 5,193,793 |
| `.gbrjob` | 1 | 5,193 |
| `.gif` | 11 | 4,701 |
| `.gz` | 4 | 4,688,847 |
| `.h` | 29 | 465,607 |
| `.hcom` | 1 | 256 |
| `.html` | 23 | 218,188 |
| `.ibs` | 1 | 2,813 |
| `.ico` | 4 | 73,668 |
| `.idl` | 1 | 4,679 |
| `.in` | 1 | 558 |
| `.inc` | 1 | 17 |
| `.ini` | 3 | 791 |
| `.jpg` | 4 | 74,051 |
| `.js` | 1 | 3,655 |
| `.json` | 15 | 32,756 |
| `.kicad_dru` | 1 | 2,076 |
| `.kicad_mod` | 15,394 | 187,602,613 |
| `.kicad_pcb` | 35 | 92,028,428 |
| `.kicad_prl` | 2 | 5,109 |
| `.kicad_pro` | 54 | 755,714 |
| `.kicad_sch` | 104 | 30,844,086 |
| `.kicad_sym` | 255 | 236,636,060 |
| `.kicad_wks` | 38 | 351,026 |
| `.lib` | 52 | 28,229,427 |
| `.md` | 7 | 35,367 |
| `.mo` | 115 | 31,547,360 |
| `.mod` | 3 | 2,648 |
| `.model` | 1 | 29 |
| `.npy` | 5 | 1,033 |
| `.npz` | 2 | 819 |
| `.out` | 1 | 1,387 |
| `.pbm` | 2 | 82 |
| `.pck` | 4 | 89,493 |
| `.pdf` | 4 | 22,178,259 |
| `.pem` | 25 | 675,394 |
| `.pgm` | 2 | 538 |
| `.pkl` | 1 | 716 |
| `.png` | 69 | 2,109,562 |
| `.pos` | 1 | 16,621 |
| `.ppm` | 2 | 1,562 |
| `.ps1` | 1 | 9,033 |
| `.pth` | 1 | 151 |
| `.pxd` | 8 | 111,037 |
| `.py` | 3,462 | 61,223,091 |
| `.pyc` | 1,520 | 31,908,796 |
| `.pyd` | 84 | 124,696,192 |
| `.pyf` | 7 | 2,433 |
| `.pyi` | 290 | 4,234,995 |
| `.pyw` | 1 | 570 |
| `.pyx` | 5 | 38,990 |
| `.ras` | 2 | 2,112 |
| `.rst` | 3 | 19,696 |
| `.sample` | 1 | 2,249 |
| `.sgi` | 2 | 3,934 |
| `.sh` | 1 | 713 |
| `.sndt` | 1 | 129 |
| `.sp` | 1 | 2,242 |
| `.step` | 6,945 | 3,435,104,260 |
| `.stp` | 4 | 1,541,061 |
| `.svg` | 2 | 57,291 |
| `.tar` | 2 | 435,716 |
| `.template` | 1 | 1,654 |
| `.tiff` | 2 | 2,652 |
| `.tlb` | 1 | 8,736 |
| `.tmpl` | 2 | 356 |
| `.toml` | 62 | 1,789 |
| `.txt` | 144 | 566,346 |
| `.typed` | 20 | 431 |
| `.types` | 1 | 48,509 |
| `.vbs` | 1 | 70 |
| `.voc` | 1 | 63 |
| `.wav` | 6 | 66,836 |
| `.wbk` | 12 | 12,064 |
| `.webp` | 2 | 864 |
| `.whl` | 4 | 3,321,408 |
| `.wings` | 4 | 38,754 |
| `.wrl` | 6,848 | 1,457,471,282 |
| `.xbm` | 2 | 564 |
| `.xml` | 57 | 15,332 |
| `.xsl` | 1 | 153 |
| `.xz` | 1 | 172 |
| `.zip` | 7 | 3,621 |
| `[none]` | 126 | 655,334 |
| **Total** | **36,616** | **6,062,825,447** |
