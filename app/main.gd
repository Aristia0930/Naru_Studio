extends Control

const Document = preload("res://core/document.gd")
const Runner = preload("res://core/runner.gd")
const Stage = preload("res://ui/stage.gd")
const Exporter = preload("res://core/exporter.gd")

var document = Document.new()
var runner = Runner.new()
var stage = Stage.new()
var scene_id: String = "scene_intro"
var node_id: String = "node_card"
var event_index: int = -1
var character_index: int = -1
var script_index: int = 0
var asset_index: int = -1
var mode: String = "node"
var refreshing: bool = false
var project_title: Label
var status: Label
var scene_list: ItemList
var node_list: ItemList
var event_list: ItemList
var character_list: ItemList
var script_list: ItemList
var asset_list: ItemList
var inspector: VBoxContainer
var tabs: TabContainer
var code_edit: CodeEdit
var dialogue_panel: PanelContainer
var dialogue_text: RichTextLabel
var log_list: ItemList
var log_detail: TextEdit
var file_dialog: FileDialog
var confirm: ConfirmationDialog
var pending: Callable
var file_action: String = ""
var log_path: String
var engine_offset: int = 0
var log_poll: float = 0
var toolbar: HBoxContainer

func _ready() -> void:
	get_tree().auto_accept_quit = false
	get_window().min_size = Vector2i(1100, 730)
	get_window().close_requested.connect(func(): _confirm_action(func(): get_tree().quit()))
	get_window().files_dropped.connect(_files_dropped)
	DirAccess.make_dir_recursive_absolute(ProjectSettings.globalize_path("res://logs"))
	log_path = ProjectSettings.globalize_path("res://logs/session-" + str(Time.get_unix_time_from_system()).replace(".", "-") + ".jsonl")
	_setup_theme()
	document.fresh()
	_build_ui()
	add_child(runner)
	runner.stage = stage
	runner.error_probe = _poll_engine_errors
	runner.logged.connect(_log)
	runner.dialogue_changed.connect(_dialogue)
	runner.command_started.connect(func(index):
		if index < event_list.item_count:
			event_list.select(index)
			event_list.ensure_current_is_visible())
	runner.state_changed.connect(func(value):
		status.text = {"running": "● 테스트 실행 중", "completed": "● 이벤트 완료 · 중지하면 편집으로 복귀", "failed": "● 오류로 중지됨", "stopped": "● 편집 모드"}.get(value, value)
		if not runner.running:
			_restore_stage()
		_refresh_inspector())
	_refresh()
	_log("info", "Studio 0.1 · 장면을 편집하고 F5로 바로 테스트하세요.", {})
	for arg in OS.get_cmdline_user_args():
		if arg.begins_with("--project-file="):
			_open_project(arg.trim_prefix("--project-file="))

func _setup_theme() -> void:
	var palette = Theme.new()
	var font = SystemFont.new()
	font.font_names = PackedStringArray(["Malgun Gothic", "Noto Sans CJK KR", "sans-serif"])
	palette.default_font = font
	palette.default_font_size = 14
	for type in ["Panel", "PanelContainer"]:
		palette.set_stylebox("panel", type, _box("#1c2936", 8, 12))
	for type in ["Button", "OptionButton"]:
		palette.set_stylebox("normal", type, _box("#2a3b4b", 6, 10))
		palette.set_stylebox("hover", type, _box("#3b5368", 6, 10))
		palette.set_stylebox("pressed", type, _box("#247965", 6, 10))
		palette.set_stylebox("disabled", type, _box("#202d39", 6, 10))
	for type in ["LineEdit", "TextEdit", "CodeEdit", "ItemList"]:
		palette.set_stylebox("normal", type, _box("#13212e", 5, 8))
		palette.set_stylebox("focus", type, _box("#1b3040", 5, 8))
		palette.set_color("font_color", type, Color("#e0eaf2"))
	palette.set_color("font_color", "Label", Color("#dbe7ef"))
	theme = palette

func _box(color: String, radius: int, padding: int) -> StyleBoxFlat:
	var style = StyleBoxFlat.new()
	style.bg_color = Color(color)
	style.set_corner_radius_all(radius)
	style.content_margin_left = padding
	style.content_margin_right = padding
	style.content_margin_top = padding
	style.content_margin_bottom = padding
	return style

func _label(text: String, parent: Node, font_size: int = 14) -> Label:
	var label = Label.new()
	label.text = text
	label.add_theme_font_size_override("font_size", font_size)
	parent.add_child(label)
	return label

