extends RefCounted

const Document = preload("res://core/document.gd")

static func export_game(data: Dictionary, asset_root: String, destination: String, include_engine: bool = false) -> String:
	var issues = Document.validate(data)
	if not issues.is_empty(): return "\n".join(issues)
	if DirAccess.dir_exists_absolute(destination): return "기존 폴더에는 내보내지 않습니다. 새 폴더를 선택하세요."
	for asset in data.assets:
		var source = Document.safe_asset_path(asset_root, asset.path)
		if source.is_empty() or not FileAccess.file_exists(source): return "내보낼 에셋 없음: " + str(asset.path)
	var error = DirAccess.make_dir_recursive_absolute(destination)
	if error != OK: return "내보내기 폴더 생성 실패: " + error_string(error)
	var files = ["core/document.gd", "core/runner.gd", "core/script_context.gd", "ui/stage.gd", "player.gd", "player.tscn"]
	for relative in files:
		var target = destination.path_join(relative)
		DirAccess.make_dir_recursive_absolute(target.get_base_dir())
		error = DirAccess.copy_absolute(ProjectSettings.globalize_path("res://" + relative), target)
		if error != OK: return "런타임 복사 실패: " + relative + " · " + error_string(error)
	for asset in data.assets:
		var target = Document.safe_asset_path(destination, asset.path)
		DirAccess.make_dir_recursive_absolute(target.get_base_dir())
		error = DirAccess.copy_absolute(Document.safe_asset_path(asset_root, asset.path), target)
		if error != OK: return "에셋 복사 실패: " + asset.path
	var project = "config_version=5\n\n[application]\nconfig/name=\"Studio Game\"\nrun/main_scene=\"res://player.tscn\"\n\n[display]\nwindow/size/viewport_width=960\nwindow/size/viewport_height=640\nwindow/stretch/mode=\"canvas_items\"\n\n[rendering]\nrenderer/rendering_method=\"gl_compatibility\"\n"
	for pair in [["project.godot", project], ["game.json", JSON.stringify(data, "\t")]]:
		var file = FileAccess.open(destination.path_join(pair[0]), FileAccess.WRITE)
		if file == null: return "내보내기 파일 저장 실패: " + str(pair[0])
		file.store_string(pair[1])
		file.close()
	if include_engine:
		error = DirAccess.copy_absolute(OS.get_executable_path(), destination.path_join("Game.exe"))
		if error != OK: return "PC 런타임 복사 실패: " + error_string(error)
		var command = FileAccess.open(destination.path_join("Start Game.cmd"), FileAccess.WRITE)
		if command == null: return "게임 시작 파일 생성 실패"
		command.store_string("@echo off\r\nstart \"\" \"%~dp0Game.exe\" --path \"%~dp0.\" --log-file \"%~dp0game.log\"\r\n")
		command.close()
		# Preserve the engine's license along with its redistributable executable.
		var license = ProjectSettings.globalize_path("res://../refs/godot-4.7.2/source/LICENSE.txt")
		if not FileAccess.file_exists(license): return "Godot 라이선스 파일을 찾을 수 없습니다."
		error = DirAccess.copy_absolute(license, destination.path_join("GODOT-LICENSE.txt"))
		if error != OK: return "Godot 라이선스 복사 실패"
		var notices = FileAccess.open(destination.path_join("GODOT-THIRD-PARTY-LICENSES.txt"), FileAccess.WRITE)
		if notices == null: return "Godot 구성요소 고지 생성 실패"
		notices.store_string(JSON.stringify({"copyright": Engine.get_copyright_info(), "licenses": Engine.get_license_info()}, "\t"))
		notices.close()
	return ""
