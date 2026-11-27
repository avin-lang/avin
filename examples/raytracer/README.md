# raytracer

A small Whitted-style ray tracer: spheres on a checkered floor under two lights, with shadows,
specular highlights and mirror reflections. Vectors are a `data` type with operator methods
(`a + b`, `v * k`, `-v`), shapes are a `data` type with a variant per kind of surface, and the
nearest hit is `shapes.flatMap(_.intersect(ray)).minByOption(_.t)`. The image is cut into bands of
rows that are rendered on threads of their own; the scene is immutable and its surface colours are
`@sendable` functions, so the threads share it without copying. The output is a plain-text PPM image
(`./main 640 400 > scene.ppm`), which most image viewers open.
