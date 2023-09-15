import math
import sys

# https://www.rastergrid.com/blog/2010/09/efficient-gaussian-blur-with-linear-sampling/
def gaussian(x, c):
    return (1 / math.sqrt(2 * math.pi * c * c)) * math.exp(-(x*x)/(2*c*c))

size = int(sys.argv[1])
variance = int(sys.argv[2])

kernel = [gaussian(x, variance) for x in range(0, size)]

print(f"// Generated with command: ", end="")
print(" ".join(sys.argv))
print(f"#define KERNEL_SIZE {size}")
print(f"const float kernel[KERNEL_SIZE] = float[KERNEL_SIZE](", end="")
print(", ".join(map("{:.6f}".format, kernel)), end="")
print(");")
