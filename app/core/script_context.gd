extends RefCounted

var runner: Node
var generation: int

func alive() -> bool:
	return is_instance_valid(runner) and runner.generation == generation and runner.running

func get_value(key: String, fallback: Variant = null) -> Variant:
	return runner.variables.get(key, fallback) if alive() else fallback

func set_value(key: String, value: Variant) -> void:
	if alive():
		runner.variables[key] = value

func log(message: String) -> void:
	if alive():
		runner.record("info", message)

func fail(message: String) -> void:
	if alive():
		runner.fail(message)

func say(speaker: String, text: String) -> void:
	if alive():
		await runner.show_dialogue(speaker, text, generation)

func wait(seconds: float) -> void:
	if alive():
		await runner.wait_seconds(seconds, generation)

func move(target: String, x: float, y: float, seconds: float = 0.4) -> void:
	if alive():
		await runner.move_target(target, Vector2(x, y), seconds, generation)