func _button(text: String, parent: Node, action: Callable) -> Button:
	var button = Button.new()
	button.text = text
	button.pressed.connect(action)
	parent.add_child(button)
	return button

func _list(parent: Node, height: int = 90) -> ItemList:
	var list = ItemList.new()
	list.custom_minimum_size.y = height
	list.size_flags_vertical = Control.SIZE_EXPAND_FILL
	list.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	parent.add_child(list)
	return list

func _build_ui() -> void:
	var background = ColorRect.new()
	background.color = Color("#101a24")
	background.set_anchors_and_offsets_preset(Control.PRESET_FULL_RECT)
	background.mouse_filter = Control.MOUSE_FILTER_IGNORE
	add_child(background)
	var margin = MarginContainer.new()
	margin.set_anchors_and_offsets_preset(Control.PRESET_FULL_RECT)
	for side in ["left", "right", "top", "bottom"]:
		margin.add_theme_constant_override("margin_" + side, 12)
	add_child(margin)
	var root = VBoxContainer.new()
	root.add_theme_constant_override("separation", 10)
	margin.add_child(root)
	toolbar = HBoxContainer.new()
	toolbar.add_theme_constant_override("separation", 6)
	root.add_child(toolbar)
	_label("이면기록  STUDIO", toolbar, 20)
	_label("  0.1  ", toolbar, 12).modulate = Color("#76d8c2")
	_button("새 프로젝트", toolbar, func(): _confirm_action(_new_project))
	_button("열기", toolbar, func(): _confirm_action(func(): _choose_file("open")))
	_button("저장  Ctrl+S", toolbar, _save)
	_button("다른 이름", toolbar, func(): _choose_file("save"))
	_button("↶", toolbar, func(): _history(false))
	_button("↷", toolbar, func(): _history(true))
	_button("▶ 실행  F5", toolbar, func(): _play(0))
	_button("여기부터  F6", toolbar, func(): _play(maxi(0, event_index)))
	_button("■ 중지  F8", toolbar, _stop)
	_button("PC 내보내기", toolbar, func(): _choose_file("export"))
	project_title = _label("", root, 16)
	var body = HSplitContainer.new()
	body.size_flags_vertical = Control.SIZE_EXPAND_FILL
	body.split_offset = 205
	root.add_child(body)
	var left = VBoxContainer.new()
	left.custom_minimum_size.x = 185
	body.add_child(left)
	var scene_header = HBoxContainer.new()
	left.add_child(scene_header)
	_label("장면", scene_header, 16)
	_button("＋", scene_header, _add_scene)
	scene_list = _list(left, 110)
	scene_list.size_flags_stretch_ratio = 0.4
	scene_list.item_selected.connect(func(index):
		if not runner.running:
			_commit_code()
			scene_id = document.data.scenes[index].id
			node_id = ""
			event_index = -1
			_refresh())
	var object_header = HBoxContainer.new()
	left.add_child(object_header)
	_label("장면 오브젝트", object_header, 16)
	_button("＋", object_header, _add_node)
	node_list = _list(left, 140)
	node_list.item_selected.connect(func(index):
		node_id = _scene().nodes[index].id
		mode = "node"
		stage.selected_id = node_id
		stage.queue_redraw()
		_refresh_inspector())
	_button("선택 오브젝트 삭제", left, _delete_node)
	_label("960 × 540 기준 좌표\n이미지·음원 파일을 창에 드롭\nF5 실행 / F8 편집으로 복귀", left, 12)
	var center_right = HSplitContainer.new()
	center_right.split_offset = 890
	body.add_child(center_right)
	var center = VSplitContainer.new()
	center.split_offset = 400
	center.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	center_right.add_child(center)
	var stage_column = VBoxContainer.new()
	center.add_child(stage_column)
	var stage_title = HBoxContainer.new()
	stage_column.add_child(stage_title)
	status = _label("● 편집 모드", stage_title)
	status.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	var grid_toggle = CheckBox.new()
	grid_toggle.text = "격자"
	grid_toggle.button_pressed = true
	grid_toggle.toggled.connect(func(value): stage.grid = value; stage.queue_redraw())
	stage_title.add_child(grid_toggle)
	stage.size_flags_vertical = Control.SIZE_EXPAND_FILL
	stage_column.add_child(stage)
	stage.selected.connect(func(id): node_id = id; mode = "node"; _refresh_lists(); _refresh_inspector())
	stage.move_started.connect(func(): document.checkpoint())
	stage.moved.connect(func(id, point):
		var item = Document.find(_scene().nodes, id)
		if not item.is_empty():
			item.x = point.x
			item.y = point.y
			_update_title())
	dialogue_panel = PanelContainer.new()
	stage_column.add_child(dialogue_panel)
	var dialogue_row = HBoxContainer.new()
	dialogue_panel.add_child(dialogue_row)
	dialogue_text = RichTextLabel.new()
	dialogue_text.custom_minimum_size.y = 68
	dialogue_text.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	dialogue_text.fit_content = true
	dialogue_row.add_child(dialogue_text)
	_button("다음  Enter", dialogue_row, func(): runner.next_dialogue())
	dialogue_panel.hide()
	tabs = TabContainer.new()
	tabs.custom_minimum_size.y = 210
	center.add_child(tabs)
	_build_event_tab()
	_build_data_tab()
	_build_code_tab()
	_build_asset_tab()
	var inspector_scroll = ScrollContainer.new()
	inspector_scroll.custom_minimum_size.x = 265
	center_right.add_child(inspector_scroll)
	inspector = VBoxContainer.new()
	inspector.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	inspector.add_theme_constant_override("separation", 9)
	inspector_scroll.add_child(inspector)
	var log_header = HBoxContainer.new()
	root.add_child(log_header)
	_label("실행 기록 · 오류 로그", log_header, 15)
	_button("로그 폴더 열기", log_header, func(): OS.shell_open(ProjectSettings.globalize_path("res://logs")))
	var log_split = HSplitContainer.new()
	log_split.custom_minimum_size.y = 115
	root.add_child(log_split)
	log_list = _list(log_split, 105)
	log_detail = TextEdit.new()
	log_detail.editable = false
	log_detail.custom_minimum_size = Vector2(370, 105)
	log_split.add_child(log_detail)
	log_list.item_selected.connect(_select_log)
	file_dialog = FileDialog.new()
	file_dialog.access = FileDialog.ACCESS_FILESYSTEM
	file_dialog.use_native_dialog = true
	file_dialog.file_selected.connect(_file_selected)
	file_dialog.dir_selected.connect(_export_selected)
	add_child(file_dialog)
	confirm = ConfirmationDialog.new()
	confirm.title = "저장하지 않은 변경"
	confirm.dialog_text = "저장하지 않은 변경이 있습니다. 변경을 버리고 계속할까요?"
	confirm.confirmed.connect(func(): if pending.is_valid(): pending.call())
	add_child(confirm)

