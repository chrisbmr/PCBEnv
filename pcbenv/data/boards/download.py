import pathlib
import urllib.request
import urllib.parse
import zipfile

MANIFEST = {
    "A64 OlinuXino Rev G": "https://raw.githubusercontent.com/OLIMEX/OLINUXINO/refs/heads/master/HARDWARE/A64-OLinuXino/2.%20Older%20hardware%20revisions/A64-OLinuXino%20hardware%20revision%20G/A64-OlinuXino_Rev_G.kicad_pcb",
    "ASP-DAC 2021": "https://github.com/aspdac-submission-pcb-layout/PCBBenchmarks/archive/refs/heads/master.zip",
    "CIAA ACC": "https://raw.githubusercontent.com/ciaa/Hardware/master/PCB/ACC/CIAA_ACC/ciaa_acc.kicad_pcb"
}

SCRIPT_DIR = pathlib.Path(__file__).parent.resolve()
BOARDS_DIR = SCRIPT_DIR

BOARDS_DIR.mkdir(exist_ok=True)

print(f"Downloading boards to {BOARDS_DIR} ...")

for name, src in MANIFEST.items():
    dst = BOARDS_DIR / pathlib.Path(urllib.parse.urlparse(src).path).name

    if dst.name == "master.zip":
        dst = dst.with_name(name + ' ' + dst.name)

    if dst.exists():
        print(f"Skipped {dst} as it already exists.")
        continue

    print(f"Fetching {name} from {src} and saving to {dst} ...")
    try:
        urllib.request.urlretrieve(src, dst)
    except:
        continue

    if dst.suffix == '.zip':
        print(f"Extracting {dst} ...")
        with zipfile.ZipFile(dst) as zipf:
            zipf.extractall(BOARDS_DIR)
