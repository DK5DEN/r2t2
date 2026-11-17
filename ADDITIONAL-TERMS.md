# Additional terms

Copyright (C) 2026 Dennis Eisold, DK5DEN

Unless a file says otherwise, this repository is licensed under the GNU Affero General Public
License, version 3 only (`AGPL-3.0-only`, see [`LICENSE`](LICENSE)), supplemented by the
additional terms below under section 7 of that license.

## 1. Attribution and source (section 7(b))

Every copy and every modified version of this work, conveyed or run for users over a network,
has to keep these author attributions:

- the notice "Based on R2T2 by Dennis Eisold, DK5DEN" with a link to the source repository
  <https://github.com/DK5DEN/r2t2>,
- the reference to afu.tools with a link to <https://afu.tools>,
- the afu.tools logo, wherever this work or the original it is derived from displays it.

Where the work has a user interface, these attributions belong to the Appropriate Legal
Notices and have to stay visible there, in the place and size in which the original shows
them. They may be supplemented by notices about your own changes, but not removed, hidden or
made unreadable.

## 2. Modified versions (section 7(c))

Modified versions have to be marked as changed in a reasonable way, so that they are not taken
for the original.

## 3. Names and logo (section 7(e))

The afu.tools name and logo are not licensed under the AGPL. This license grants no right to
use them for anything but the attribution required in section 1; in particular not to
advertise a modified version or to suggest that it comes from or is endorsed by afu.tools.

## Parts under other licenses

- `base-image/port/drivers-net-r2t2/` (the `rad0` driver, DL2STG, ported to Linux 6.12) and
  `base-image/port/dts/` (device tree) are parts of the Linux kernel and stay under
  `GPL-2.0`, see [`LICENSES/GPL-2.0.txt`](LICENSES/GPL-2.0.txt). The additional terms above do
  not apply to them.
- The released SD card images contain Debian and the Linux kernel, each under its own
  licenses, and the unchanged `boot.bin` of the device.