func _build_event_tab() -> void:
	var column = VBoxContainer.new()
	column.name = "이벤트"
	tabs.add_child(column)
	var row = HBoxContainer.new()
	column.add_child(row)
	var picker = OptionButton.new()
	for key in Document.COMMANDS:
		picker.add_item(Document.COMMANDS[key])
		picker.set_item_metadata(picker.item_count - 1, key)
	row.add_child(picker)
	_button("명령 추가", row, func(): _add_event(picker.get_item_metadata(picker.selected)))
	_button("↑", row, func(): _shift_event(-1))
	_button("↓", row, func(): _shift_event(1))
	_button("삭제", row, _delete_event)
	event_list = _list(column)
	event_list.item_selected.connect(func(index): event_index = index; mode = "event"; _refresh_inspector())

func _build_data_tab() -> void:
	var column = VBoxContainer.new()
	column.name = "데이터"
	tabs.add_child(column)
	var row = HBoxContainer.new()
	column.add_child(row)
	_label("캐릭터 정의", row)
	_button("캐릭터 추가", row, func():
		if not _editable(): return
		_commit_code()
		document.checkpoint()
		document.data.characters.append({"id": Document.uid("character"), "name": "새 캐릭터", "hp": 100, "description": ""})
		character_index = document.data.characters.size() - 1
		mode = "character"
		_refresh())
	character_list = _list(column)
	character_list.item_selected.connect(func(index): character_index = index; mode = "character"; _refresh_inspector())

