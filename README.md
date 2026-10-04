<!-- README.md -->
# HVD Component Manager

A standalone C++20 / Qt 6 desktop application for reusable hardware component definitions.
The `hvd_components_core` library uses Qt Core only. The Widgets UI and application startup
are separate targets. This milestone does not modify or depend on HVD or PVD internals.

## Build and launch

The verified toolchain is Windows MSYS2 UCRT64 GCC, Qt 6, CMake and Ninja.
The desktop target requires the installed Qt Concurrent, OpenGL and OpenGLWidgets modules and an
OpenGL 3.3 capable driver; the core library still uses Qt Core only. No dependencies
are downloaded by the build. Put `C:\msys64\ucrt64\bin` on PATH (including at runtime).

```powershell
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCMAKE_PREFIX_PATH=C:/msys64/ucrt64
cmake --build build --parallel 4
ctest --test-dir build --output-on-failure
.\build\hvd_component_manager.exe
```

Qt Test is required with `BUILD_TESTING=ON` (default). Use `-DBUILD_TESTING=OFF` for an
application-only build. Bundled packages are copied next to the executable at build time.
For a portable distribution, ship that `components` directory and deploy Qt runtime files
with the matching UCRT64 `windeployqt6` tool; the developer executable requires Qt on PATH.

Startup options:

```powershell
.\build\hvd_component_manager.exe --bundled-root .\resources\components --user-root D:\MyComponents
.\build\hvd_component_manager.exe --smoke-exit
.\build\ui_tests.exe -platform windows
```

The default user root is `QStandardPaths::AppDataLocation/components` under the HVD /
ComponentManager application identity. The window shows the resolved location. User and
bundled roots must be separate, nonoverlapping directories. `--smoke-exit` launches the real
window and exits after 1.5 seconds; it is intended for launch checks.

## Application workflow

Search by name, manufacturer, part number or tags. Combine category and kind filters,
select a component, and inspect its identity, revision, description, pins, electrical
limits, configurable properties, assets, source references and verification status.
Missing assets are visibly labelled and produce warnings rather than crashes.
Supported PDF, text and raster image references can be opened in the system document viewer.
Catalogue asset references stay within their packages. The procedural synthetic board renders
in Model Preview; the separate KiCad Libraries tab renders supported external VRML models.

**New component** creates a detached editor. Enter a name, category, kind, metadata and
pins, then Save. Pin physical numbers, GPIO identifiers and logical signals have distinct
columns. Capabilities are comma-separated names from the package format. Errors appear
in the editor and leave it open. Closing a modified editor or application offers Save,
Discard and Cancel.

**Edit user component** preserves its ID and automatically increases its revision when
content changes. Bundled entries are read-only. **Create editable copy** assigns a new ID
and copies referenced assets into an independent user package when saved. **Import package**
selects a directory containing `component.json`. **Export package** asks for a new directory
path, which must not already exist. Neither import nor export overwrites existing packages.
**Reload** reads both roots transactionally. Invalid packages leave the previous valid
in-memory catalogue intact and show diagnostics; correct or move the invalid package and retry.

## Architecture and public core API

- `src/core/ComponentDefinition.*`: value models, schema decoding, serialization and validation.
- `src/core/Catalogue.*`: stable-ID lookup, registration, search/filtering and user operations.
- `src/storage/ComponentPackage.*`: package loading, atomic metadata saves, staged copies and path checks.
- `src/ui/`: QWidget presentation, detached metadata/pin editing, procedural geometry
  generation and an independent OpenGL viewport.
- `src/app/main.cpp`: application identity and configurable package roots.
- `resources/components/`: read-only synthetic examples and package format documentation.
- `tests/`: core regression tests and Qt widget workflow smoke tests.

Link `hvd_components_core` and include `core/Catalogue.h`. `Catalogue::load` replaces the
catalogue only if all packages are valid. `find(id)` returns a const package pointer or null;
do not retain pointers across catalogue mutations. `search(query, category, kind)` returns
independent values. `registerPackage` rejects duplicate IDs. `saveUser`, `importPackage`
and `exportPackage` return success and structured `Diagnostics` with severity, path and
message. `PackageStore` offers lower-level load/save/copy operations. JSON decoding and
package loading assign their output only after successful validation. Supply a fresh
Diagnostics collection for each operation. Warnings are accepted; errors block mutation.

The metadata editor retains properties, electrical limits, assets, sources and extensions.
For this foundation, these advanced fields are authored in JSON or imported from packages;
the GUI edits basic metadata and pins. See [package format](resources/components/README.md).

