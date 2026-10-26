extends SceneTree

func _initialize() -> void:
	call_deferred("_run")

func _run() -> void:
	var studio = load("res://main.tscn").instantiate()
	root.add_child(studio)
	for index in range(8): await process_frame
	await RenderingServer.frame_post_draw
	var output = ProjectSettings.globalize_path("res://test-output/studio-overview.png")
	DirAccess.make_dir_recursive_absolute(output.get_base_dir())
	var result = root.get_texture().get_image().save_png(output)
	var record = {
		"screenshot": output, "save_error": result, "window_size": [root.size.x, root.size.y],
		"stage": [studio.stage.size.x, studio.stage.size.y],
		"log_bottom": studio.log_list.global_position.y + studio.log_list.size.y,
		"window_bottom": root.size.y
	}
	var file = FileAccess.open("res://test-output/visual-smoke.json", FileAccess.WRITE)
	file.store_string(JSON.stringify(record, "  "))
	file.close()
	print(JSON.stringify(record))
	studio.queue_free()
	await process_frame
	quit(0 if result == OK else 1)
