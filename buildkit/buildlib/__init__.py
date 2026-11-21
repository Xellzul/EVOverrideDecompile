"""The build kit's build path: buildkit/build.py is the entry point.

  host   build.ini, paths, processes, hashes
  work   the build root the repository's tools run in: their sources, the image's toolchain behind Wine launchers,
         the CW Pro 3 plugins' panels seeded from buildkit/panels
  check  `build.py check`: every file of unpack/layout.tsv in the image, by size and SHA-256, and the programs
"""
