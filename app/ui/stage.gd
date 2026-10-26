extends Control

signal selected(id: String)
signal move_started
signal moved(id: String, point: Vector2)

var scene_data: Dictionary = {}
var project_data: Dictionary = {}
var asset_root: String = ""
var selected_id: String = ""
var interactive: bool = true
var grid: bool = true
var textures: Dictionary = {}
var dragging: bool = false
var drag_offset: Vector2
const MODEL = Vector2(960, 540)

func _ready() -> void:
	clip_contents = true
	custom_minimum_size = Vector2(480, 300)
	mouse_default_cursor_shape = Control.CURSOR_CROSS
	resized.connect(queue_redraw)

func scale_factor() -> float:
	return maxf(0.01, minf(size.x / MODEL.x, size.y / MODEL.y))

func origin() -> Vector2:
	return (size - MODEL * scale_factor()) / 2.0

func model_point(point: Vector2) -> Vector2:
	return (point - origin()) / scale_factor()

func configure(scene: Dictionary, project: Dictionary, folder: String) -> void:
	scene_data = scene
	project_data = project
	asset_root = folder
	textures.clear()
	queue_redraw()

func _draw() -> void:
	draw_rect(Rect2(Vector2.ZERO, size), Color("#101820"))
	if scene_data.is_empty():
		return
	var factor = scale_factor()
	var offset = origin()
	var bounds = Rect2(offset, MODEL * factor)
	draw_rect(bounds, Color(str(scene_data.get("background", "#192c3b"))))
	if grid:
		for x in range(0, 961, 40):
			draw_line(offset + Vector2(x, 0) * factor, offset + Vector2(x, 540) * factor, Color(1, 1, 1, 0.045))
		for y in range(0, 541, 40):
			draw_line(offset + Vector2(0, y) * factor, offset + Vector2(960, y) * factor, Color(1, 1, 1, 0.045))
	var font = get_theme_default_font()
	for item in scene_data.get("nodes", []):
		if not item.get("visible", true):
			continue
		var rectangle = Rect2(offset + Vector2(item.x, item.y) * factor, Vector2(item.width, item.height) * factor)
		var texture = texture_for(str(item.get("asset", "")))
		if texture != null:
			draw_texture_rect(texture, rectangle, false)
		else:
			draw_style_box(_box(Color(str(item.get("color", "#54d6bd")))), rectangle)
			draw_string(font, rectangle.position + Vector2(12, 28), str(item.get("name", "오브젝트")), HORIZONTAL_ALIGNMENT_LEFT, maxf(0, rectangle.size.x - 16), 15, Color("#152c32"))
		if interactive and item.id == selected_id:
			draw_rect(rectangle.grow(2), Color("#a9ffe5"), false, 2)
			for corner in [rectangle.position, rectangle.position + Vector2(rectangle.size.x, 0), rectangle.end, rectangle.position + Vector2(0, rectangle.size.y)]:
				draw_rect(Rect2(corner - Vector2(3, 3), Vector2(6, 6)), Color("#e3fff7"))
	draw_rect(bounds, Color("#364a5b"), false, 1)

func _box(color: Color) -> StyleBoxFlat:
	var box = StyleBoxFlat.new()
	box.bg_color = color
	box.set_corner_radius_all(10)
	return box

func texture_for(id: String) -> Texture2D:
	if id.is_empty():
		return null
	if textures.has(id):
		return textures[id]
	for asset in project_data.get("assets", []):
		if asset.id == id and asset.get("kind") == "image":
			var image = Image.load_from_file(asset_root.path_join(asset.path))
			if image != null and not image.is_empty():
				textures[id] = ImageTexture.create_from_image(image)
				return textures[id]
	return null

func _gui_input(event: InputEvent) -> void:
	if not interactive or scene_data.is_empty():
		return
	if event is InputEventMouseButton and event.button_index == MOUSE_BUTTON_LEFT:
		if not event.pressed:
			dragging = false
			return
		var point = model_point(event.position)
		var items = scene_data.nodes.duplicate()
		items.reverse()
		for item in items:
			if item.get("visible", true) and Rect2(Vector2(item.x, item.y), Vector2(item.width, item.height)).has_point(point):
				selected_id = item.id
				drag_offset = point - Vector2(item.x, item.y)
				dragging = true
				selected.emit(selected_id)
				move_started.emit()
				queue_redraw()
				accept_event()
				return
		selected_id = ""
		selected.emit("")
		queue_redraw()
	if event is InputEventMouseMotion and dragging:
		var point = model_point(event.position) - drag_offset
		point = point.snapped(Vector2(1, 1))
		moved.emit(selected_id, point)
		queue_redraw()
