extends SceneTree

var checks: Dictionary = {}
var signal_value: int = 0

func _initialize() -> void:
	call_deferred("_run")

func _receive_completed(value: int) -> void:
	signal_value = value

func _run() -> void:
	var scene := Node2D.new()
	scene.name = "ResearchScene"
	var owned_child := Node2D.new()
	owned_child.name = "SavedChild"
	scene.add_child(owned_child)
	owned_child.owner = scene
	var unowned_child := Node2D.new()
	unowned_child.name = "TransientChild"
	scene.add_child(unowned_child)
	var packed := PackedScene.new()
	checks["pack_scene"] = packed.pack(scene) == OK
	checks["save_scene_text"] = ResourceSaver.save(packed, "res://generated_scene.tscn") == OK
	var reloaded := load("res://generated_scene.tscn") as PackedScene
	var first := reloaded.instantiate()
	var second := reloaded.instantiate()
	checks["scene_ownership"] = first.has_node("SavedChild") and not first.has_node("TransientChild")
	(first.get_node("SavedChild") as Node2D).position.x = 42.0
	checks["independent_instances"] = (second.get_node("SavedChild") as Node2D).position.x == 0.0
	first.free()
	second.free()
	scene.free()

	var emitter := Node.new()
	emitter.add_user_signal("completed", [{"name": "value", "type": TYPE_INT}])
	emitter.connect("completed", _receive_completed)
	emitter.emit_signal("completed", 17)
	checks["signal_delivery"] = signal_value == 17
	emitter.disconnect("completed", _receive_completed)
	emitter.emit_signal("completed", 99)
	checks["signal_disconnect"] = signal_value == 17
	emitter.free()

	var record := Resource.new()
	record.resource_name = "CharacterRecord"
	record.set_meta("record_id", "character.research")
	checks["save_resource_text"] = ResourceSaver.save(record, "res://generated_record.tres") == OK
	var loaded_record := load("res://generated_record.tres")
	checks["resource_round_trip"] = loaded_record.resource_name == "CharacterRecord" and loaded_record.get_meta("record_id") == "character.research"

	var world := Node2D.new()
	root.add_child(world)
	var wall := StaticBody2D.new()
	wall.position = Vector2(80, 0)
	var wall_shape := CollisionShape2D.new()
	var wall_rect := RectangleShape2D.new()
	wall_rect.size = Vector2(10, 200)
	wall_shape.shape = wall_rect
	wall.add_child(wall_shape)
	world.add_child(wall)
	var player := CharacterBody2D.new()
	player.motion_mode = CharacterBody2D.MOTION_MODE_FLOATING
	var player_shape := CollisionShape2D.new()
	var player_rect := RectangleShape2D.new()
	player_rect.size = Vector2(10, 10)
	player_shape.shape = player_rect
	player.add_child(player_shape)
	var camera := Camera2D.new()
	player.add_child(camera)
	world.add_child(player)
	await physics_frame
	var collision_seen := false
	for index in range(90):
		await physics_frame
		player.velocity = Vector2(120, 0)
		collision_seen = player.move_and_slide() or collision_seen
	checks["horizontal_motion_and_wall"] = collision_seen and player.position.x > 0 and player.position.x <= 70.1
	checks["camera_attached"] = camera.get_parent() == player and camera.enabled
	var result := {"engine_version": Engine.get_version_info(), "checks": checks, "wall_stop_x": player.position.x, "physics_ticks": 90, "scope": "Headless structural and runtime probe; no visual UI, Android export, jump or combat test"}
	var output := FileAccess.open("res://probe-result.json", FileAccess.WRITE)
	output.store_string(JSON.stringify(result, "  "))
	output.close()
	print("FOUNDATION_PROBE_RESULT=" + JSON.stringify(result))
	world.free()
	quit(0 if checks.values().all(func(value): return value == true) else 1)
