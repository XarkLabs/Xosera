# Xoboing Boing Ball Demo for Xosera

Copyright (c) 2022 Thomas Jager
This version is a derivative of the original at <https://gitlab.com/0xTJ/xoboing>.  

[MIT License](LICENSE)

I'd like to thank 0xTJ for making this awesome demo available for Xosera.  Releasing it has helped with testing and improving Xosera (as well as it being a fun and nostalgic demo - which is what this project is really all about). -Xark

0xTJ gets all the credit, only the proof-of-concept bounce audio was added along with some Xosera API and build modifications.

## Building

Export environment variable `XOSERA_M68K_API` to point to the `Xosera/xosera_m68k_api` folder containing the Xosera m68k API to use (from Xosera repository).  **NOTE:** This is not necessary if building from within the Xosera repository (`XOSERA_M68K_API` will default to `../xosera_m68k_api` from the repository).

To build a `.bin` file:

```shell
export XOSERA_M68K_API=/home/user/github/Xosera/xosera_m68k_api
cd xosera_boing_m68k
make
```

To upload to rosco_m68k, make sure `SERIAL` and `BAUD` are set correctly and
you can do either `make load` (generic kermit), `make test` (Linux using screen) or `make mactest` (macOS using screen).

```shell
make load
```
