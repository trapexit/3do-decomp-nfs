# Need for Speed for 3DO

<p align="center">
  <img src="assets/need_for_speed_banner.png" alt="The Need for Speed">
</p>

This is a decompilation and reconstruction of Road & Track Presents - The Need for Speed (USA, Europe)
for the 3DO, part of the [3DO Decomp
Project](https://www.patreon.com/trapexit/posts/announcing-3do-164834756).
The intent is not a byte-for-byte build of the original executables,
but a functional reconstructed codebase targeting the [3DO
DevKit](https://github.com/trapexit/3do-devkit), suitable as a starting
point for enhancements on the 3DO and ports to other platforms.


## Enhancements

* Boot BannerScreen, with reproducible source artwork.
* Correct movie-to-menu signal ownership and failure-safe resource teardown.
* Consistent per-bank memory checks for race music at startup and race entry.


## Download

As I do not hold the copyright for the game or assets, I'm not in a
position to upload a complete ISO. Releases are distributed as
[xdelta3](https://github.com/jmacd/xdelta) patch files instead.

Find the files on the [Releases page](https://github.com/trapexit/3do-decomp-nfs/releases).

You will need a copy of Road & Track Presents - The Need for Speed (USA, Europe) for the 3DO.
The reference ISO has SHA-256:

You will need a copy of Road & Track Presents - The Need for Speed (USA, Europe):

* ISO md5sum: `c7ad1ad975983997aa61bb3118edc1c4`
* BIN md5sum: `b213789b67a3368207a2ebf2c222847a`

If you have a `.bin` file, you can convert it to an ISO with 3dt:

`3dt to-iso "Road & Track Presents - The Need for Speed (USA, Europe).bin"`"


### Apply Patch

```sh
xdelta3 -d -s "Road & Track Presents - The Need for Speed (USA, Europe).iso" need_for_speed_v1.0.xdelta "Need for Speed v1.0.iso"
```

If you want a GUI to apply the patch, use [Delta
Patcher](https://github.com/marco-calautti/DeltaPatcher).


### Create Patch

```sh
xdelta3 -9 -S lzma -B1073741824 -e -s "Road & Track Presents - The Need for Speed (USA, Europe).iso" iso/need_for_speed.iso need_for_speed_v1.0.xdelta
```



## Building

As it targets the [3DO DevKit](https://github.com/trapexit/3do-devkit)
it works the same as other 3DO Devkit projects.

1. Dump your copy of Road & Track Presents - The Need for Speed (USA, Europe) for the 3DO. The md5sum of the
   ISO should be `c7ad1ad975983997aa61bb3118edc1c4`.
2. Download the devkit from https://github.com/trapexit/3do-devkit
3. Download this repo
4. Activate the devkit environment
5. `3dt unpack --only-assets "Road & Track Presents - The Need for Speed (USA, Europe).iso" -o takeme`
6. Run `make`

This will build the code, link the executable into `takeme/LaunchMe`
and build the iso at `iso/need_for_speed.iso`.


## Reporting Issues

This project, like all the [3DO Decomp
Project](https://www.patreon.com/trapexit/posts/announcing-3do-164834756)
reconstructions, was automated using custom tooling and AI to do a
bulk of the manual decompilation and reconstruction. Byte-for-byte
accuracy is not practical due to 3DO games regularly using different
static libraries, operating system versions, compilers, and
SDKs. Functional equivalence targeting the modern [3DO
DevKit](https://github.com/trapexit/3do-devkit) allows for ease of
maintenance but also means bugs can creep in more easily. If you find
any bugs please file a ticket on the
[issues](https://github.com/trapexit/3do-decomp-road-rash/issues)
page. Please include as much information as you can including
screenshots, platform (real hardware or emulator), etc.


## License

This project's original contributions are licensed under the
[GNU General Public License, version 2](LICENSE) (GPL-2.0-only).
This license does not grant rights to the original Road Rash game or
its assets, which remain the property of their respective copyright
holders.


## References

* https://www.patreon.com/trapexit/posts/announcing-3do-164834756
* https://www.twitch.tv/3dodev
* https://3dodev.com
* https://github.com/trapexit/3do-devkit


## Donations / Sponsorship

If you find the work I'm doing valuable please consider supporting its
ongoing development.

https://github.com/trapexit/support
