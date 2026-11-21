# Escape Velocity Override

Byte-matching decompilation of **Escape Velocity: Override 1.0.2f** for PowerPC Mac

You need:
 - Provide files from **input-files/README.md**
 - **Docker**

Prepare build enviroment:
 - `docker build --platform linux/amd64 --build-context inputs=input-files -t evo-buildkit buildkit`

Run build:
 - `docker run --rm --platform linux/amd64 --security-opt seccomp=unconfined -v "$PWD":/repo evo-buildkit`

Outputs file to: buildkit/out/

Verify the image (every tool, library, header and game file by size and SHA-256):

`docker run --rm --platform linux/amd64 --security-opt seccomp=unconfined -v "$PWD":/repo evo-buildkit python3 /repo/buildkit/build.py check`

Options for `build.py`:
 - `--config FILE` - use another build.ini (default: buildkit/build.ini)
 - `--keep` - keep the build root (/var/tmp/evo-kit/evo) to look at the objects and link maps