func _build_code_tab() -> void:
	var column = VBoxContainer.new()
	column.name = "코드"
	tabs.add_child(column)
	var row = HBoxContainer.new()
	column.add_child(row)
	_button("새 코드", row, _add_script)
	_button(".gd 가져오기", row, func(): _choose_file("script"))
	_button("적용·문법 검사", row, _check_code)
	_label("run(ctx) → 이벤트의 '코드 이벤트 실행'으로 연결", row, 12)
	var split = HSplitContainer.new()
	split.size_flags_vertical = Control.SIZE_EXPAND_FILL
	column.add_child(split)
	script_list = _list(split)
	script_list.custom_minimum_size.x = 155
	script_list.item_selected.connect(func(index):
		_commit_code()
		script_index = index
		code_edit.text = document.data.scripts[index].source
		mode = "script"
		_refresh_inspector())
	code_edit = CodeEdit.new()
	code_edit.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	code_edit.gutters_draw_line_numbers = true
	code_edit.indent_automatic = true
	code_edit.auto_brace_completion_enabled = true
	var highlighter = CodeHighlighter.new()
	for word in ["extends", "func", "var", "if", "else", "return", "await", "for", "in", "true", "false", "null"]:
		highlighter.add_keyword_color(word, Color("#7edcc3"))
	highlighter.add_color_region('"', '"', Color("#e8c487"))
	highlighter.add_color_region("#", "", Color("#7c91a5"), true)
	code_edit.syntax_highlighter = highlighter
	code_edit.text_changed.connect(_update_title)
	split.add_child(code_edit)

func _build_asset_tab() -> void:
	var column = VBoxContainer.new()
	column.name = "에셋"
	tabs.add_child(column)
	var row = HBoxContainer.new()
	column.add_child(row)
	_button("이미지·음원 가져오기", row, func(): _choose_file("asset"))
	_button("선택 이미지 배치", row, _place_asset)
	asset_list = _list(column)
	asset_list.item_selected.connect(func(index): asset_index = index; mode = "asset"; _refresh_inspector())

func _scene() -> Dictionary:
	return Document.find(document.data.scenes, scene_id)

func _editable() -> bool:
	if runner.running:
		_log("warning", "테스트를 중지한 뒤 편집하세요. 실행 중 상태는 원본과 분리됩니다.", {})
		return false
	return true

func _refresh() -> void:
	if _scene().is_empty():
		scene_id = document.data.scenes[0].id
	_refresh_lists()
	if script_index >= document.data.scripts.size():
		script_index = document.data.scripts.size() - 1
	code_edit.text = document.data.scripts[script_index].source if script_index >= 0 else ""
	_restore_stage()
	_refresh_inspector()
	_update_title()

func _refresh_lists() -> void:
	var entries = [[scene_list, document.data.scenes, scene_id], [node_list, _scene().get("nodes", []), node_id], [character_list, document.data.characters, ""], [script_list, document.data.scripts, ""], [asset_list, document.data.assets, ""]]
	for entry in entries:
		entry[0].clear()
		for item in entry[1]:
			entry[0].add_item(str(item.get("name", item.id)))
			if item.id == entry[2]: entry[0].select(entry[0].item_count - 1)
	event_list.clear()
	for event in _scene().get("events", []):
		var summary = str(event.get("text", event.get("target", event.get("script", event.get("asset", "")))))
		event_list.add_item("%02d  %s   %s" % [event_list.item_count + 1, Document.COMMANDS.get(event.type, event.type), summary.left(46)])
	if event_index >= 0 and event_index < event_list.item_count: event_list.select(event_index)
	if character_index >= 0 and character_index < character_list.item_count: character_list.select(character_index)
	if script_index >= 0 and script_index < script_list.item_count: script_list.select(script_index)
	if asset_index >= 0 and asset_index < asset_list.item_count: asset_list.select(asset_index)
	_update_title()

func _restore_stage() -> void:
	if not runner.running:
		stage.configure(_scene(), document.data, document.project_path.get_base_dir())
		stage.interactive = true
		stage.grid = true
		stage.selected_id = node_id

func _update_title() -> void:
	if project_title != null:
		var modified = document.dirty() or (code_edit != null and script_index >= 0 and script_index < document.data.scripts.size() and code_edit.text != document.data.scripts[script_index].source)
		project_title.text = ("● " if modified else "") + str(document.data.get("name", "프로젝트")) + "  /  " + str(_scene().get("name", "")) + "     " + (document.project_path if not document.project_path.is_empty() else "아직 저장하지 않은 프로젝트")
		get_window().title = "이면기록 Studio 0.1 — " + str(document.data.get("name", "")) + (" *" if modified else "")

func _change(item: Dictionary, key: String, value: Variant) -> void:
	if refreshing or item.get(key) == value or not _editable(): return
	document.checkpoint()
	item[key] = value
	stage.queue_redraw()
	_refresh_lists()

