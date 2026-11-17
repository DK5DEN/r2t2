# boot.bin

`boot.bin` of the DARC R2T2 as the devices were delivered (17.08.2016, 4429280 bytes,
sha256 `b202387c09e1b97f26ae46852fa9252a01d531d9619521eb2f93f6d238ce74ee`), copied unchanged
from the SD card of a device. It holds the first stage boot loader (FSBL), the FPGA bitstream
and U-Boot 2014.07, and loads `image.ub` from the same partition.

It is here so that an image can be built, and a card be written, without an original SD card.
It is not built from this repository. Its sources are the original R2T2 project by Stefan
Goerg (DL2STG), https://github.com/dh1tw/r2t2 (GPL-3.0; U-Boot is GPL-2.0).
