# sfyri

sfyri is a simple and ultralightweight desktop app for writing disk image files (like `.iso` or `.img` files) onto USB drives and other removable media. It's the kind of tool you'd use to make a bootable USB stick.

It's built in C, and the app is small, fast, and free of heavy dependencies.

The whole workflow is just four steps:

1. Pick an image file
2. Pick a target disk
3. Confirm you've chosen the right one
4. Burn

## ⚠️ Before You Use It

sfyri writes directly to disks at a low level. If you pick the wrong disk, you can **permanently erase its contents**. Please read this before clicking Burn:

- **Double-check the device path.** Make sure it's really the USB drive you intend to overwrite, not your hard drive.
- **Unmount the drive first**, if your operating system requires it.
- **Expect to need admin/root permissions.** Writing raw data to a disk requires elevated privileges on macOS, Linux, and Windows.
- **Never select your system disk.** There is no undo.

When in doubt, stop and double-check rather than proceed.

## Features

- Clean native desktop interface
- Weighs only 30KB
- Standalone binary, no installers, no bloat
- Works on macOS, Linux, and Windows
- Live progress while burning, without freezing the interface

## What the App Looks Like

The main window has:

- A field for the image path, with a **Browse** button to open a file picker
- A list of available disks, with a **Refresh** button
- A status area that shows errors or progress
- A **Burn** button that asks for confirmation before doing anything
- A theme toggle, plus links to the project's website and GitHub page

## Installing

Grab the binary for your platform from the [releases page](https://github.com/lvtik/sfyri/releases).

```sh
tar -xzf sfyri-vX.X.X-<platform>-<arch>.tar.gz
cd sfyri-vX.X.X-<platform>-<arch>
sudo ./sfyri
```

Replace `vX.X.X` with the version you downloaded, and `<platform>-<arch>` with your system — e.g. `linux-x86_64`, `linux-arm64`, `darwin-x86_64`, `darwin-arm64`, or `windows-x86_64`.

On Windows, the release is a `.zip` containing `sfyri.exe` instead of a `.tar.gz`.

## Building

To build sfyri yourself, you'll need:

- A C compiler that supports C99
- `make`
- raylib installed on your system
- The bundled `lib/raygui/raygui.h` file (already included in this repo)

### On macOS

Install raylib using Homebrew:

```sh
brew install raylib
```

sfyri lists your disks using the built-in `diskutil list` command.

### On Linux

Install raylib using your distro's package manager, or build it from source.

sfyri lists your disks using:

```sh
lsblk -d -n -o NAME,PATH,TYPE
```

Heads up: actually burning an image will usually ask for elevated (sudo/root) permissions, since writing to a raw disk device requires it.

### On Windows

Build with [MSYS2](https://www.msys2.org/), using the UCRT64 environment. From an MSYS2 UCRT64 shell:

```sh
pacman -S mingw-w64-ucrt-x86_64-gcc mingw-w64-ucrt-x86_64-raylib make
```

sfyri lists your disks using PowerShell (`Get-CimInstance Win32_DiskDrive`), and writes to disks by their `\\.\PhysicalDriveN` path.

Heads up: burning an image requires running the app as Administrator, and the target disk must not have any of its partitions open in Explorer or another program.

### Compiling

Build it:

```sh
make
```

This creates the app at `bin/sfyri` (`bin/sfyri.exe` on Windows).

Other useful commands:

```sh
make rebuild   # rebuild everything from scratch
make clean     # remove build files
```

## Running the App

```sh
./bin/sfyri
```

If you get a permissions error when trying to burn an image, try running the app with elevated privileges (e.g. `sudo ./bin/sfyri` on Linux/macOS, or "Run as administrator" on Windows).

## Project Structure

If you're looking through the code, here's how it's organized:

```text
.
├── include/
│   ├── disk.h      # disk-related declarations
│   ├── img.h        # image-related declarations
│   └── ui.h         # UI declarations
├── src/
│   ├── disk.c        # finds and lists connected disks
│   ├── img.c          # checks image size, writes to disk
│   ├── main.c          # app entry point
│   └── ui.c             # the raylib/raygui interface
├── lib/
│   ├── raygui/            # bundled raygui headers
│   └── raylib/             # bundled raylib source (if not using a system install)
└── Makefile
```

**What each file does:**

- `src/ui.c` — the interface itself: theme switching, the file picker, disk selector, and the burn confirmation flow.
- `src/disk.c` — figures out what disks are connected, per platform (macOS, Linux, or Windows).
- `src/img.c` — checks the image file's size and burns it to disk on a background thread, per platform (POSIX vs. Windows raw I/O).
- The `Makefile` links against raylib installed on your system, and treats the bundled raygui headers as external headers.

## Links

- Website: <https://lvtik.github.io>
- Repository: <https://github.com/lvtik/sfyri>

## License

Licensed under MIT