## Adding the next component

Create a user component through the editor, or prepare a new directory with `component.json`
and optional assets, then import it. Use a unique stable ID and a positive revision. Validate
it with the core API before distributing it. Export a directory package for another user.
New bundled entries belong in `resources/components/<package>/component.json`; rebuild to
copy them to the executable's catalogue.

Leave unsupported specifications null or omitted. Add explicit units to every electrical
quantity. Only mark a source inspected after actually reading it; source review is an author
assertion, never physical certification. Changing content requires a greater revision;
changing identity means a new component. Existing pin IDs should remain stable across edits.

## Initial data and limitations

Three synthetic examples demonstrate a sensor, a carrier board and an interactive procedural
demo board; none represents a real manufacturer part. One intentional missing text asset demonstrates diagnostics. No Pico 2 W,
ROBO-PICO, IR4427, IRF3205 or CSD18531Q5A datasheet, image or attachment is available in this
workspace. No manufacturer specifications were inferred or invented. A real ROBO-PICO carrier
and its installed Pico must be separate definitions when source material becomes available.

Directory packaging only; no ZIP, plugin framework, resource allocation or schematic/PCB editor.
3D rendering supports the procedural demo and the bounded KiCad VRML97 triangle loader.
STEP, F3D, textures, transparency and general CAD import are unsupported. A successful metadata edit replaces the package's current revision;
this version does not archive earlier revisions. Package commits use QSaveFile and a staged
same-parent directory rename, but do not promise power-loss recovery or multi-process editing.
If one package is invalid, a first catalogue load remains empty; diagnostics identify the blocker.

## Later HVD integration

HVD can link the core library or consume the versioned directory format without Widgets.
A project instance must own an instance ID, label, configuration and assigned connections,
and reference the component ID **and exact definition revision**. HVD should preserve a
project-local definition snapshot or resolve an explicit revision archive; never silently
substitute a newer revision. This application stores catalogue definitions only.

GPIO, PWM, PIO, UART, SPI and I2C ownership stays with HVD's existing hardware authority.
Capabilities describe support and allocate nothing. Matching capabilities alone do not
establish electrical compatibility. No project persistence or Add to HVD action is included.

## Validation and governance

The configured ARAMF worker remains the sole governance and status authority. Task contracts,
evidence and postflight are under `ARAMF_WORKER_HVD_COMPONENTS/verification/tasks/`;
Project Memory uses its existing canonical recorder. No certification is claimed.

Focused tests cover schema validation, duplicate component/pin IDs, defaults and constraints,
unknown bounds, separate identifiers, search/filtering, save/load, package import/export,
unsupported schema, transactional failures, containment, bundled immutability, independent
copies and an OS-denied atomic replacement preserving old bytes. Qt UI tests exercise search,
filters, inspection, creation, pins, save, editing, reopening, copying and unsaved-change prompts.
The symlink test reports SKIP if Windows denies creation of directory symlinks. CTest runs
native Windows widget and OpenGL tests; a functioning display and graphics context are required.
The model suite checks geometry, framebuffer pixels, selection clearing, descriptor errors,
mouse orbit, wheel zoom, reset, fit, resize and repeated closing. Automated checks are supplemented
by inspection of a screenshot from the actual running application.

Supporting documentation snapshots in `reference-material/` cover Qt CMake, Qt Test, CTest
and MSYS2 environments. They are references rather than manufacturer source documents.

## Interactive Model Preview

Select **Demo Board ? synthetic model** to see shaded triangle geometry in the application.
A new application selection defaults to this component. The last selected ID is stored in
QSettings on normal closure and restored if it still exists in the catalogue.

- Hold the left mouse button and drag to rotate around the board.
- Scroll the mouse wheel to zoom; touchpad wheel input is also supported.
- **Reset view** restores the initial connector-end perspective and fitted camera.
- **Fit to view** preserves rotation and reframes the complete model for the current viewport.
- Resizing preserves relative zoom and updates the perspective aspect ratio.

The neutral background, directional diffuse/specular light and XYZ orientation axes belong
to `ModelPreview`; catalogue and persistence services contain no rendering dependencies.
`DemoBoardGeometry` generates genuine 3D triangles with thickness and flat surface normals.
The 50 x 24 x 1.6 mm PCB and all other dimensions are synthetic design choices. Four gold
rings with dark centres and labels M1-M4 are mounting-hole **markers**, not drilled holes.
The two rows have 16 metallic posts each. A dark chip, metallic end connector and yellow
pin-1 triangle make orientation apparent. The depicted posts are visual demo geometry,
not a verified physical pinout. No Pico or ROBO-PICO accuracy is asserted.

