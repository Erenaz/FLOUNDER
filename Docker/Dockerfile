FROM mambaorg/micromamba:1.5.10
ENV MAMBA_DOCKERFILE_ACTIVATE=1
RUN micromamba install -y -n base -c conda-forge \
    geant4=11.3.* geant4-data-* \
    root>=6.30 \
    cmake ninja make pkg-config \
    gcc_linux-64 gxx_linux-64 gfortran_linux-64 binutils_linux-64 \
    xerces-c tbb git python=3.11 \
 && micromamba clean -a -y
ENV Geant4_DIR=/opt/conda/lib/cmake/Geant4
ENV CMAKE_PREFIX_PATH=/opt/conda
SHELL ["/bin/bash","-lc"]