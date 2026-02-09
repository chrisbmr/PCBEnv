import time

RENDER = False

STAY_OPEN_SECONDS = 1 if RENDER else 0

def stay_open(seconds=None):
    time.sleep(seconds if seconds is not None else STAY_OPEN_SECONDS)
