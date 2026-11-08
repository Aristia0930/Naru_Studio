def run(ctx):
    assert ctx.scene is None


def change(ctx):
    obj = ctx.scene.object("npc")
    assert obj.show_name is False and obj.name_text == ""
    assert obj.name_position == "above"
    assert obj.name_background_color == "#0c1018"
    assert obj.name_background_transparent is False
    obj.name_position = "below"
    obj.name_background_color = "#2468ac"
    obj.name_background_transparent = True
    assert obj.name_position == "below" and obj.name_background_color == "#2468ac"
    assert obj.name_background_transparent is True
    for key, invalid in (("name_position", "left"), ("name_position", ""),
                         ("name_background_color", "red"), ("name_background_color", "#ff"),
                         ("name_background_color", "#00gg00"), ("name_position", "below\0")):
        try:
            setattr(obj, key, invalid)
        except ValueError:
            pass
        else:
            raise AssertionError("invalid style accepted")
    for key, invalid in (("name_position", False), ("name_background_color", 1),
                         ("name_background_transparent", 1), ("name_background_transparent", "yes")):
        try:
            setattr(obj, key, invalid)
        except TypeError:
            pass
        else:
            raise AssertionError("invalid style type accepted")
    assert obj.name_position == "below" and obj.name_background_color == "#2468ac"
    obj.show_name = True
    obj.name_text = "여관 주인"
    assert obj.show_name is True and obj.name_text == "여관 주인"
    for invalid in (1, "yes", None):
        try:
            obj.show_name = invalid
        except TypeError:
            pass
        else:
            raise AssertionError("show_name type accepted")
    for invalid in (12, False, None):
        try:
            obj.name_text = invalid
        except TypeError:
            pass
        else:
            raise AssertionError("name_text type accepted")
    for invalid in ("\n", "\t", "\0", "x" * 513):
        try:
            obj.name_text = invalid
        except ValueError:
            pass
        else:
            raise AssertionError("invalid text accepted")
    assert obj.show_name is True and obj.name_text == "여관 주인"
    obj.name_text = ""
    assert obj.name_text == ""
    try:
        obj.name_text = "\ud800"
    except UnicodeError:
        pass
    else:
        raise AssertionError("invalid Unicode accepted")
    assert obj.name_text == ""
    return True
