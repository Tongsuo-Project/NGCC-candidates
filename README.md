# NGCC-candidates

Private mirror of **NGCC Round-1 candidate reference implementations and
specification documents**, used by
[Tongsuo-Project/NGCC-provider](https://github.com/Tongsuo-Project/NGCC-provider).

## Layout

| Path | Contents |
|------|----------|
| `kem-NN/`, `sign-NN/` | Per-scheme tree: **Reference Implementation** (byte-frozen) plus **Spec** docs |
| `…/Implementations/` (or equivalent) | Submitter reference sources (`provenance/sources.sha256`) |
| `…/Algorithm specifications/` (and variants) | Algorithm specification PDFs from Round-1 packages |
| `…/中文资料/`, `Supporting Documentation/` | Extra non-IP docs when present in the package |
| `provenance/` | Catalog, archive checksums, integration policy, Spec inventory |
| `PROVENANCE.json` | Origin of this tree |

**Not included:** harness / ABI glue (lives only in NGCC-provider), Test Vectors,
Basic information / 算法基本信息 (signed forms), and pure Intellectual Property statement PDFs.

## Policy

- Author algorithm **Implementation** sources must remain unmodified.
- Prefer author PRs into this repository for algorithm fixes; NGCC-provider should
  consume a pinned commit/tag of this repo rather than editing trees in place.
- Spec PDFs are taken from official NICCS Round-1 candidate packages (even when
  the Implementation tree follows a later author revision).


## Security levels

This repository keeps **all Reference Implementation parameter sets** from the
Round-1 submitter packages, including **512-bit** security levels (and NIST
Level-5 / `*-III` sets where those correspond to 512-bit classical security).

[NGCC-provider](https://github.com/Tongsuo-Project/NGCC-provider) still
**integrates and tests only 128-bit and 256-bit** (plus selected 192-bit) levels;
384/512 sources here are for archive completeness and are not wired into
provider tests.

Names that embed `512` as a **polynomial dimension** (e.g. `yuanyang-512`,
`CTL-257-512`) are not necessarily 512-bit security—see the provider summary.

## Source kinds

See `provenance/source-catalog.json`:

- `official-round-1-package` — NICCS / PKC forum Round-1 candidate zips
  (`https://www.niccs.org.cn/.../Round 1 candidates/`).
- `latest-author-revision` — later author packages, preferred for Implementation
  when newer than the Round-1 zip.

Spec inventory: `provenance/specs-from-round1.json`.

## Integrity

```sh
# Verify every tracked author Implementation file against the frozen manifest
while read -r hash path; do
  echo "$hash  $path"
done < provenance/sources.sha256 | sha256sum -c -
```

Migrated from `Tongsuo-Project/NGCC-provider` @ `1fa5763` (`ngcc/upstream/`,
excluding `api/`). Spec documents added from Round-1 packages in a later commit.
