# 2026-10-27 handoff complete

Development root: this game_tool / Naru_Studio repository.
Confirmed architecture: independent C++ engine, Qt editor UI, Python game scripts.

Initial transfer copied 548 files and verified SHA-256 for every copied file.
Unity REA metadata/type observations and public-reference-source research were added afterward.
MIGRATION_MANIFEST.json records the initial snapshot; HANDOFF_MANIFEST.json records the additions.

Start with README.md, AGENTS.md, NARU_NEXT_STEPS.md and SESSION_STATE.md.
The C++/Qt/Python product is not implemented yet. Next: check the development environment,
pin compatible versions, then connect a minimal CMake core test, Qt window and Python embedding.
app/ is the older Godot experiment only (39 passing checks), not the product engine.

Research includes RPG Maker, Godot and Unity, with raw REA evidence and stated coverage limits.
Local Unity reference-source text is excluded from Git; its acquisition manifest and downloader
allow another PC to retrieve the pinned public source. Original Unity binaries are not copied.
Godot research binaries are local-only and excluded from Git. Ghidra installation stays at the
old configured location so the existing MCP registration continues to work.

The original game/engine-tool copy is retained as an archive. Continue development here.
The old game directory remains for game planning, scripts and assets.

Git remote target: https://github.com/Aristia0930/Naru_Studio.git
Remote push is verified separately against the committed local HEAD; this file is not itself proof of push.
