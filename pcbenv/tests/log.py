import logging
import types
import numpy as np

DEBUG = logging.DEBUG
INFO = logging.INFO
NOTICE = logging.INFO + 5
WARNING = logging.WARNING
ERROR = logging.ERROR

_initialized = False

def notice(self, msg, *args, **kwargs):
    self.log(NOTICE, msg, *args, **kwargs)

def init(level=None, dest=None):
    global _initialized
    log = logging.getLogger('default')
    if _initialized:
        if level is not None:
            log.setLevel(level)
        return log

    fmt = logging.Formatter(fmt="[%(process)d] [%(asctime)s] %(levelname)s %(message)s")
    hnd = logging.StreamHandler()

    hnd.setFormatter(fmt)
    log.addHandler(hnd)
    log.setLevel(level if level is not None else INFO)
    log.notice = types.MethodType(notice, log)

    if dest is not None:
        log.addHandler(logging.FileHandler(dest))

    logging.addLevelName(logging.DEBUG, "D")
    logging.addLevelName(logging.INFO, "I")
    logging.addLevelName(NOTICE, "N")
    logging.addLevelName(logging.WARNING, "W")
    logging.addLevelName(logging.ERROR, "E")

    np.set_printoptions(linewidth=128)

    _initialized = True
    return log
