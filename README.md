# jufo-radio

The goal of this project is to digitalize an old radio and present the project at the "Jugend Forscht" science fair.

Because of this I will not be accepting pull requests etc.
In the offchance that you still have a suggestion or an objection about the source code, please make your voice heard via github discussions

## Building

To build, first install the [dependencies](#dependencies) (Marked dependencies can be ignored, as they are pulled in using gitmodules)
The repository can then be cloned with `git clone --recurse-submodules -b dragparent https://github.com/The-personified-devil/jufo-radio`
To build the application, run `make`

## Dependencies

- [VLC version 3.x](https://code.videolan.org/videolan/vlc-3.0)
- [libvlcpp](https://code.videolan.org/videolan/libvlcpp)
- [lvgl](https://github.com/lvgl/lvgl) Fork with changes necessary to build is supplied using gitmodules
- [lv\_drivers](https://github.com/lvgl/lv_drivers) Fork with changes necessary to build is supplied using gitmodules
