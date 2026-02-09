
import math

class Vec2:
    def __init__(self, a, b=None):
        self.x = (a['x'] if isinstance(a, dict) else a[0]) if b is None else a
        self.y = (a['y'] if isinstance(a, dict) else a[1]) if b is None else b

    def __add__(self, v):
        return Vec2(self.x + v.x, self.y + v.y)
    def __sub__(self, v):
        return Vec2(self.x - v.x, self.y - v.y)
    def __mul__(self, s):
        return Vec2(self.x * s, self.y * s)

    def abs(self):
        return Vec2(abs(self.x), abs(self.y))

    def dot(self, v):
        return self.x * v.x + self.y * v.y

    @property
    def len_squared(self):
        return self.x * self.x + self.y * self.y
    @property
    def len(self):
        return math.sqrt(self.len_squared)
    @property
    def len45(self):
        x = abs(self.x)
        y = abs(self.y)
        return x + y - min(x, y) * (2 - math.sqrt(2))

    def rescaled(self, s):
        s /= self.len_euclidean
        return Vec2(self.x * s, self.y * s)

    def rotated_90ccw(self):
        return Vec2(-self.y, self.x)
