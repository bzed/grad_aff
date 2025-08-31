# grad_aff - linux edition by @bzed

# CLI

```
grad_aff CLI Tool
Usage: grad_aff_cli <command> [options]

Commands:
  pbo info <pbo_file>                 Show information about a PBO file.
  pbo extract <pbo_file> <out_dir>    Extract a PBO file to the target directory.
  paa info <paa_file>                 Show information about a PAA file.
  paa to-png <paa_file> <out_png>     Convert a PAA file to a PNG image.
  paa from-png <in_png> <out_paa>     Convert a PNG image to a PAA file.
  p3d info <p3d_file>                 Show information about a P3D model file.
  wrp info <wrp_file>                 Show information about a WRP file.
  help                                Show this help message.
```

# Building

Hint: Don't forget to checkout the git submodules!

To build under a current Debian you need the following packages:

```
apt install \
    build-essential cmake \
    tao-pegtl-dev libtsl-ordered-map-dev \
    libboost-dev libsquish-dev libcatch2-dev libtbb-dev \
    libopenimageio-dev openimageio-tools
```

Unfortunately, the pkg-config file of libsquish is broken, this is a hacky way to fix it:
```
sudo sed 's,prefix=,prefix=/usr,' -i /usr/lib/*/pkgconfig/libsquish.pc
```

Build with
```
cmake -S . -B build; make -C build
```

The resulting binary will be in `build/cli/grad_aff_cli`, install it to your preferred PATH directory.