func _text_field(title: String, item: Dictionary, key: String, multiline: bool = false) -> void:
	_label(title, inspector, 12)
	if multiline:
		var field = TextEdit.new()
		field.text = str(item.get(key, ""))
		field.custom_minimum_size.y = 90
		field.editable = not runner.running
		field.focus_exited.connect(func(): _change(item, key, field.text))
		inspector.add_child(field)
	else:
		var field = LineEdit.new()
		field.text = str(item.get(key, ""))
		field.editable = not runner.running
		field.text_submitted.connect(func(value): _change(item, key, value))
		field.focus_exited.connect(func(): _change(item, key, field.text))
		inspector.add_child(field)

func _number_field(title: String, item: Dictionary, key: String, minimum: float = -10000, maximum: float = 10000) -> void:
	_label(title, inspector, 12)
	var field = SpinBox.new()
	field.min_value = minimum
	field.max_value = maximum
	field.step = 0.1
	field.value = float(item.get(key, 0))
	field.editable = not runner.running
	field.value_changed.connect(func(value): _change(item, key, value))
	inspector.add_child(field)

func _toggle_field(title: String, item: Dictionary, key: String) -> void:
	var field = CheckBox.new()
	field.text = title
	field.button_pressed = bool(item.get(key, false))
	field.disabled = runner.running
	field.toggled.connect(func(value): _change(item, key, value))
	inspector.add_child(field)

func _reference_field(title: String, item: Dictionary, key: String, options: Array, allow_empty: bool = false) -> void:
	_label(title, inspector, 12)
	var field = OptionButton.new()
	field.disabled = runner.running
	if allow_empty:
		field.add_item("없음")
		field.set_item_metadata(0, "")
	for option in options:
		field.add_item(str(option.get("name", option.id)))
		field.set_item_metadata(field.item_count - 1, option.id)
	for index in range(field.item_count):
		if field.get_item_metadata(index) == item.get(key, ""): field.select(index)
	field.item_selected.connect(func(index): _change(item, key, field.get_item_metadata(index)))
	inspector.add_child(field)

func _refresh_inspector() -> void:
	if inspector == null: return
	refreshing = true
	for child in inspector.get_children():
		inspector.remove_child(child)
		child.queue_free()
	_label("속성", inspector, 19)
	if runner.running: _label("테스트 중 · 편집 원본 보호", inspector, 12)
	var item: Dictionary = {}
	match mode:
		"node": item = Document.find(_scene().get("nodes", []), node_id)
		"event":
			if event_index >= 0 and event_index < _scene().events.size(): item = _scene().events[event_index]
		"character":
			if character_index >= 0 and character_index < document.data.characters.size(): item = document.data.characters[character_index]
		"script":
			if script_index >= 0: item = document.data.scripts[script_index]
		"asset":
			if asset_index >= 0 and asset_index < document.data.assets.size(): item = document.data.assets[asset_index]
	if item.is_empty():
		_text_field("프로젝트 이름", document.data, "name")
		_text_field("장면 이름", _scene(), "name")
		_text_field("배경색 (#RRGGBB)", _scene(), "background")
	else:
		_label(str(item.id), inspector, 11).modulate = Color("#829caf")
		if mode != "event": _text_field("이름", item, "name")
		match mode:
			"node":
				for key in ["x", "y", "width", "height"]: _number_field(key, item, key, 1 if key in ["width", "height"] else -10000)
				_text_field("색상 (#RRGGBB)", item, "color")
				_toggle_field("표시", item, "visible")
				_reference_field("이미지", item, "asset", document.data.assets.filter(func(asset): return asset.kind == "image"), true)
			"character":
				_number_field("기본 체력", item, "hp", 1, 999999)
				_text_field("설명", item, "description", true)
			"asset": _label(str(item.path), inspector, 11)
			"script":
				_label("코드 탭에서 수정 → 적용·검사\n이벤트 목록에서 코드 실행 연결", inspector, 12)
			"event": _event_fields(item)
	code_edit.editable = not runner.running
	refreshing = false

