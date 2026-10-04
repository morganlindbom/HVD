<!-- README.md -->
# Component directory package format, version 1

A package is one directory containing `component.json` and optional assets in relative
subdirectories. The definition's integer `schemaVersion: 1` versions this entire format.
JSON is strict and has no comments. `demo-sensor` and `demo-carrier` are synthetic examples.

Required identity fields: `schemaVersion` (1), `id` (1-128 ASCII letters/digits/dot/underscore/
hyphen, starting with a letter or digit), `revision` (positive integer), `name`, `category`,
`kind`, and `verificationStatus`. A name and category cannot be blank. Duplicate IDs across
bundled/user roots are errors, including different revisions of the same ID. A catalogue
stores one current revision per ID. Changed saved content requires a greater revision.

`kind` is `board`, `microcontroller`, `sensor`, `actuator`, `driver`, `passive`, or `connector`.
`manufacturer`, `partNumber`, `description` are optional strings; null/empty means unknown.
`tags` is an optional array of strings. Missing collections default to empty. The reader
rejects incorrect types and unknown top-level fields; add future data under `extensions`.

`pins` contains objects with required stable `id` and `name`, optional string/null
`physicalNumber`, `gpioIdentifier`, `logicalSignal`, optional `capabilities` string array,
and optional `electricalLimits`. Pin IDs must be unique within a definition. Capability
names are `digital-input`, `digital-output`, `analog-input`, `pwm`, `uart`, `spi`, `i2c`,
`pio`, `power`, `ground`. Physical identifiers can be package numbers or connector positions
such as `J1.1`. They must never be treated as GPIO identifiers or logical signal names.

Definition and pin `electricalLimits` arrays contain objects with `name`, explicit `unit`,
and optional finite numeric/null `minimum` and `maximum`. Null or missing bounds are unknown;
missing data is never zero. A known minimum cannot exceed a known maximum. Units are explicit
strings such as `V`, `A`, `Ohm` or `degC`; this milestone does not convert or certify units.

`properties` contains objects with stable `id`, `name`, `type`, optional `default`, and
optional `constraints` object. Types: `string`, `boolean`, `number`, `integer`. Null default
means unknown. Numeric `minimum`/`maximum`, typed nonempty `choices` arrays, and a valid
regular-expression `pattern` for strings are supported. The default must satisfy constraints.

`assets` maps `datasheet`, `image`, `symbol`, `footprint`, `model3d` to relative paths or null.
Resolve against the owning package directory. Absolute paths, drive paths, backslashes,
traversal, empty segments, Windows aliases and symlink escapes are rejected. Missing assets
warn and remain referenced. `component.json` cannot itself be an asset. Import/export copies
referenced available files only; unrelated package files are not copied. Exports publish to
a new directory and never merge into existing output. Safe missing assets remain missing
through a round-trip. The directory name is not a component identity.

`sources` is an array of objects containing required `title`, an absolute HTTP(S) `uri`, and
explicit boolean `inspected`. Put local documents in `assets`. `verificationStatus` is
`unverified`, `demonstration`, or `source-reviewed`; source-reviewed requires at least one
inspected source. These are provenance assertions by the package author, not hardware,
electrical or application certification. `extensions` is an optional free-form object
preserved through serialization and editing.

User saves atomically replace only `component.json` using QSaveFile with direct-write
fallback disabled. Invalid content and failed replacements preserve previous valid bytes.
New/copied/imported/exported packages stage all files and validate them before publication
by same-parent directory rename. Existing destinations are rejected. Definitions are capped
at 4 MiB on load. Keep asset sizes reasonable; import/export currently has no size quota.

Catalogue data remains independent of project instances. Later HVD consumers must reference
`id` plus `revision` and retain that revision's definition when saving a project. They own
configuration, connections and hardware resource allocation. This schema makes no physical
compatibility claims and contains no resource reservations.

## Procedural model descriptor

`demo-board` adds a third synthetic bundled package for the interactive preview. Its geometry
is associated explicitly through `extensions.model`, not through the component name or ID:

```json
{
  "schemaVersion": 1,
  "type": "procedural",
  "generator": "demo-board-v1",
  "units": "mm"
}
```

This descriptor's schema is independently versioned inside package schema 1. The core preserves
it as extension data during save/load and import/export. The UI interprets only this supported
procedural generator. Unsupported descriptors clear the previous scene with an explanatory
message. An `assets.model3d` filename alone never enables mesh, STEP or F3D rendering.

The demo uses millimetres and a PCB-centred right-handed coordinate system: +X along length
away from the connector, +Y across width, +Z toward the chip. PCB bounds are -25..25 mm in X,
-12..12 mm in Y and -0.8..0.8 mm in Z. Other dimensions, connector placement, 32 header posts
and mounting markers are synthetic visual choices. The four labelled gold rings are markers
and do not cut holes in the board. No geometry is manufacturer-verified.