Coordinates use millimetres: origin at the PCB centre, +X along its length toward the end
opposite the connector, +Y across its width, and +Z toward the mounted chip. The PCB spans
X=-25..25, Y=-12..12 and Z=-0.8..0.8 mm. The connector is at the negative-X end.

The association is `ComponentDefinition.extensions.model`, with its own descriptor schema
version. Supported descriptor: `{"schemaVersion":1,"type":"procedural",
"generator":"demo-board-v1","units":"mm"}`. Unknown schemas, generators, types and units
show an empty state and clear previous geometry. Catalogue file descriptors remain unsupported;
external KiCad VRML loading uses a separate source resolver. Future CAD descriptors can extend this namespace without coupling the core to
OpenGL or pretending that STEP/F3D files are parsed.

For an immediate demonstration and native desktop screenshot:

```powershell
.\build\hvd_component_manager.exe --select example.synthetic-demo-board --screenshot .\artifacts\model-preview.png
```

The capture records the running application client area after rendering, including its live
OpenGL framebuffer and its annotations, even if another desktop window obscures it. The viewport
framebuffer is copied into its exact client-area rectangle; no geometry is recreated for the image. The application remains open
unless `--smoke-exit` is also supplied. The verified screenshot is `artifacts/model-preview.png`.


## KiCad Libraries

The **KiCad Libraries** source tab browses external files read-only. **Choose library root** stores
`kicad/libraryRoot` in the same HVD/ComponentManager QSettings configuration as catalogue selection.
The installation default is `<executable-directory>/kicad`, independent of the working directory.
For development explicitly configure `-DHVD_KICAD_DEVELOPMENT_ROOT=<source-tree>/kicad` with CMake,
or launch with `--kicad-root <directory>`. Nothing copies or embeds this large tree during the build.

Discovery in this checkout found `kicad/footprints` (15,086 `.kicad_mod` files inside `.pretty`
libraries), `kicad/symbols` (225 `.kicad_sym` libraries), and `kicad/3dmodels` (6,839 `.wrl`
and 6,936 `.step` files inside `.3dshapes` libraries). Counts describe this copied tree, not a
required installation. Default scanning uses these source folders and excludes unrelated demos.
Enable **Include additional folders** to also index demo/template and loose files; default scanning
retains the three main library roots. Main source identifiers remain stable in either scan mode.
Nested library paths qualify identities to avoid same-name collisions. Standalone license files were found only under unrelated demos; the
selected resistor VRML has an embedded copyright and CC-BY-SA 4.0 notice with an exception.
The source and all notices remain unchanged. Review each asset's license before redistribution.

Select Footprints, choose a library or search its qualified `library:item` name. Metadata scanning
runs in Qt Concurrent with cancellation. The list shows 1000 matches per page with Previous/Next controls, a range/total indicator, and
explicit active filters. All indexed items are reachable through pagination or qualified-ID search. Meshes are parsed only on selection, outside the GUI thread; generation checks discard
superseded scans and model results. GPU upload occurs only in QOpenGLWidget's current context.

To demonstrate the actual copied model, search:
`Resistor_THT:R_Axial_DIN0207_L6.3mm_D2.5mm_P7.62mm_Horizontal`.
Its footprint references STEP; the browser explicitly resolves and renders the existing same-name
VRML companion. **STEP is not parsed.** Rotate by dragging, zoom with the wheel, use Reset/Fit, and
toggle Show numbered pads. Click a marker to highlight it and inspect original footprint XY coordinates.
Markers are connection centres drawn over the mesh, not exact copper outlines or inferred GPIO functions.

### Parser and coordinate contract

`hvd_kicad` is a separate CPU-side library using Qt Core/Gui, with no QWidget/OpenGL dependency.
`parseFootprint` uses a recursive S-expression parser with quoted-string escapes, nested expressions,
finite-number validation, and nesting/size bounds. `KiCadPad` records are independent from component
pins: repeated numbers and unnumbered mechanical pads remain separate array entries. Positions,
rotation, dimensions, layers and drills are retained. Dedicated footprint geometry is described below.
Complex custom/trapezoid outlines are visibly diagnosed as incomplete; only their origins and supported
drills are shown. Symbol pin parsing/mapping remains outside this milestone.

