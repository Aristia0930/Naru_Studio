"""새 폴더에 API 이벤트 예제를 생성한다. 기존 프로젝트는 덮어쓰지 않는다."""
import json
from pathlib import Path
import sys

root = Path(sys.argv[1])
if root.exists() and any(root.iterdir()):
    raise SystemExit("빈 폴더 또는 새 경로를 지정하세요.")
(root / "scenes").mkdir(parents=True, exist_ok=True)
(root / "scripts").mkdir(exist_ok=True)
(root / "assets").mkdir(exist_ok=True)
project = dict(formatVersion=1, name="이벤트로 API 요청", startScene="start", scenes=["start"],
               globalScripts=[], dataTypes=[], networkConnections=[dict(id="local", baseUrl="http://127.0.0.1:8765", timeoutMs=10000, headers={})],
               variables=[dict(name="api_ok", value=False), dict(name="api_code", value=0), dict(name="api_error", value=""),
                          dict(name="공지내용", value=""), dict(name="점수", value=0), dict(name="분기결과", value="시작 전")])
request = dict(type="network_request", connection="local", path="/hello", method="GET", headers={}, body="",
               okVariable="api_ok", codeVariable="api_code", errorVariable="api_error", mappings={"/message": "공지내용", "/score": "점수"})
branch = dict(type="if_variable", variable="api_ok", compare="==", value=True,
              then=[dict(type="set_variable", variable="분기결과", op="set", value="성공"), dict(type="log", text="API 성공: 실행 변수에서 공지내용과 점수를 확인하세요.")],
              **{"else": [dict(type="set_variable", variable="분기결과", op="set", value="실패"), dict(type="log", text="API 실패: api_error와 api_code를 확인하세요.")]})
scene = dict(formatVersion=1, id="start", name="병렬 API 요청과 플레이어 이동", width=960, height=540,
             background="#202830", player="hero", objects=[
                 dict(id="hero", name="주인공", x=80, y=250, width=40, height=40, color="#5fa7cf"),
                 dict(id="notice", name="API 요청 (병렬 1회)", x=400, y=200, width=90, height=90, color="#dbb560", showName=True,
                      event=dict(trigger="parallel", repeat=False, commands=[request, branch]))])
for path, value in [(root / "project.json", project), (root / "scenes/start.json", scene)]:
    path.write_text(json.dumps(value, ensure_ascii=False, indent=2)+"\n", encoding="utf-8")
print(root)