func _event_fields(item: Dictionary) -> void:
	_label(Document.COMMANDS.get(item.type, item.type), inspector, 16)
	match item.type:
		"dialogue.say":
			_text_field("화자", item, "speaker")
			_text_field("대사", item, "text", true)
		"wait": _number_field("대기 시간 (초)", item, "duration", 0, 3600)
		"node.move", "node.show":
			_reference_field("대상", item, "target", _scene().nodes)
			if item.type == "node.move":
				_number_field("도착 X", item, "x")
				_number_field("도착 Y", item, "y")
				_number_field("시간 (초)", item, "duration", 0.01, 3600)
			else: _toggle_field("표시", item, "visible")
		"audio.play", "audio.stop":
			_text_field("채널 (bgm / sfx 등)", item, "channel")
			if item.type == "audio.play":
				_reference_field("음원", item, "asset", document.data.assets.filter(func(asset): return asset.kind == "audio"))
				_number_field("음량 (dB)", item, "volume", -60, 6)
				_toggle_field("반복 재생", item, "loop")
		"variable.set":
			_text_field("변수 이름", item, "key")
			_number_field("값", item, "value")
		"script.call": _reference_field("실행할 코드", item, "script", document.data.scripts)

func _add_scene() -> void:
	if not _editable(): return
	_commit_code()
	document.checkpoint()
	scene_id = Document.uid("scene")
	document.data.scenes.append({"id": scene_id, "name": "새 장면", "background": "#192c3b", "nodes": [], "events": []})
	node_id = ""
	event_index = -1
	_refresh()

func _add_node() -> void:
	if not _editable(): return
	_commit_code()
	document.checkpoint()
	node_id = Document.uid("node")
	_scene().nodes.append({"id": node_id, "name": "새 오브젝트", "asset": "", "x": 100.0, "y": 100.0, "width": 160.0, "height": 180.0, "color": "#54d6bd", "visible": true})
	mode = "node"
	_refresh()

func _delete_node() -> void:
	if not _editable() or node_id.is_empty(): return
	_commit_code()
	for event in _scene().events:
		if event.get("target") == node_id:
			_log("warning", "이 오브젝트를 사용하는 명령이 있습니다. 명령 대상을 먼저 바꾸세요.", {"event": event.id})
			return
	document.checkpoint()
	_scene().nodes = _scene().nodes.filter(func(item): return item.id != node_id)
	node_id = ""
	_refresh()

func _add_event(kind: String) -> void:
	if not _editable(): return
	_commit_code()
	var event = {"id": Document.uid("event"), "type": kind}
	match kind:
		"dialogue.say": event.merge({"speaker": "화자", "text": "대사를 입력하세요."})
		"wait": event.duration = 0.5
		"node.move", "node.show":
			if _scene().nodes.is_empty(): _log("warning", "장면에 오브젝트를 먼저 추가하세요.", {}); return
			event.merge({"target": _scene().nodes[0].id, "x": 500.0, "y": 180.0, "duration": 0.5, "visible": true})
		"audio.play":
			var audio = document.data.assets.filter(func(asset): return asset.kind == "audio")
			if audio.is_empty(): _log("warning", "에셋 탭에서 음원을 먼저 가져오세요.", {}); return
			event.merge({"asset": audio[0].id, "channel": "bgm", "volume": -12.0, "loop": false})
		"audio.stop": event.channel = "bgm"
		"variable.set": event.merge({"key": "value", "value": 1})
		"script.call":
			if document.data.scripts.is_empty(): _log("warning", "코드 탭에서 코드를 먼저 추가하세요.", {}); return
			event.script = document.data.scripts[maxi(0, script_index)].id
	document.checkpoint()
	_scene().events.append(event)
	event_index = _scene().events.size() - 1
	mode = "event"
	_refresh()

func _shift_event(direction: int) -> void:
	if not _editable(): return
	_commit_code()
	var destination = event_index + direction
	if event_index < 0 or destination < 0 or destination >= _scene().events.size(): return
	document.checkpoint()
	var item = _scene().events.pop_at(event_index)
	_scene().events.insert(destination, item)
	event_index = destination
	_refresh()

func _delete_event() -> void:
	if not _editable() or event_index < 0 or event_index >= _scene().events.size(): return
	_commit_code()
	document.checkpoint()
	_scene().events.remove_at(event_index)
	event_index = mini(event_index, _scene().events.size() - 1)
	_refresh()

func _commit_code() -> void:
	if code_edit == null or runner.running or script_index < 0 or script_index >= document.data.scripts.size(): return
	var item = document.data.scripts[script_index]
	if item.source != code_edit.text:
		document.checkpoint()
		item.source = code_edit.text
		_update_title()

