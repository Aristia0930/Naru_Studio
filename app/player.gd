extends Control

const Stage = preload("res://ui/stage.gd")
const Runner = preload("res://core/runner.gd")
const Document = preload("res://core/document.gd")
var runner = Runner.new()
var dialogue: Label
var messages: Label

func _ready() -> void:
	var column = VBoxContainer.new()
	column.set_anchors_and_offsets_preset(Control.PRESET_FULL_RECT)
	add_child(column)
	var stage = Stage.new()
	stage.size_flags_vertical = Control.SIZE_EXPAND_FILL
	column.add_child(stage)
	dialogue = Label.new()
	dialogue.custom_minimum_size.y = 70
	dialogue.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
	column.add_child(dialogue)
	var next = Button.new()
	next.text = "다음 / Enter"
	next.pressed.connect(func(): runner.next_dialogue())
	column.add_child(next)
	messages = Label.new()
	column.add_child(messages)
	add_child(runner)
	runner.stage = stage
	if OS.get_cmdline_user_args().has("--autotest"):
		runner.auto_advance = true
		runner.state_changed.connect(func(state):
			if state == "completed": get_tree().quit(0)
			elif state == "failed": get_tree().quit(1))
	runner.dialogue_changed.connect(func(speaker, text): dialogue.text = speaker + "\n" + text)
	runner.logged.connect(func(level, message, location):
		print(JSON.stringify({"level": level, "message": message, "location": location}))
		if level == "error": messages.text = message)
	var parser = JSON.new()
	var file = FileAccess.open("res://game.json", FileAccess.READ)
	if file == null or parser.parse(file.get_as_text()) != OK:
		messages.text = "게임 데이터 파일을 읽을 수 없습니다."
		return
	var errors = Document.validate(parser.data)
	if not errors.is_empty(): messages.text = "\n".join(errors); return
	runner.start(parser.data, parser.data.startScene, ProjectSettings.globalize_path("res://"))

func _unhandled_key_input(event: InputEvent) -> void:
	if event is InputEventKey and event.pressed and event.keycode == KEY_ENTER:
		runner.next_dialogue()

func _exit_tree() -> void:
	runner.stop()
