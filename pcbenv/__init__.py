import json
import pcbenv.EnvPCB as EnvPCB

from importlib_resources import files, path

def make(name="pcb-v1", conf=None):
    PATH = [
        str(files('pcbenv.data').joinpath('*.json')),
        str(files('pcbenv.data.ui.glsl').joinpath('*.glsl')),
        str(files('pcbenv.data.ui.qt').joinpath('*.ui'))
    ]
    return EnvPCB.create_env({
        'name': name,
        'PATH': PATH,
        'settings_json': None if conf is None else json.dumps(conf)
    })
