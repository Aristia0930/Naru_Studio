extends SceneTree

const Document = preload("res://core/document.gd")
const Runner = preload("res://core/runner.gd")
const Exporter = preload("res://core/exporter.gd")
var checks: Array = []
var studio: Control
var output: String

func _initialize() -> void:
	call_deferred("_run")

func check(name: String, condition: bool) -> void:
	checks.append({"name": name, "passed": condition})
	print(("PASS " if condition else "FAIL ") + name)

func wait_until(predicate: Callable, seconds: float = 3) -> bool:
	var deadline = Time.get_ticks_msec() + int(seconds * 1000)
	while not predicate.call() and Time.get_ticks_msec() < deadline:
		await process_frame
	return predicate.call()

func _run() -> void:
	output = ProjectSettings.globalize_path("res://test-output/run-" + str(Time.get_ticks_usec()))
	DirAccess.make_dir_recursive_absolute(output)
	studio = load("res://main.tscn").instantiate()
	root.add_child(studio)
	await process_frame
	var document = studio.document
	check("Editor shell builds with event/code/asset tabs", studio.tabs.get_tab_count() == 4 and studio.event_list.item_count == 3)
	check("Default project validates", Document.validate(document.data).is_empty())
	var original = document.data.duplicate(true)
	studio._add_node()
	check("UI add object updates document and list", studio._scene().nodes.size() == 2 and studio.node_list.item_count == 2)
	studio._history(false)
	check("Undo restores original objects", document.data == original)
	studio._history(true)
	check("Redo restores edited objects", studio._scene().nodes.size() == 2)
	studio._history(false)
	studio.code_edit.text = "extends RefCounted\nfunc run(ctx):\n\tctx.set_value(\"from_code\", 9)\n\tctx.log(\"hook executed\")\n\tawait ctx.wait(0.01)\n"
	studio._add_node()
	check("Pending code survives a different editing action", document.data.scripts[0].source.contains("from_code"))
	studio._history(false)
	var node = studio._scene().nodes[0]
	studio._change(node, "x", 111.0)
	check("Inspector change updates document", node.x == 111.0)
	var project_file = output.path_join("project/project.json")
	check("First project save succeeds", document.save_project(project_file).is_empty())
	check("Saved document becomes clean", not document.dirty())
	var image = Image.create(32, 32, false, Image.FORMAT_RGBA8)
	image.fill(Color("#54d6bd"))
	var image_file = output.path_join("tile.png")
	image.save_png(image_file)
	check("Image imports into project assets", document.import_asset(image_file).is_empty() and document.data.assets.size() == 1)
	check("Duplicate image import uses existing identity", document.import_asset(image_file).is_empty() and document.data.assets.size() == 1)
	var wav = AudioStreamWAV.new()
	wav.format = AudioStreamWAV.FORMAT_16_BITS
	wav.mix_rate = 22050
	wav.data = PackedByteArray()
	var silence = PackedByteArray()
	silence.resize(4410)
	wav.data = silence
	var wav_file = output.path_join("tone.wav")
	wav.save_to_wav(wav_file)
	check("WAV imports and decodes", document.import_asset(wav_file).is_empty() and document.data.assets.size() == 2)
	var first_asset = document.data.assets[0]
	document.data.scenes[0].nodes[0].asset = first_asset.id
	check("Save overwrites atomically with backup", document.save_project(project_file).is_empty() and FileAccess.file_exists(project_file + ".bak"))
	var loaded = Document.new()
	check("Reload restores equivalent JSON content", loaded.load_project(project_file).is_empty() and loaded.data == JSON.parse_string(JSON.stringify(document.data)))
	var saved_before = loaded.data.duplicate(true)
	var bad = FileAccess.open(output.path_join("broken.json"), FileAccess.WRITE)
	bad.store_string("{broken")
	bad.close()
	check("Malformed JSON leaves active project untouched", not loaded.load_project(output.path_join("broken.json")).is_empty() and loaded.data == saved_before)
	var corrupt = saved_before.duplicate(true)
	corrupt.scenes[0].nodes[0].x = "bad"
	check("Malformed numeric property rejected", not Document.validate(corrupt).is_empty())
	corrupt = saved_before.duplicate(true)
	corrupt.scenes[0].events[1].duration = {}
	check("Malformed event parameter rejected", not Document.validate(corrupt).is_empty())
	check("Traversal asset path rejected", Document.safe_asset_path(output, "../outside.png").is_empty())
	check("Missing run(ctx) contract rejected", not Runner.compile_source("extends RefCounted\nfunc other():\n\tpass\n").error.is_empty())
	var clone_file = output.path_join("copy/project.json")
	check("Save As carries asset files", loaded.save_project(clone_file).is_empty() and FileAccess.file_exists(output.path_join("copy").path_join(first_asset.path)))
	studio._refresh()
	var events = studio._scene().events
	events[1].duration = 0.03
	studio.runner.auto_advance = true
	var before_preview = document.data.duplicate(true)
	var completed = [false]
	studio.runner.state_changed.connect(func(state): if state == "completed": completed[0] = true)
	studio._play(0)
	check("UI events and asynchronous code hook complete", await wait_until(func(): return completed[0]))
	check("Code hook mutates execution variables", studio.runner.variables.get("from_code") == 9)
	check("Animation reaches runtime destination", is_equal_approx(studio.runner.scene.nodes[0].x, 700.0))
	check("Preview leaves editable project unchanged", document.data == before_preview)
	check("Preview image texture loads without importer cache", studio.stage.texture_for(first_asset.id) != null)
	var audio = document.data.assets[1]
	check("Audio channel playback can start", studio.runner.play_audio(audio.id, "test", -15, true) and studio.runner.players.has("test"))
	studio._stop()
	await process_frame
	check("Stop clears players and tweens and restores edit mode", studio.runner.players.is_empty() and studio.runner.tweens.is_empty() and studio.stage.interactive and not studio.runner.running)
	check("Stop restores original placement", studio.stage.scene_data.nodes[0].x == before_preview.scenes[0].nodes[0].x)
	completed[0] = false
	studio._play(2)
	check("Run from selected command completes", await wait_until(func(): return completed[0]))
	check("Run from selected command skips earlier animation", studio.runner.scene.nodes[0].x == before_preview.scenes[0].nodes[0].x)
	studio._stop()
	studio.runner.auto_advance = false
	studio._play(0)
	await process_frame
	check("Dialogue waits for user input", studio.runner.dialogue_pending)
	studio._stop()
	await process_frame
	check("Stop cancels pending dialogue", not studio.runner.dialogue_pending and not studio.runner.running)
	var failing = document.data.duplicate(true)
	failing.scripts[0].source = "extends RefCounted\nfunc run(ctx):\n\tctx.fail(\"intentional hook failure\")\n"
	studio.runner.start(failing, studio.scene_id, project_file.get_base_dir(), 2)
	await process_frame
	check("Code-reported errors stop preview", not studio.runner.running)
	var last = studio.log_list.get_item_metadata(studio.log_list.item_count - 1)
	check("Error logs carry event and scene identity", last.level == "error" and last.location.get("event") == "event_script" and last.location.get("scene") == "scene_intro")
	studio._select_log(studio.log_list.item_count - 1)
	check("Selecting error returns to failing command", studio.event_index == 2 and studio.mode == "event")
	var missing = document.data.duplicate(true)
	missing.assets[0].path = "assets/not-found.png"
	studio.runner.start(missing, studio.scene_id, project_file.get_base_dir())
	check("Missing assets fail before execution", not studio.runner.running)
	var game_dir = output.path_join("exported-game")
	check("Portable game project exports", Exporter.export_game(document.data, project_file.get_base_dir(), game_dir, false).is_empty())
	check("Export contains isolated runtime and assets", FileAccess.file_exists(game_dir.path_join("player.gd")) and FileAccess.file_exists(game_dir.path_join(first_asset.path)) and not FileAccess.file_exists(game_dir.path_join("main.gd")))
	check("Export refuses existing destination", not Exporter.export_game(document.data, project_file.get_base_dir(), game_dir, false).is_empty())
	studio._stop()
	studio._refresh()
	var report = {"checks": checks, "exported_game": game_dir, "log_file": studio.log_path, "output": output}
	var file = FileAccess.open("res://test-output/latest.json", FileAccess.WRITE)
	file.store_string(JSON.stringify(report, "  "))
	file.close()
	studio.queue_free()
	await process_frame
	await process_frame
	quit(0 if checks.all(func(item): return item.passed) else 1)
