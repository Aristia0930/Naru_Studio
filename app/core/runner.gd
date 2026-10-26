extends Node

const Document = preload("res://core/document.gd")
const Context = preload("res://core/script_context.gd")
signal logged(level: String, message: String, location: Dictionary)
signal dialogue_changed(speaker: String, text: String)
signal state_changed(state: String)
signal command_started(index: int)

var stage: Control
var snapshot: Dictionary = {}
var scene: Dictionary = {}
var asset_root: String = ""
var variables: Dictionary = {}
var running: bool = false
var generation: int = 0
var cursor: int = -1
var dialogue_pending: bool = false
var auto_advance: bool = false
var players: Dictionary = {}
var tweens: Array = []
var error_probe: Callable

static func compile_source(source: String) -> Dictionary:
	var code = GDScript.new()
	code.source_code = source
	var status = code.reload()
	if status != OK:
		return {"error": "코드 문법 오류: " + error_string(status)}
	if code.get_instance_base_type() != "RefCounted":
		return {"error": "코드 이벤트는 extends RefCounted 형식이어야 합니다."}
	var has_run = false
	for method in code.get_script_method_list():
		if method.name == "run" and method.args.size() == 1:
			has_run = true
	if not has_run:
		return {"error": "func run(ctx) 진입 함수가 필요합니다."}
	return {"script": code, "error": ""}

func record(level: String, message: String) -> void:
	var location = {"scene": scene.get("id", ""), "index": cursor}
	if cursor >= 0 and cursor < scene.get("events", []).size():
		location["event"] = scene.events[cursor].get("id", "")
	logged.emit(level, message, location)

func fail(message: String) -> void:
	record("error", message)
	stop("failed")

func start(project: Dictionary, scene_id: String, folder: String, index: int = 0) -> void:
	stop()
	scene = {}
	cursor = -1
	var errors = Document.validate(project)
	if not errors.is_empty():
		fail("\n".join(errors))
		return
	snapshot = project.duplicate(true)
	scene = Document.find(snapshot.scenes, scene_id)
	if scene.is_empty():
		fail("테스트할 장면이 없습니다.")
		return
	asset_root = folder
	variables = {}
	cursor = index
	for asset in snapshot.assets:
		var path = Document.safe_asset_path(folder, asset.path)
		if path.is_empty() or not FileAccess.file_exists(path):
			fail("에셋 파일 없음: " + str(asset.path))
			return
	for item in snapshot.scripts:
		var result = compile_source(item.source)
		if not result.error.is_empty():
			fail("%s: %s" % [item.name, result.error])
			return
	stage.configure(scene, snapshot, folder)
	stage.interactive = false
	stage.grid = false
	running = true
	generation += 1
	state_changed.emit("running")
	record("info", "테스트 시작 · %s · %s번 명령부터" % [scene.name, index + 1])
	_run(generation, index)

func stop(state: String = "stopped") -> void:
	generation += 1
	running = false
	dialogue_pending = false
	for tween in tweens:
		if is_instance_valid(tween):
			tween.kill()
	tweens.clear()
	for player in players.values():
		player.stop()
		player.queue_free()
	players.clear()
	dialogue_changed.emit("", "")
	state_changed.emit(state)

func next_dialogue() -> void:
	dialogue_pending = false

func show_dialogue(speaker: String, text: String, token: int) -> void:
	if not running or token != generation:
		return
	dialogue_pending = true
	dialogue_changed.emit(speaker, text)
	if auto_advance:
		dialogue_pending = false
	while dialogue_pending and running and token == generation:
		await get_tree().process_frame
	if token == generation:
		dialogue_changed.emit("", "")

func wait_seconds(seconds: float, token: int) -> void:
	var deadline = Time.get_ticks_msec() + int(clampf(seconds, 0, 3600) * 1000)
	while running and generation == token and Time.get_ticks_msec() < deadline:
		await get_tree().process_frame

