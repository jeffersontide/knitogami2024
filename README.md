# Knit Simulation

Simulation code to support the paper [Geometric modeling of knitted fabrics](https://www.pnas.org/doi/10.1073/pnas.2416536122), a.k.a. [You Have to Grow Wefts to Fold Them](https://arxiv.org/abs/2408.08409).

## Compilation

To build, see the original [codebase wiki](https://github.com/wimvanrees/growth_SM2018/wiki) for instructions on how to compile and run. You may need to add extra locations to your PATH variable, e.g.,
```
export PATH="/opt/homebrew/opt/qt@5/bin:$PATH"
```

Note: for Apple Silicon devices, a reviewer has suggested the following steps to help load the `libigl` library:
```
(1) You can download the libigl library from their github page.
(2) Then add it to the directory src/libshell/igl.
(3) Go into your common.hpp file and edit lines 35-38 as follows:
        #include "igl/barycentric_coordinates.h"
        #include "igl/facet_components.h"
        #include "igl/point_mesh_squared_distance.h"
        #include "igl/remove_unreferenced.h"
That will get cmake to search local directories and it will find it in the src.
```

## Workflow

The build process outputs an executable, `shell`, in the `/bin` directory.

Create a folder within `/bin` to store all files related to a particular run, e.g.:
```
cd bin
mkdir run_miura
cd run_miura
```

Copy the executable and any relevant knit pattern files to the run directory:
```
cp ../shell .
cp ../patterns/knit_3a_miura.txt .
```
Each pattern file is simply a text file containing a rectangular, whitespace-delimited pattern of 'K' and 'P' characters, defining a knitted stitch motif of knit (K) and purl (P) stitches. The first line of each pattern file specifies the number of rows and columns to tile the motif.

Run the executable with any specified parameters:
```
./shell -sim knit -filename knit_3a_miura.txt -xCurv -1.0 -yCurv 1.0 \
    -res 0.5 -dx 1.4 -dy 1.2 -h 0.5 -E 1.0 -nu 0.4 \
    -boundarySpringConstant 0.0 -nSteps 10
```

The result is a set of VTP files containing the resulting meshes at each quasistatic simulation step, as values of $\kappa_x$ are increased from $0$ to $xCurv$ and $\kappa_y$ is increased from $0$ to $yCurv$. VTP files can be viewed with [Paraview](https://www.paraview.org).

### Parameters

* `-dx` and `-dy` specify the `x`- and `y`-dimensions of a single knit or purl stitch within the knit pattern.
* `-xCurv` and `-yCurv` specify the natural curvatures in the $x$ and $y$ directions for "knit" regions of the surface. For "purl" regions, curvatures of the opposite sign are used.
* In the Föppl-von-Kármán model of a surface, `-h` specifies the thickness parameter, `-E` specifies the Young's Modulus, and `-nu` specifies the Poisson ratio of the material.
* `-boundarySpringConstant` includes a Hookean spring energy between the boundary vertices of the mesh and their initial positions in the `xy`-plane, with linear spring constant density (along the boundary) equal to the specified value.

In Figure 3 of the paper, all simulations used the parameters `h = 0.5`, `dx = 1.4`, `dy = 1.2`, `E = 1`, `nu = 0.4`, `xCurv = -1`, and `yCurv = 1`. Corresponding patterns are labeled in `bin/patterns`. In simulations corresponding to Figures 3 b, c, f, and g, the value `boundarySpringConstant = 0.0001` was used; in all others, `boundarySpringConstant = 0` was used.

