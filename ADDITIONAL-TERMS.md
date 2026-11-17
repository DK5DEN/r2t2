# Additional terms

Copyright (C) 2026 Dennis Eisold, DK5DEN

Unless a file says otherwise, this repository is licensed under the GNU Affero General Public
License, version 3 only (`AGPL-3.0-only`, see [`LICENSE`](LICENSE)), supplemented by the
following additional term under section 7(b) of that license.

## Attribution

The references to the original author and to afu.tools must not be removed. Every copy and
every modified version of this work, conveyed or run for users over a network, has to keep:

- the reference to the original author, Dennis Eisold, DK5DEN, with the link to the source
  repository <https://github.com/DK5DEN/r2t2>,
- the reference to afu.tools with the link to <https://afu.tools>.

This holds wherever the work shows these references: in the source files, in the
documentation and in a user interface. You may add notices about your own changes next to
them.

## Parts under other licenses

- `base-image/port/drivers-net-r2t2/` (the `rad0` driver, DL2STG, ported to Linux 6.12) and
  `base-image/port/dts/` (device tree) are parts of the Linux kernel and stay under
  `GPL-2.0`, see [`LICENSES/GPL-2.0.txt`](LICENSES/GPL-2.0.txt). The additional term above
  does not apply to them.
- The released SD card images contain Debian and the Linux kernel, each under its own
  licenses, and the unchanged `boot.bin` of the device.
