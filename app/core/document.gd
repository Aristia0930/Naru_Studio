extends RefCounted

const VERSION = 1
const COMMANDS = {
	"dialogue.say": "대사 표시", "wait": "시간 대기", "node.show": "대상 표시/숨김",
	"node.move": "이동 애니메이션", "audio.play": "음악/효과음 재생",
	"audio.stop": "음악/효과음 정지", "variable.set": "변수 설정", "script.call": "코드 이벤트 실행"
}
const SCRIPT_TEMPLATE = "extends RefCounted\n\nfunc run(ctx):\n\tvar count = ctx.get_value(\"visits\", 0) + 1\n\tctx.set_value(\"visits\", count)\n\tctx.log(\"코드 이벤트 실행: %s회\" % count)\n\tawait ctx.say(\"코드 이벤트\", \"UI 이벤트와 코드가 연결되었습니다.\")\n"

var data: Dictionary = {}
var project_path: String = ""
var past: Array = []
var future: Array = []
var saved_text: String = ""

static func uid(prefix: String) -> String:
	return prefix + "_" + str(Time.get_ticks_usec()) + "_" + str(randi() % 100000)

func fresh() -> void:
	data = {
		"formatVersion": VERSION, "name": "나의 첫 게임", "startScene": "scene_intro",
		"assets": [], "characters": [{"id": "character_sample", "name": "샘플 캐릭터", "hp": 100, "description": "제작툴 테스트용 데이터"}],
		"scripts": [{"id": "script_sample", "name": "첫 코드 이벤트", "source": SCRIPT_TEMPLATE}],
		"scenes": [{"id": "scene_intro", "name": "시작 장면", "background": "#192c3b", "nodes": [
			{"id": "node_card", "name": "샘플 오브젝트", "asset": "", "x": 420.0, "y": 180.0, "width": 160.0, "height": 220.0, "color": "#54d6bd", "visible": true}
		], "events": [
			{"id": "event_welcome", "type": "dialogue.say", "speaker": "안내", "text": "첫 장면입니다. 아래 다음 버튼으로 진행하세요."},
			{"id": "event_move", "type": "node.move", "target": "node_card", "x": 700.0, "y": 180.0, "duration": 0.6},
			{"id": "event_script", "type": "script.call", "script": "script_sample"}
		]}]
	}
	project_path = ""
	past.clear()
	future.clear()
	saved_text = ""

func checkpoint() -> void:
	past.append(data.duplicate(true))
	if past.size() > 80:
		past.pop_front()
	future.clear()

func undo() -> bool:
	if past.is_empty():
		return false
	future.append(data.duplicate(true))
	data = past.pop_back()
	return true

func redo() -> bool:
	if future.is_empty():
		return false
	past.append(data.duplicate(true))
	data = future.pop_back()
	return true

func dirty() -> bool:
	return JSON.stringify(data) != saved_text

static func find(items: Array, id: String) -> Dictionary:
	for item in items:
		if item.get("id", "") == id:
			return item
	return {}

static func safe_asset_path(base: String, relative: String) -> String:
	if relative.is_empty() or relative.is_absolute_path() or relative.contains(":") or relative.replace("\\", "/").split("/").has(".."):
		return ""
	return base.path_join(relative)

static func validate(candidate: Variant) -> Array:
	var errors: Array = []
	if not candidate is Dictionary:
		return ["프로젝트 최상위 데이터는 객체여야 합니다."]
	if candidate.get("formatVersion") != VERSION:
		return ["지원하지 않는 프로젝트 버전입니다."]
	for key in ["scenes", "assets", "scripts", "characters"]:
		if not candidate.get(key) is Array:
			return ["필수 목록이 없습니다: " + key]
	if candidate.scenes.is_empty():
		return ["장면이 하나 이상 필요합니다."]
	var ids: Dictionary = {}
	var groups: Array = [candidate.scenes, candidate.assets, candidate.scripts, candidate.characters]
	for scene in candidate.scenes:
		if not scene is Dictionary or not scene.get("nodes") is Array or not scene.get("events") is Array:
			return ["장면의 nodes/events 목록이 잘못되었습니다."]
		groups.append(scene.nodes)
		groups.append(scene.events)
		if not scene.get("name") is String or not Color.html_is_valid(str(scene.get("background", ""))):
			errors.append("장면 이름 또는 배경색이 잘못되었습니다.")
	for group in groups:
		for item in group:
			if not item is Dictionary or not item.get("id") is String or item.id.is_empty():
				return ["모든 항목에 문자열 ID가 필요합니다."]
			if ids.has(item.id):
				errors.append("중복 ID: " + item.id)
			ids[item.id] = true
	if find(candidate.scenes, str(candidate.get("startScene", ""))).is_empty():
		errors.append("시작 장면을 찾을 수 없습니다.")
	for asset in candidate.assets:
		if not asset.get("name") is String or not asset.get("kind") in ["audio", "image"]:
			errors.append("에셋 이름 또는 종류 오류: " + str(asset.id))
		if not asset.get("path") is String or safe_asset_path("assets", asset.get("path", "")).is_empty():
			errors.append("잘못된 에셋 경로: " + str(asset.id))
	for script in candidate.scripts:
		if not script.get("source") is String or not script.get("name") is String:
			errors.append("코드 내용이 없습니다: " + str(script.id))
	for scene in candidate.scenes:
		for item in scene.nodes:
			if not item.get("name") is String or not item.get("visible") is bool or not Color.html_is_valid(str(item.get("color", ""))):
				errors.append("오브젝트 이름·표시·색상 오류: " + str(item.id))
			for key in ["x", "y", "width", "height"]:
				if not (item.get(key) is float or item.get(key) is int) or not is_finite(float(item.get(key, 0))):
					errors.append("오브젝트 숫자 값 오류: " + str(item.id) + "." + key)
			if not str(item.get("asset", "")).is_empty() and find(candidate.assets, str(item.asset)).is_empty():
				errors.append("오브젝트 에셋 참조 없음: " + str(item.id))
		for event in scene.events:
			var kind = event.get("type", "")
			if not COMMANDS.has(kind):
				errors.append("알 수 없는 명령: " + str(kind))
			if kind in ["node.move", "node.show"] and find(scene.nodes, str(event.get("target", ""))).is_empty():
				errors.append("명령의 대상이 없습니다: " + str(event.id))
			if kind == "script.call" and find(candidate.scripts, str(event.get("script", ""))).is_empty():
				errors.append("명령의 코드가 없습니다: " + str(event.id))
			if kind == "audio.play" and find(candidate.assets, str(event.get("asset", ""))).is_empty():
				errors.append("명령의 음원이 없습니다: " + str(event.id))
			var numbers: Array = []
			if kind in ["wait", "node.move"]: numbers.append("duration")
			if kind == "node.move": numbers.append_array(["x", "y"])
			if kind == "audio.play": numbers.append("volume")
			for key in numbers:
				if not (event.get(key) is float or event.get(key) is int) or not is_finite(float(event.get(key, 0))):
					errors.append("명령 숫자 값 오류: " + str(event.id) + "." + key)
			if kind == "dialogue.say" and (not event.get("speaker") is String or not event.get("text") is String):
				errors.append("대사 내용은 문자열이어야 합니다: " + str(event.id))
			if kind in ["audio.play", "audio.stop"] and not event.get("channel") is String:
				errors.append("음원 채널 이름이 없습니다: " + str(event.id))
			if kind == "node.show" and not event.get("visible") is bool:
				errors.append("표시 여부는 참/거짓이어야 합니다: " + str(event.id))
	return errors

