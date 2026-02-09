import numpy as np
import matplotlib.pyplot as plt

def add_sat(a,b):
    return np.clip(a.astype(np.int16) + b.astype(np.int16), 0, np.iinfo(a.dtype).max).astype(a.dtype)

def as_color(x,r,g,b):
    return np.stack([x*r,x*g,x*b], axis=-1).astype(np.uint8)

def chans_to_rgb(X):
    rgb = np.zeros((X.shape[0], X.shape[1], 3), dtype=np.uint8)
    rgb = add_sat(rgb, as_color(X[:,:,0], 1, 0, 0))
    rgb = add_sat(rgb, as_color(X[:,:,3], 1, 1, 0))
    rgb = add_sat(rgb, as_color(X[:,:,6], 1, 0, 0.5))
    rgb = add_sat(rgb, as_color(X[:,:,1], 0, 1, 0))
    rgb = add_sat(rgb, as_color(X[:,:,4], 0, 0, 1))
    rgb = add_sat(rgb, as_color(X[:,:,2], 1, 1, 1))
    rgb = add_sat(rgb, as_color(X[:,:,5], 0.5, 0.5, 0.5))
    return rgb

def show_image(X):
    plt.imshow(chans_to_rgb(X))
    plt.show()