func _check_code() -> void:
	_commit_code()
	if script_index < 0: return
	var result = Runner.compile_source(code_edit.text)
	_log("info" if result.error.is_empty() else "error", "문법 검사 통과 · func run(ctx) 연결 가능" if result.error.is_empty() else result.error, {"script": document.data.scripts[script_index].id})

func _add_script() -> void:
	if not _editable(): return
	_commit_code()
	document.checkpoint()
	document.data.scripts.append({"id": Document.uid("script"), "name": "새 코드 이벤트", "source": Document.SCRIPT_TEMPLATE})
	script_index = document.data.scripts.size() - 1
	mode = "script"
	_refresh()

func _place_asset() -> void:
	if not _editable() or asset_index < 0 or asset_index >= document.data.assets.size(): return
	var asset = document.data.assets[asset_index]
	if asset.kind != "image": _log("warning", "배치할 이미지 에셋을 선택하세요.", {}); return
	_add_node()
	var item = Document.find(_scene().nodes, node_id)
	item.asset = asset.id
	item.name = asset.name
	_refresh()

func _choose_file(action: String) -> void:
	if not _editable(): return
	_commit_code()
	file_action = action
	file_dialog.filters = PackedStringArray()
	match action:
		"open": file_dialog.file_mode = FileDialog.FILE_MODE_OPEN_FILE; file_dialog.add_filter("*.json", "Studio 프로젝트")
		"save": file_dialog.file_mode = FileDialog.FILE_MODE_SAVE_FILE; file_dialog.add_filter("*.json", "Studio 프로젝트"); file_dialog.current_file = "project.json"
		"asset": file_dialog.file_mode = FileDialog.FILE_MODE_OPEN_FILE; file_dialog.add_filter("*.png,*.jpg,*.jpeg,*.webp,*.mp3,*.wav,*.ogg", "이미지·음원")
		"script": file_dialog.file_mode = FileDialog.FILE_MODE_OPEN_FILE; file_dialog.add_filter("*.gd", "GDScript 이벤트")
		"export": file_dialog.file_mode = FileDialog.FILE_MODE_OPEN_DIR
	file_dialog.title = {"open": "프로젝트 열기", "save": "프로젝트 저장", "asset": "에셋 가져오기", "script": "코드 파일 가져오기", "export": "PC 실행본을 저장할 상위 폴더"}.get(action, "파일 선택")
	file_dialog.popup_centered_ratio(0.7)

func _file_selected(path: String) -> void:
	match file_action:
		"open": _open_project(path)
		"save":
			var error = document.save_project(path)
			_log("info" if error.is_empty() else "error", "저장 완료 · " + path if error.is_empty() else error, {})
			_refresh()
		"asset": _import_asset(path)
		"script":
			var file = FileAccess.open(path, FileAccess.READ)
			if file == null: _log("error", "코드 파일을 열 수 없습니다.", {}); return
			document.checkpoint()
			document.data.scripts.append({"id": Document.uid("script"), "name": path.get_file(), "source": file.get_as_text()})
			script_index = document.data.scripts.size() - 1
			mode = "script"
			tabs.current_tab = 2
			_refresh()
			_check_code()

func _import_asset(path: String) -> void:
	_commit_code()
	var error = document.import_asset(path)
	_log("info" if error.is_empty() else "error", "에셋 가져오기 · " + path.get_file() if error.is_empty() else error, {})
	_refresh()

func _files_dropped(paths: PackedStringArray) -> void:
	if not _editable(): return
	_commit_code()
	for path in paths: _import_asset(path)
	tabs.current_tab = 3

func _save() -> void:
	if not _editable(): return
	get_viewport().gui_release_focus()
	_commit_code()
	if document.project_path.is_empty(): _choose_file("save"); return
	var error = document.save_project(document.project_path)
	_log("info" if error.is_empty() else "error", "저장 완료" if error.is_empty() else error, {})
	_update_title()

func _open_project(path: String) -> void:
	var error = document.load_project(path)
	if not error.is_empty(): _log("error", error, {}); return
	scene_id = str(document.data.startScene)
	node_id = ""
	event_index = -1
	script_index = 0 if not document.data.scripts.is_empty() else -1
	_refresh()
	_log("info", "프로젝트 열기 · " + path, {})

func _new_project() -> void:
	_stop()
	document.fresh()
	scene_id = "scene_intro"
	node_id = "node_card"
	event_index = -1
	script_index = 0
	_refresh()

func _confirm_action(action: Callable) -> void:
	_commit_code()
	if document.dirty(): pending = action; confirm.popup_centered()
	else: action.call()