func load_project(path: String) -> String:
	var file = FileAccess.open(path, FileAccess.READ)
	if file == null:
		return "프로젝트를 열 수 없습니다: " + error_string(FileAccess.get_open_error())
	var parser = JSON.new()
	if parser.parse(file.get_as_text()) != OK:
		return "JSON %s행: %s" % [parser.get_error_line(), parser.get_error_message()]
	var errors = validate(parser.data)
	if not errors.is_empty():
		return "\n".join(errors)
	data = parser.data
	project_path = path
	past.clear()
	future.clear()
	saved_text = JSON.stringify(data)
	return ""

func save_project(path: String) -> String:
	var errors = validate(data)
	if not errors.is_empty():
		return "\n".join(errors)
	var folder = path.get_base_dir()
	var result = DirAccess.make_dir_recursive_absolute(folder)
	if result != OK:
		return "폴더 생성 실패: " + error_string(result)
	# A different directory must carry all referenced assets with it.
	if not project_path.is_empty() and folder != project_path.get_base_dir():
		for asset in data.assets:
			var source = safe_asset_path(project_path.get_base_dir(), asset.path)
			var destination = safe_asset_path(folder, asset.path)
			if source.is_empty() or destination.is_empty() or not FileAccess.file_exists(source):
				return "복사할 에셋이 없습니다: " + str(asset.path)
			DirAccess.make_dir_recursive_absolute(destination.get_base_dir())
			result = DirAccess.copy_absolute(source, destination)
			if result != OK:
				return "에셋 복사 실패: " + error_string(result)
	var temporary = path + ".tmp"
	var file = FileAccess.open(temporary, FileAccess.WRITE)
	if file == null:
		return "임시 저장 실패: " + error_string(FileAccess.get_open_error())
	file.store_string(JSON.stringify(data, "\t"))
	file.flush()
	file.close()
	if FileAccess.file_exists(path):
		result = DirAccess.copy_absolute(path, path + ".bak")
		if result != OK:
			return "기존 파일 백업 실패: " + error_string(result)
	result = DirAccess.rename_absolute(temporary, path)
	if result != OK:
		return "저장 파일 교체 실패: " + error_string(result)
	project_path = path
	saved_text = JSON.stringify(data)
	return ""

func import_asset(source: String) -> String:
	if project_path.is_empty():
		return "에셋을 넣기 전에 프로젝트를 저장하세요."
	var extension = source.get_extension().to_lower()
	if not extension in ["png", "jpg", "jpeg", "webp", "mp3", "wav", "ogg"]:
		return "지원 형식: PNG/JPG/WebP, MP3/WAV/OGG"
	if not FileAccess.file_exists(source):
		return "파일을 찾을 수 없습니다."
	if extension in ["png", "jpg", "jpeg", "webp"]:
		var image = Image.load_from_file(source)
		if image == null or image.is_empty(): return "이미지 파일을 해석할 수 없습니다."
	else:
		var audio: AudioStream
		match extension:
			"mp3": audio = AudioStreamMP3.load_from_file(source)
			"wav": audio = AudioStreamWAV.load_from_file(source)
			"ogg": audio = AudioStreamOggVorbis.load_from_file(source)
		if audio == null: return "음원 파일을 해석할 수 없습니다."
	var id = "asset_" + FileAccess.get_sha256(source).substr(0, 16)
	if not find(data.assets, id).is_empty():
		return ""
	var relative = "assets/" + id + "." + extension
	var destination = project_path.get_base_dir().path_join(relative)
	DirAccess.make_dir_recursive_absolute(destination.get_base_dir())
	if DirAccess.copy_absolute(source, destination) != OK:
		return "에셋 파일 복사 실패"
	checkpoint()
	data.assets.append({"id": id, "name": source.get_file(), "path": relative, "kind": "image" if extension in ["png", "jpg", "jpeg", "webp"] else "audio"})
	return ""
