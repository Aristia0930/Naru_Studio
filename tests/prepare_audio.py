import importlib.util
from pathlib import Path
import shutil
import sys

root, target = Path(sys.argv[1]), Path(sys.argv[2])
# Only remove the explicitly provided build test project.
assert target.resolve().is_relative_to((root/'build').resolve())
if target.exists():
    shutil.rmtree(target)
spec=importlib.util.spec_from_file_location('audio_generator',root/'tools/generate_audio_example.py')
module=importlib.util.module_from_spec(spec)
spec.loader.exec_module(module)
module.generate(target)