func _history(redo: bool) -> void:
	if not _editable(): return
	_commit_code()
	if redo: document.redo()
	else: document.undo()
	_refresh()

func _play(index: int) -> void:
	if runner.running: _stop()
	get_viewport().gui_release_focus()
	_commit_code()
	runner.start(document.data, scene_id, document.project_path.get_base_dir(), index)
	_refresh_inspector()

func _stop() -> void:
	runner.stop()
	_restore_stage()
	_refresh_inspector()

func _dialogue(speaker: String, text: String) -> void:
	dialogue_panel.visible = not text.is_empty()
	dialogue_text.text = speaker + "\n" + text

func _log(level: String, message: String, location: Dictionary) -> void:
	var entry = {"time": Time.get_datetime_string_from_system(), "level": level, "message": message, "location": location}
	if log_list != null:
		if log_list.item_count >= 2000: log_list.remove_item(0)
		log_list.add_item("%s  %-7s  %s" % [entry.time.substr(11), level.to_upper(), message.replace("\n", " ")])
		var index = log_list.item_count - 1
		log_list.set_item_metadata(index, entry)
		log_list.set_item_custom_fg_color(index, Color("#ff8e9e") if level == "error" else Color("#efcb87") if level == "warning" else Color("#a9c8d9"))
		log_list.ensure_current_is_visible()
	var file = FileAccess.open(log_path, FileAccess.READ_WRITE if FileAccess.file_exists(log_path) else FileAccess.WRITE)
	if file != null:
		file.seek_end()
		file.store_line(JSON.stringify(entry))

func _select_log(index: int) -> void:
	var entry = log_list.get_item_metadata(index)
	log_detail.text = JSON.stringify(entry, "  ")
	var location = entry.location
	if runner.running: return
	_commit_code()
	if location.has("scene") and not Document.find(document.data.scenes, str(location.scene)).is_empty(): scene_id = location.scene
	if location.has("event"):
		for event in _scene().events:
			if event.id == location.event: event_index = _scene().events.find(event); mode = "event"; tabs.current_tab = 0
	if location.has("script"):
		var item = Document.find(document.data.scripts, location.script)
		if not item.is_empty(): _commit_code(); script_index = document.data.scripts.find(item); mode = "script"; tabs.current_tab = 2
	_refresh()

func _export_selected(folder: String) -> void:
	_commit_code()
	var target = folder.path_join("StudioGame_" + str(Time.get_unix_time_from_system()).replace(".", "_"))
	var result = Exporter.export_game(document.data, document.project_path.get_base_dir(), target, true)
	_log("info" if result.is_empty() else "error", "PC 실행본 생성 · " + target + "/Start Game.cmd" if result.is_empty() else result, {})

func _process(delta: float) -> void:
	log_poll += delta
	if log_poll < 0.4: return
	log_poll = 0
	_poll_engine_errors()

func _poll_engine_errors() -> void:
	var file = FileAccess.open("res://logs/engine.log", FileAccess.READ)
	if file == null: return
	if file.get_length() < engine_offset: engine_offset = 0
	file.seek(engine_offset)
	var error_found = false
	var detail = ""
	while file.get_position() < file.get_length():
		var line = file.get_line()
		if line.contains("SCRIPT ERROR:") or line.begins_with("ERROR:"):
			error_found = true
			detail += line + "\n"
		elif error_found and not line.is_empty(): detail += line + "\n"
	engine_offset = file.get_position()
	if error_found:
		if runner.running: runner.fail(detail)
		else: _log("error", detail, {"source": "Godot", "file": "logs/engine.log"})

func _unhandled_key_input(event: InputEvent) -> void:
	if not event is InputEventKey or not event.pressed or event.echo: return
	match event.keycode:
		KEY_F5: _play(0)
		KEY_F6: _play(maxi(0, event_index))
		KEY_F8: _stop()
		KEY_ENTER:
			if runner.dialogue_pending: runner.next_dialogue()
		KEY_S:
			if event.ctrl_pressed: _save()
		KEY_Z:
			if event.ctrl_pressed: _history(event.shift_pressed)

func _input(event: InputEvent) -> void:
	if not event is InputEventKey or not event.pressed or event.echo: return
	if event.keycode in [KEY_F5, KEY_F6, KEY_F8] or (event.ctrl_pressed and event.keycode == KEY_S):
		_unhandled_key_input(event)
		get_viewport().set_input_as_handled()
