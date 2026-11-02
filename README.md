# NGCC-candidates

NGCC (Next-Generation Cryptography Competition) is a global call for next-generation commercial cryptography algorithms, launched in 2025 by the Institute of Commercial Cryptography Standards (ICCS). The [notice on collecting next-generation public-key algorithm proposals](https://www.niccs.org.cn/symmbzyjy/tzgg/pc/content/1976155884915003392/content_1976155884915003392.html) officially published the submission requirements and evaluation criteria for public-key cryptographic algorithms and started accepting public-key algorithm proposals.

This repository holds the reference implementations of NGCC public-key proposals (KEM, signature, and two key exchange candidates: kex-06 MAMBA-NIKE, a one-pass, KEM-shaped key exchange, and kex-08 NIIKE, a DH-style non-interactive key exchange), including algorithm sources and specification documents from the submitted packages.

We keep author sources **bit-for-bit identical** to the originals and do not modify them. When newer author releases appear, we try to update this tree to match. Please open an issue if an implementation here is outdated, and include a link to the newer reference implementation when you have one.

The [Source provenance](#source-provenance) tables below list, for every scheme directory in this tree, where that code was taken from. [`provenance/source-catalog.json`](provenance/source-catalog.json) is the machine-readable source of truth (URLs, `source_kind`, check dates, and archive SHA-256 digests).

## Source provenance

This repository is a community archive of submitter reference implementations and specification documents. It is **not** an official NGCC test platform.

Each row answers two questions: **where** the tree for that scheme was taken from, and **when this repository’s catalog recorded/verified** that source. **Catalog as-of** is the catalog field `source_checked_date`. It is the date this archive checked the linked source — **not** an author submission timestamp (those dates are not recorded here).

Official Round-1 packages were published on the [NICCS Round-1 candidates](https://www.niccs.org.cn/niccs/Proposal/Public-Key%20Cryptographic%20Algorithms/Round%201%20candidates/) page. Later author revisions were posted on the [PKC Forum](https://list.niccs.org.cn/archives/list/pkcforum@list.niccs.org.cn/) archives, GitHub, or author sites, as linked in the Source column.

**Source kind**

| Catalog `source_kind` | Label in tables | Meaning |
| --- | --- | --- |
| `official-round-1-package` | Official Round-1 package | Code from the official NICCS Round-1 candidate ZIP on niccs.org.cn |
| `latest-author-revision` | Latest author revision | Code from a later author revision (PKC Forum attachment, GitHub commit or tree, or author-site download), not the original Round-1 ZIP alone |

**SHA-256 (short)** is the first 12 hex characters of `archives[0].sha256`. Full digests and archive paths are in [`provenance/source-catalog.json`](provenance/source-catalog.json).

The tree currently has **76** scheme directories (41 KEM, 33 signature, 2 key exchange). The catalog also records withdrawn **sign-29 (Tins)**, which is **not present** in this tree.

### KEM

| ID | Algorithm | Source kind | Source | Catalog as-of | SHA-256 (short) |
| --- | --- | --- | --- | --- | --- |
| `kem-01` | Aigis-Enc+ | Latest author revision | [Aigis-Enc.zip](https://list.niccs.org.cn/archives/list/pkcforum@list.niccs.org.cn/message/GGSOU5DK46JKEP27W2B3KRRAJWCW7CLZ/attachment/4/Aigis-Enc.zip) | 2026-10-20 | `c8c633aa1ae8` |
| `kem-02` | Amoeba | Official Round-1 package | [Amoeba.zip](https://www.niccs.org.cn/niccs/Proposal/Public-Key%20Cryptographic%20Algorithms/Round%201%20candidates/Amoeba.zip) | 2026-10-20 | `719c438a30cc` |
| `kem-03` | BAG-Loong | Official Round-1 package | [BAG-Loong.zip](https://www.niccs.org.cn/niccs/Proposal/Public-Key%20Cryptographic%20Algorithms/Round%201%20candidates/BAG-Loong.zip) | 2026-10-20 | `2795fd57d3d0` |
| `kem-04` | BAG-Piglet | Official Round-1 package | [BAG-Piglet.zip](https://www.niccs.org.cn/niccs/Proposal/Public-Key%20Cryptographic%20Algorithms/Round%201%20candidates/BAG-Piglet.zip) | 2026-10-20 | `31afe1f2f4e9` |
| `kem-05` | BIKE_MLThre | Official Round-1 package | [BIKE_MLThre.zip](https://www.niccs.org.cn/niccs/Proposal/Public-Key%20Cryptographic%20Algorithms/Round%201%20candidates/BIKE_MLThre.zip) | 2026-10-20 | `3a5d97532003` |
| `kem-06` | BRA | Official Round-1 package | [BRA.zip](https://www.niccs.org.cn/niccs/Proposal/Public-Key%20Cryptographic%20Algorithms/Round%201%20candidates/BRA.zip) | 2026-10-20 | `612a3fe69c7a` |
| `kem-07` | BRQC | Official Round-1 package | [BRQC.zip](https://www.niccs.org.cn/niccs/Proposal/Public-Key%20Cryptographic%20Algorithms/Round%201%20candidates/BRQC.zip) | 2026-10-20 | `2c76bdd4e4df` |
| `kem-08` | BW-KEM | Official Round-1 package | [BW-KEM.zip](https://www.niccs.org.cn/niccs/Proposal/Public-Key%20Cryptographic%20Algorithms/Round%201%20candidates/BW-KEM.zip) | 2026-10-20 | `a1f8a079df7d` |
| `kem-09` | CheetahKEM | Official Round-1 package | [CheetahKEM.zip](https://www.niccs.org.cn/niccs/Proposal/Public-Key%20Cryptographic%20Algorithms/Round%201%20candidates/CheetahKEM.zip) | 2026-10-20 | `fc321e46bac9` |
| `kem-10` | C-Multi-UR-AG | Official Round-1 package | [C-Multi-UR-AG.zip](https://www.niccs.org.cn/niccs/Proposal/Public-Key%20Cryptographic%20Algorithms/Round%201%20candidates/C-Multi-UR-AG.zip) | 2026-10-20 | `c41e372a55e1` |
| `kem-11` | COMPASS-KEM | Official Round-1 package | [COMPASS-KEM.zip](https://www.niccs.org.cn/niccs/Proposal/Public-Key%20Cryptographic%20Algorithms/Round%201%20candidates/COMPASS-KEM.zip) | 2026-10-20 | `8fc838488ac0` |
| `kem-12` | CTL | Official Round-1 package | [CTL.zip](https://www.niccs.org.cn/niccs/Proposal/Public-Key%20Cryptographic%20Algorithms/Round%201%20candidates/CTL.zip) | 2026-10-20 | `512caaf5fa04` |
| `kem-13` | DKEM | Latest author revision | [GitHub commit 5900418088df](https://github.com/dkemdkex/dkem-dkex/commit/5900418088dfa73b624dec1a2d4aaca751230b12) | 2026-10-20 | `76791cc687f5` |
| `kem-14` | DTRU | Official Round-1 package | [DTRU.zip](https://www.niccs.org.cn/niccs/Proposal/Public-Key%20Cryptographic%20Algorithms/Round%201%20candidates/DTRU.zip) | 2026-10-20 | `ed443d6a9e2e` |
| `kem-15` | FLIT | Latest author revision | [GitHub tree c416982d41b7](https://github.com/MathEternal/FLIT-NGCC/tree/c416982d41b70283ed4823319c2a4d8d23e6f055) | 2026-10-20 | `96f72dcc2d83` |
| `kem-16` | HARE | Latest author revision | [hare-implementations-v1.5.zip](https://pqc-hare.info/downloads/hare-implementations-v1.5.zip) | 2026-10-20 | `46d780bdfcd3` |
| `kem-17` | HEP-QC | Official Round-1 package | [HEP-QC.zip](https://www.niccs.org.cn/niccs/Proposal/Public-Key%20Cryptographic%20Algorithms/Round%201%20candidates/HEP-QC.zip) | 2026-10-20 | `3780991127f4` |
| `kem-18` | LoongKEM | Official Round-1 package | [LoongKEM.zip](https://www.niccs.org.cn/niccs/Proposal/Public-Key%20Cryptographic%20Algorithms/Round%201%20candidates/LoongKEM.zip) | 2026-10-20 | `a6e5070647a4` |
| `kem-19` | Lore | Latest author revision | [Lore-Reference-Implementations-20260927.zip](https://list.niccs.org.cn/archives/list/pkcforum@list.niccs.org.cn/message/W6XIOQDXGIUZPVKR3MZW4HZCDJEVYRMR/attachment/4/Lore-Reference-Implementations-20260927.zip) | 2026-10-20 | `b81b265a58dc` |
| `kem-20` | MAMBA-Frost | Official Round-1 package | [MAMBA-Frost.zip](https://www.niccs.org.cn/niccs/Proposal/Public-Key%20Cryptographic%20Algorithms/Round%201%20candidates/MAMBA-Frost.zip) | 2026-10-20 | `ae363dbefd69` |
| `kem-21` | MAMBA-Viper | Official Round-1 package | [MAMBA-Viper.zip](https://www.niccs.org.cn/niccs/Proposal/Public-Key%20Cryptographic%20Algorithms/Round%201%20candidates/MAMBA-Viper.zip) | 2026-10-20 | `cdf52bb77066` |
| `kem-22` | Mithril | Latest author revision | [Mithril-source-codes.zip](https://www.pq-arcane.org/dist/downloads/Mithril-source-codes.zip) | 2026-10-20 | `5e0760538bb4` |
| `kem-23` | Mito | Official Round-1 package | [Mito.zip](https://www.niccs.org.cn/niccs/Proposal/Public-Key%20Cryptographic%20Algorithms/Round%201%20candidates/Mito.zip) | 2026-10-20 | `ee11788a3e8b` |
| `kem-24` | MORNING-Scabbard | Official Round-1 package | [MORNING-Scabbard.zip](https://www.niccs.org.cn/niccs/Proposal/Public-Key%20Cryptographic%20Algorithms/Round%201%20candidates/MORNING-Scabbard.zip) | 2026-10-20 | `fdb6749116bd` |
| `kem-25` | NEV | Latest author revision | [NEV_Implementations.zip](https://list.niccs.org.cn/archives/list/pkcforum@list.niccs.org.cn/message/UZPNPPDYL5APZLO573LFHXX2ZLPVLZDE/attachment/4/NEV_Implementations.zip) | 2026-10-20 | `48e6bc5a3b81` |
| `kem-26` | NSS-HQC | Official Round-1 package | [NSS-HQC.zip](https://www.niccs.org.cn/niccs/Proposal/Public-Key%20Cryptographic%20Algorithms/Round%201%20candidates/NSS-HQC.zip) | 2026-10-20 | `033994e27bf0` |
| `kem-27` | NTRE | Official Round-1 package | [NTRE.zip](https://www.niccs.org.cn/niccs/Proposal/Public-Key%20Cryptographic%20Algorithms/Round%201%20candidates/NTRE.zip) | 2026-10-20 | `9c0bad4c4846` |
| `kem-28` | OAEP-NTRU | Latest author revision | [GitHub tree f846d6ebda76](https://github.com/oaep-ntru/oaepntru/tree/f846d6ebda76af62278f408a6ebff36f1dfc207b) | 2026-10-20 | `114d6b869766` |
| `kem-29` | Polar-KEM | Official Round-1 package | [Polar-KEM.zip](https://www.niccs.org.cn/niccs/Proposal/Public-Key%20Cryptographic%20Algorithms/Round%201%20candidates/Polar-KEM.zip) | 2026-10-20 | `3ae9d4f1a717` |
| `kem-30` | PolarLAC | Official Round-1 package | [PolarLAC.zip](https://www.niccs.org.cn/niccs/Proposal/Public-Key%20Cryptographic%20Algorithms/Round%201%20candidates/PolarLAC.zip) | 2026-10-20 | `906be26ce8d2` |
| `kem-31` | QIMEN-PIKE | Official Round-1 package | [QIMEN-PIKE.zip](https://www.niccs.org.cn/niccs/Proposal/Public-Key%20Cryptographic%20Algorithms/Round%201%20candidates/QIMEN-PIKE.zip) | 2026-10-20 | `eff17fd345bb` |
| `kem-32` | QCTM | Official Round-1 package | [QCTM.zip](https://www.niccs.org.cn/niccs/Proposal/Public-Key%20Cryptographic%20Algorithms/Round%201%20candidates/QCTM.zip) | 2026-10-21 | `24a3986a4fbb` |
| `kem-33` | QUBE | Official Round-1 package | [QUBE.zip](https://www.niccs.org.cn/niccs/Proposal/Public-Key%20Cryptographic%20Algorithms/Round%201%20candidates/QUBE.zip) | 2026-10-20 | `f5eedd8a4bf7` |
| `kem-34` | Rudraksh2 | Official Round-1 package | [Rudraksh2.zip](https://www.niccs.org.cn/niccs/Proposal/Public-Key%20Cryptographic%20Algorithms/Round%201%20candidates/Rudraksh2.zip) | 2026-10-20 | `569d526cf7ab` |
| `kem-35` | Scloud+ | Official Round-1 package | [Scloud+.zip](https://www.niccs.org.cn/niccs/Proposal/Public-Key%20Cryptographic%20Algorithms/Round%201%20candidates/Scloud%2B.zip) | 2026-10-20 | `050aad7293ec` |
| `kem-36` | TRIKE | Latest author revision | [trike-implementations-iccs-v1.6.zip](https://pqc-trike.info/downloads/trike-implementations-iccs-v1.6.zip) | 2026-10-20 | `18145a3ce255` |
| `kem-37` | TriQ-KEM | Official Round-1 package | [TriQ-KEM.zip](https://www.niccs.org.cn/niccs/Proposal/Public-Key%20Cryptographic%20Algorithms/Round%201%20candidates/TriQ-KEM.zip) | 2026-10-20 | `1e4b96e7a849` |
| `kem-38` | UVW-KEM | Official Round-1 package | [UVW-KEM.zip](https://www.niccs.org.cn/niccs/Proposal/Public-Key%20Cryptographic%20Algorithms/Round%201%20candidates/UVW-KEM.zip) | 2026-10-20 | `f9a1b135ea16` |
| `kem-39` | Weaver | Latest author revision | [GitHub tree 09369c281856](https://github.com/Akutamatsu/weaverkem/tree/09369c28185641d6d7ad044252b536d977d9a4ae) | 2026-10-20 | `5409ec043d48` |
| `kem-40` | YuanYang.KEM | Official Round-1 package | [YuanYang.KEM.zip](https://www.niccs.org.cn/niccs/Proposal/Public-Key%20Cryptographic%20Algorithms/Round%201%20candidates/YuanYang.KEM.zip) | 2026-10-20 | `fe1bf3d78272` |
| `kem-41` | ZEN | Latest author revision | [ZEN-20261030.zip](https://light-lattice.org/zen/implementations/20261030/ZEN-20261030.zip) | 2026-10-31 | `fb0933c53698` |

`kem-32` (QCTM) stays on the official Round-1 ZIP. The catalog notes a 2026-10-08 team announcement of a revision; as of the 2026-10-21 catalog check, no downloadable revised source was found and the official package hash was unchanged. This tree is that official package, not an announced revision.

### Signature

| ID | Algorithm | Source kind | Source | Catalog as-of | SHA-256 (short) |
| --- | --- | --- | --- | --- | --- |
| `sign-01` | Aigis-Sig+ | Latest author revision | [Aigis-sig_Implementations_v1.0.1.zip](https://list.niccs.org.cn/archives/list/pkcforum@list.niccs.org.cn/message/2BWLA2SMHII22YXJM6EWHVGE26WKG2PG/attachment/4/Aigis-sig_Implementations_v1.0.1.zip) | 2026-10-20 | `0b135bc02513` |
| `sign-02` | BiT | Latest author revision | [BiT-20261012.zip](https://light-lattice.org/bit/implementations/20261012/BiT-20261012.zip) | 2026-10-20 | `f798dc3ca559` |
| `sign-03` | cedrus+c | Latest author revision | [GitHub commit 81e737ebee39](https://github.com/zhong-h/CEDRUSPLUSC-SM3/commit/81e737ebee3985c19ea052a6f958d536fd6d44ca) | 2026-10-20 | `5ddc4cd733d2` |
| `sign-04` | cedrus-α | Official Round-1 package | [cedrus-α.zip](https://www.niccs.org.cn/niccs/Proposal/Public-Key%20Cryptographic%20Algorithms/Round%201%20candidates/cedrus-%CE%B1.zip) | 2026-10-20 | `b90559ca94bd` |
| `sign-05` | Chinith | Official Round-1 package | [Chinith.zip](https://www.niccs.org.cn/niccs/Proposal/Public-Key%20Cryptographic%20Algorithms/Round%201%20candidates/Chinith.zip) | 2026-10-20 | `4d31ba3fe871` |
| `sign-06` | COMPASS-SIG | Official Round-1 package | [COMPASS-SIG.zip](https://www.niccs.org.cn/niccs/Proposal/Public-Key%20Cryptographic%20Algorithms/Round%201%20candidates/COMPASS-SIG.zip) | 2026-10-20 | `ce88066506fe` |
| `sign-07` | CS | Official Round-1 package | [CS.zip](https://www.niccs.org.cn/niccs/Proposal/Public-Key%20Cryptographic%20Algorithms/Round%201%20candidates/CS.zip) | 2026-10-20 | `c790d31cd4a2` |
| `sign-08` | DARTS | Latest author revision | [GitHub commit 98687cf6910c](https://github.com/MathEternal/DARTS-NGCC/commit/98687cf6910c1409d188464c9ceff69cbf409cc4) | 2026-10-31 | `6974090d6b1c` |
| `sign-09` | DOVE | Official Round-1 package | [DOVE.zip](https://www.niccs.org.cn/niccs/Proposal/Public-Key%20Cryptographic%20Algorithms/Round%201%20candidates/DOVE.zip) | 2026-10-20 | `19169e545f6a` |
| `sign-10` | Facto-DSA | Official Round-1 package | [Facto-DSA.zip](https://www.niccs.org.cn/niccs/Proposal/Public-Key%20Cryptographic%20Algorithms/Round%201%20candidates/Facto-DSA.zip) | 2026-10-20 | `b3de38fcc2d9` |
| `sign-11` | Flextree | Official Round-1 package | [Flextree.zip](https://www.niccs.org.cn/niccs/Proposal/Public-Key%20Cryptographic%20Algorithms/Round%201%20candidates/Flextree.zip) | 2026-10-20 | `f31d434ac297` |
| `sign-12` | Galas Signature | Official Round-1 package | [Galas Signature.zip](https://www.niccs.org.cn/niccs/Proposal/Public-Key%20Cryptographic%20Algorithms/Round%201%20candidates/Galas%20Signature.zip) | 2026-10-20 | `98d57af868fa` |
| `sign-13` | GreatWall | Latest author revision | [GitHub commit 4732e167cb21](https://github.com/GreatWall-Crypto/NGCC-GreatWall/commit/4732e167cb219eeba9d8852a6a67840f28f3a28e) | 2026-10-20 | `8449d5600b2c` |
| `sign-14` | Lynxer | Official Round-1 package | [Lynxer.zip](https://www.niccs.org.cn/niccs/Proposal/Public-Key%20Cryptographic%20Algorithms/Round%201%20candidates/Lynxer.zip) | 2026-10-20 | `34d863f7df89` |
| `sign-15` | MORNING-ATLAS | Official Round-1 package | [MORNING-ATLAS.zip](https://www.niccs.org.cn/niccs/Proposal/Public-Key%20Cryptographic%20Algorithms/Round%201%20candidates/MORNING-ATLAS.zip) | 2026-10-20 | `c796b106a7d4` |
| `sign-16` | Octarine | Latest author revision | [Octarine-source-codes.zip](https://www.pq-arcane.org/dist/downloads/Octarine-source-codes.zip) | 2026-10-20 | `f95e3764feb0` |
| `sign-17` | OPS Digital Signature Algorithm | Official Round-1 package | [OPS Digital Signature Algorithm.zip](https://www.niccs.org.cn/niccs/Proposal/Public-Key%20Cryptographic%20Algorithms/Round%201%20candidates/OPS%20Digital%20Signature%20Algorithm.zip) | 2026-10-20 | `02322c679225` |
| `sign-18` | Origami | Official Round-1 package | [Origami.zip](https://www.niccs.org.cn/niccs/Proposal/Public-Key%20Cryptographic%20Algorithms/Round%201%20candidates/Origami.zip) | 2026-10-20 | `e34f18832e96` |
| `sign-19` | Phoenix | Official Round-1 package | [Phoenix.zip](https://www.niccs.org.cn/niccs/Proposal/Public-Key%20Cryptographic%20Algorithms/Round%201%20candidates/Phoenix.zip) | 2026-10-20 | `ec801791976a` |
| `sign-20` | QingLuan | Official Round-1 package | [QingLuan.zip](https://www.niccs.org.cn/niccs/Proposal/Public-Key%20Cryptographic%20Algorithms/Round%201%20candidates/QingLuan.zip) | 2026-10-20 | `2eb8f3ed205e` |
| `sign-21` | ReSolveD-alpha | Official Round-1 package | [ReSolveD-alpha.zip](https://www.niccs.org.cn/niccs/Proposal/Public-Key%20Cryptographic%20Algorithms/Round%201%20candidates/ReSolveD-alpha.zip) | 2026-10-20 | `499821538d7f` |
| `sign-22` | Rhyme | Official Round-1 package | [Rhyme.zip](https://www.niccs.org.cn/niccs/Proposal/Public-Key%20Cryptographic%20Algorithms/Round%201%20candidates/Rhyme.zip) | 2026-10-20 | `7b509d21c667` |
| `sign-23` | shuttle | Latest author revision | [GitHub commit 5169f625ff58](https://github.com/pq-spin/Shuttle_Implementation/commit/5169f625ff582f6f05db2c0f11de25453b89db4e) | 2026-10-20 | `1730bed96847` |
| `sign-24` | Sigurd | Latest author revision | [GitHub commit 302eeeb566e8](https://github.com/QinLangT/Sigurd/commit/302eeeb566e8f653cc8ad2c7c675024e409b7c28) | 2026-10-20 | `0cd3fa6df722` |
| `sign-25` | SQIsign2D2 | Official Round-1 package | [SQIsign2D2.zip](https://www.niccs.org.cn/niccs/Proposal/Public-Key%20Cryptographic%20Algorithms/Round%201%20candidates/SQIsign2D2.zip) | 2026-10-20 | `cf635de5eebb` |
| `sign-26` | SQIsign2D-push12 | Official Round-1 package | [SQIsign2D-push12.zip](https://www.niccs.org.cn/niccs/Proposal/Public-Key%20Cryptographic%20Algorithms/Round%201%20candidates/SQIsign2D-push12.zip) | 2026-10-20 | `98b7e4ffddfe` |
| `sign-27` | SQIsignTriangle | Official Round-1 package | [SQIsignTriangle.zip](https://www.niccs.org.cn/niccs/Proposal/Public-Key%20Cryptographic%20Algorithms/Round%201%20candidates/SQIsignTriangle.zip) | 2026-10-20 | `f4afa484e450` |
| `sign-28` | SYDO | Official Round-1 package | [SYDO.zip](https://www.niccs.org.cn/niccs/Proposal/Public-Key%20Cryptographic%20Algorithms/Round%201%20candidates/SYDO.zip) | 2026-10-20 | `c4ae5b27a188` |
| `sign-30` | TRINE | Official Round-1 package | [TRINE.zip](https://www.niccs.org.cn/niccs/Proposal/Public-Key%20Cryptographic%20Algorithms/Round%201%20candidates/TRINE.zip) | 2026-10-20 | `4b22f54b0750` |
| `sign-31` | TSUOV | Official Round-1 package | [TSUOV.zip](https://www.niccs.org.cn/niccs/Proposal/Public-Key%20Cryptographic%20Algorithms/Round%201%20candidates/TSUOV.zip) | 2026-10-20 | `7ab1effc5fe9` |
| `sign-32` | UVW signature | Official Round-1 package | [UVW signature.zip](https://www.niccs.org.cn/niccs/Proposal/Public-Key%20Cryptographic%20Algorithms/Round%201%20candidates/UVW%20signature.zip) | 2026-10-20 | `bbfa8dad5ee5` |
| `sign-33` | VDOO | Official Round-1 package | [VDOO.zip](https://www.niccs.org.cn/niccs/Proposal/Public-Key%20Cryptographic%20Algorithms/Round%201%20candidates/VDOO.zip) | 2026-10-20 | `4b7bb0f15388` |
| `sign-34` | YuanYang.DSA | Official Round-1 package | [YuanYang.DSA.zip](https://www.niccs.org.cn/niccs/Proposal/Public-Key%20Cryptographic%20Algorithms/Round%201%20candidates/YuanYang.DSA.zip) | 2026-10-20 | `1eb45f24ad8f` |

**sign-29 (Tins)** is withdrawn and is not present in this tree. The catalog still records the official Round-1 ZIP URL for the record; this repository does not keep that code.

### Key exchange

| ID | Algorithm | Source kind | Source | Catalog as-of | SHA-256 (short) |
| --- | --- | --- | --- | --- | --- |
| `kex-06` | MAMBA-NIKE | Official Round-1 package | [MAMBA-NIKE.zip](https://www.niccs.org.cn/niccs/Proposal/Public-Key%20Cryptographic%20Algorithms/Round%201%20candidates/MAMBA-NIKE.zip) | 2026-10-22 | `79b53ff72612` |
| `kex-08` | NIIKE | Official Round-1 package | [NIIKE.zip](https://www.niccs.org.cn/niccs/Proposal/Public-Key%20Cryptographic%20Algorithms/Round%201%20candidates/NIIKE.zip) | 2026-10-22 | `4f79f9a62aed` |
