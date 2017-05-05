# eos-model-viewer
3D model viewer for the eos Morphable Model library

This is a viewer that displays Morphable Models from the eos Morphable Model library ([github.com/patrikhuber/eos](https://github.com/patrikhuber/eos)).
It allows to play around with the shape and colour PCA models as well as the blendshapes.

![Screenshot of the viewer](https://github.com/patrikhuber/eos-model-viewer/blob/master/doc/viewer_screenshot.png)


## Build & installation

It uses the libigl 3D viewer and nanogui.
The viewer works well, the CMake scripts are somewhat in beta stage - you're on your own compiling it!

Make sure to clone the repository with `--recursive`, or, if already cloned, run `git submodule update --init --recursive`.

The plan is to also offer binaries.
