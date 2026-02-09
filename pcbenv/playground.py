import argparse
import pathlib
import pcbenv

##################################
# Parse command line arguments ###
##################################

# -t Load tracks
# <board> A board file (.kicad_pcb or .json)

args = argparse.ArgumentParser()

args.add_argument("-t", action='store_true', help="Load tracks", default=False)
args.add_argument("board", type=str, nargs='?', help="Path to board file (json or kicad_pcb).", default="data/boards/PCBBenchmarks-master/bm1/bm1.unrouted.kicad_pcb")

args = args.parse_args()

path = str(pathlib.Path(args.board).absolute())


##########################################
# Create and use an environment instance #
##########################################

env = pcbenv.make()

print("Opening board", path)

# Load PCB
env.set_task({ "pdes": path, "load_tracks": args.t })

# Open GUI window (if available)
env.render("human")

# Wait for user actions
input("")
