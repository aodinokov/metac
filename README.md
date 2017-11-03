# metac
[![Build Status](https://travis-ci.org/aodinokov/metac.svg?branch=master)](https://travis-ci.org/aodinokov/metac)

Reflection implementation for C, based on DWARF data(debug info used by gdb).
This project is inspired by https://github.com/alexanderchuranov/Metaresc.

For now it's a PoC that is intended to show "How C can benefit if we add reflection" and at the same time "How easy it's to add this feature - we already have it in DWARF".
e.g. this project shows that it's possible to automatically serialize/deserialize binary data to/from JSON. 
It is also possible to create kind of RPC (may be even JSON-RPC), because it's possible to get function declaration from DWARF and use it for FFI calls (see libffi).

We are not taking DWARF as-is (because there may be types we don't want to have available in runtime).
Instead METAC allow to mark only requered types in .c file.
Once it's done it's possible to compile the file with debug flags (see Makefile), run metac.awk to generate .metac.c file with reflection information and use API declared in metac.h and implemented in metac.c.

metac_s11n_json.c is an example of METAC API usage (see metac_s11n_json_ut_001.c for covered cases).