func move_target(id: String, point: Vector2, duration: float, token: int) -> void:
	var item = Document.find(scene.get("nodes", []), id)
	if item.is_empty():
		fail("애니메이션 대상을 찾을 수 없습니다: " + id)
		return
	var start = Vector2(item.x, item.y)
	var tween = create_tween()
	tweens.append(tween)
	tween.tween_method(func(value):
		item.x = value.x
		item.y = value.y
		stage.queue_redraw(), start, point, maxf(0.01, duration)).set_trans(Tween.TRANS_SINE).set_ease(Tween.EASE_IN_OUT)
	while running and generation == token and tween.is_running():
		await get_tree().process_frame
	if tweens.has(tween):
		tweens.erase(tween)

func play_audio(asset_id: String, channel: String, volume: float, loop: bool) -> bool:
	var asset = Document.find(snapshot.assets, asset_id)
	if asset.is_empty() or asset.get("kind") != "audio":
		fail("음원 에셋을 찾을 수 없습니다: " + asset_id)
		return false
	var path = Document.safe_asset_path(asset_root, asset.path)
	var stream: AudioStream
	match path.get_extension().to_lower():
		"mp3": stream = AudioStreamMP3.load_from_file(path)
		"ogg": stream = AudioStreamOggVorbis.load_from_file(path)
		"wav": stream = AudioStreamWAV.load_from_file(path)
	if stream == null:
		fail("음원을 읽을 수 없습니다: " + asset.name)
		return false
	if stream is AudioStreamMP3 or stream is AudioStreamOggVorbis:
		stream.loop = loop
	if stream is AudioStreamWAV and loop:
		stream.loop_mode = AudioStreamWAV.LOOP_FORWARD
		stream.loop_end = int(stream.get_length() * stream.mix_rate)
	stop_audio(channel)
	var player = AudioStreamPlayer.new()
	player.stream = stream
	player.volume_db = clampf(volume, -60, 6)
	add_child(player)
	players[channel] = player
	player.play()
	record("info", "음원 재생 · " + asset.name)
	return true

func stop_audio(channel: String) -> void:
	if players.has(channel):
		players[channel].stop()
		players[channel].queue_free()
		players.erase(channel)

func _run(token: int, start_index: int) -> void:
	for index in range(start_index, scene.events.size()):
		if not running or generation != token:
			return
		cursor = index
		command_started.emit(index)
		var event = scene.events[index]
		record("info", "%02d · %s" % [index + 1, Document.COMMANDS.get(event.type, event.type)])
		match event.type:
			"dialogue.say":
				await show_dialogue(str(event.get("speaker", "")), str(event.get("text", "")), token)
			"wait":
				await wait_seconds(float(event.get("duration", 0.5)), token)
			"node.show":
				Document.find(scene.nodes, event.target).visible = bool(event.get("visible", true))
				stage.queue_redraw()
			"node.move":
				await move_target(event.target, Vector2(float(event.get("x", 0)), float(event.get("y", 0))), float(event.get("duration", 0.5)), token)
			"audio.play":
				play_audio(str(event.get("asset", "")), str(event.get("channel", "bgm")), float(event.get("volume", -12)), bool(event.get("loop", false)))
			"audio.stop":
				stop_audio(str(event.get("channel", "bgm")))
			"variable.set":
				variables[str(event.get("key", "value"))] = event.get("value", 0)
			"script.call":
				var item = Document.find(snapshot.scripts, event.script)
				var result = compile_source(item.source)
				if not result.error.is_empty():
					fail(result.error)
					return
				var context = Context.new()
				context.runner = self
				context.generation = token
				var instance = result.script.new()
				await instance.run(context)
				if error_probe.is_valid(): error_probe.call()
		# Yield even for immediate commands so logs and stop remain responsive.
		await get_tree().process_frame
	if running and generation == token:
		record("info", "이벤트 완료 · 실행 변수: " + JSON.stringify(variables))
		state_changed.emit("completed")
