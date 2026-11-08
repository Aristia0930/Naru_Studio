def run(ctx):
    item = ctx.db.get("media", "demo")
    assert item["picture"] == "pic"
    assert item["sound"] == "se"
    assert ctx.db.get("media", "empty")["sound"] is None
    item["picture"] = "changed"
    assert ctx.db.get("media", "demo")["picture"] == "pic"
    return item["sound"]
