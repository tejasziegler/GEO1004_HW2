# GEO1004_HW2

```
.
├── report/                    Final report PDF (and LaTeX source)
├── data/                      Input 3DBAG tile and its _out output
└── cpp/
    ├── CMakeLists.txt         Build configuration
    ├── include/               Public headers — declarations of each module
    │   ├── json.hpp           nlohmann/json (third-party, untouched)
    │   ├── io.h               CityJSON read/write + LoD2.2 filtering
    │   ├── triangulate.h      Surface triangulation via PCA + CDT
    │   ├── volume.h           Per-building volume (signed tetrahedra)
    │   └── area.h             Per-building RoofSurface area
    └── src/                   Implementations
        ├── main.cpp           CLI, orchestrates the pipeline
        ├── io.cpp
        ├── triangulate.cpp
        ├── volume.cpp
        └── area.cpp
```