`scanKiCad` returns separate `footprint:library:item`, `symbol-library:library:item`, and
`model:library:filename` identities. `resolveKiCadModel` maps `${KICAD9_3DMODEL_DIR}` to the
configured `3dmodels` directory (or a directly selected model root); unknown variables, missing files
and canonical paths outside the configured root produce errors. It never relaxes PackageStore containment.

`loadVrml` genuinely parses VRML97 `Shape`, `Appearance`, `Material`, `Coordinate`, triangle
`IndexedFaceSet`, `Group`, `Transform`, and acyclic shared `DEF`/`USE` nodes. Scene transforms include
translation, axis-angle rotation, scale, centre and scale orientation. Normals are recomputed after placement.
Diffuse colours are retained; specular/ambient/crease-angle shading is explicitly approximated by the viewer.
Textures, transparency, non-triangle faces, unknown fields/nodes and VRML1 are unsupported and diagnosed.
F3D, STEP, X3D and general CAD import are not supported. File/token/depth/triangle limits protect responsiveness.

The viewer is right-handed, millimetres, with +Z up. Footprint XY maps to `(x,-y,0)` because KiCad's
2D Y points downward. KiCad library VRML coordinates are multiplied by **2.54 mm per unit**.
Model placement is `T(offset_mm) * Rz(-rz) * Ry(-ry) * Rx(-rx) * S(scale * 2.54)`; model offsets
already use 3D axes and are not additionally Y-flipped. VRML scene transforms apply inside that matrix.
See [KiCad's official exporter implementation](https://gitlab.com/kicad/code/kicad/-/blob/9.0/pcbnew/exporters/exporter_vrml.cpp)
and the [VRML97 node specification](https://www.web3d.org/documents/specifications/14772/V2.0/part1/nodesRef.html).
The resistor has 7.62 mm pad spacing; imported bounds and lead vertices provide a numerical alignment check.

### Later component and HVD integration

External library items are references, not automatically registered manufacturer components. Symbol parsing,
explicit pin-to-pad maps, self-contained export of external sources and CAD tessellation remain future work.
Existing component package export applies only to catalogue packages. A future asset-copy workflow must select
only required files, preserve license/source metadata, and validate the destination through PackageStore.
HVD can link `hvd_components_core` and optionally `hvd_kicad`, reference definition ID/revision from project
instances, and own resource assignments itself. Physical pad numbers do not establish electrical functions.


## Repository scope and local dependencies

The `morganlindbom/HVD` repository currently contains the standalone HVD Component
Manager, including the synthetic board preview and read-only KiCad/VRML integration. It is
intended for later HVD integration; it does not contain the separate HVD/PVD application.

The copied KiCad libraries are local dependencies and are deliberately not tracked, copied
into Qt resources, or downloaded by CMake. Supply your own KiCad library distribution with
its license notices, preserving the library directory layout, for example:

```text
kicad/
  symbols/     *.kicad_sym
  footprints/  Library.pretty/*.kicad_mod
  3dmodels/    Library.3dshapes/*.wrl and/or *.step
```

The application also works without these libraries: the bundled synthetic preview and
catalogue remain available. Configure an external root with **KiCad Libraries > Choose
library root**, or use `--kicad-root <directory>`. For source-tree development:

```powershell
$env:PATH = "C:/msys64/ucrt64/bin;" + $env:PATH
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCMAKE_PREFIX_PATH=C:/msys64/ucrt64 -DHVD_KICAD_DEVELOPMENT_ROOT="$((Resolve-Path ./kicad).Path.Replace('\','/'))"
cmake --build build --parallel 4
ctest --test-dir build --output-on-failure
./build/hvd_component_manager.exe --kicad-root ./kicad
```

Omit the development-root option when no local library tree exists. The real-library
fixture test reports a skip if its resistor source is absent; parser, resolver and
synthetic fixtures still run. Native GUI tests require a Windows display and a working
OpenGL 3.3 driver. STEP references can use an explicitly reported existing VRML companion;
STEP itself is not parsed. The supported VRML97 subset and other limitations are documented
above.

The existing ARAMF-managed ignore block is preserved. Its local control directory and
AGENTS entry point stay on disk under that policy; machine-bound `.aramf.json` configuration,
downloaded reference snapshots, screenshots and test output are also local. No governed
records are deleted or rewritten to prepare this repository. There is currently no policy
requiring a generated screenshot or evidence file to be committed. Build products, personal
IDE settings and credentials are excluded. Application source, tests, bundled demonstration
definitions and this reproducibility documentation are tracked.


The [local KiCad availability audit](docs/KICAD_LIBRARY_AUDIT.md) distinguishes discovery, filters,
missing associations, absent files, STEP-only files and unsupported VRML features. The browser now
reports index completion/cancellation, retained prior indexes, per-type counts and scan scope.
Press **Rescan** after changing local library files; there is no persistent metadata cache or file watcher.
Cancelled first scans can restart when the source tab is reopened. STEP-only files are explicitly
labelled and diagnosed as present but unsupported rather than falsely reported as missing.

## Dedicated 2D footprint preview

KiCad footprint selection displays **Footprint Preview ? 2D** and **Model Preview ? 3D**
simultaneously in a horizontal resizable splitter. Both share one structurally parsed footprint
and each pad's source-record index. Repeated numbers and unnumbered mechanical pads remain distinct;
click coincident pads repeatedly to cycle records. `M<n>` labels identify unnumbered records for
inspection and do not assign electrical numbers. Linked selection shows number, type, layers,
position, rotation, dimensions and drill/offset. The two cameras operate independently.

The CPU `hvd_kicad` library prepares `FootprintDrawing` paths without QWidget/OpenGL.
Supported pads: rectangle, circle, oval/obround and roundrect with explicit corner ratio.
Circular and oval/slotted drills retain local offsets and pad rotation. Supported artwork:
lines, rectangles, circles, three-point arcs, XY polygons and reference/value/user text.
Solid, dash, dot, dash-dot and dash-dot-dot strokes are supported. Text uses an explicitly
reported Qt font approximation, not exact KiCad stroke glyphs; variables remain literal.
Silkscreen, fabrication, courtyard and named copper layers have colours and visibility controls.
Copper wildcards expand to front/back; mask/paste expansion and inner-board stackups are not
reconstructed. Mechanical holes have a separate drill layer. Back layers are initially hidden.
Custom/trapezoid/chamfered pads, legacy angle arcs, polygon curves, zones and embedded images
are incomplete and diagnosed visibly, with no invented replacement outline. This is inspection,
not a PCB editor, symbol mapper or compatibility certification.

All 2D geometry uses KiCad's stored millimetres: **+X right, +Y down**, viewed from the front.
Positive pad/text angles are counterclockwise; the painter applies the negative screen angle.
Back layers share stored coordinates and are not implicitly mirrored. **Back view** explicitly
mirrors X; layer visibility remains independent. In 3D the same pad maps to `(x,-y,0)` with +Z up.
Asymmetric fixtures verify signs, rotation, slotted-drill offsets and back-view mirroring.
Fit uses visible paths including stroke widths and supported text, so long value text can make copper small;
use wheel zoom to inspect it. Footprints still display when models are absent or unsupported.
Direct model/symbol-library selection explains the missing footprint association and clears it.

Controls: wheel zoom about the pointer; middle/right drag pan; **Fit**/**Reset** restore framing;
**Grid**, origin axes and adaptive millimetre scale provide measurement context. Layer checkboxes
serve as the legend. Click a pad in either view to synchronize its record highlight. Selection/root
changes and cancellation clear old paths, layer controls and model data; asynchronous generation
checks reject stale results. Parsing and geometry preparation run outside the GUI thread.

The `footprint-tests` suite covers primitives, actual bounds, asymmetric orientation, rotated pads
and drills, repeated/coincident records, layer pixels/visibility, camera navigation, resizing,
missing/malformed sources, missing models, direct model clearing and linked 2D/3D input.
Existing catalogue/package, KiCad, UI and 3D regression suites remain in CTest.

For this checkout's copied KiCad installation, the actual data root is
`kicad/9.0/share/kicad`, containing `footprints`, `symbols` and `3dmodels`. Configure that directory
rather than the outer `kicad` directory. For example:

```powershell
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCMAKE_PREFIX_PATH=C:/msys64/ucrt64 -DHVD_KICAD_DEVELOPMENT_ROOT="$((Resolve-Path ./kicad/9.0/share/kicad).Path.Replace('\','/'))"
cmake --build build --parallel 4
ctest --test-dir build --output-on-failure
./build/hvd_component_manager.exe --kicad-root ./kicad/9.0/share/kicad --kicad-select Resistor_THT:R_Axial_DIN0207_L6.3mm_D2.5mm_P7.62mm_Horizontal --screenshot ./artifacts/footprint-preview.png
```

Source format reference: [KiCad S-expression PCB/footprint format](https://dev-docs.kicad.org/en/file-formats/sexpr-pcb/).
Copied libraries and licenses remain local and read-only. STEP remains unsupported; supported
VRML companions are explicitly reported. No new dependency or package-containment exception is added.
