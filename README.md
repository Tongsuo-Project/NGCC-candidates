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
| `…/Basic information/` (and variants) | Basic algorithm information forms |
| `…/中文资料/`, `Supporting Documentation/` | Extra non-IP docs when present in the package |
| `provenance/` | Catalog, archive checksums, integration policy, Spec inventory |
| `PROVENANCE.json` | Origin of this tree |

**Not included:** harness / ABI glue (lives only in NGCC-provider), Test Vectors,
and pure Intellectual Property statement PDFs.

## Policy

- Author algorithm **Implementation** sources must remain unmodified.
- Prefer author PRs into this repository for algorithm fixes; NGCC-provider should
  consume a pinned commit/tag of this repo rather than editing trees in place.
- Spec PDFs are taken from official NICCS Round-1 candidate packages (even when
  the Implementation tree follows a later author revision).

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
