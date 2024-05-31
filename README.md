## summary

This is the C++ port of my [keydbr](https://github.com/robaho/leveldbr) key/value database client/server module.

## background

leveldb itself does not support multi-process access. This is a "wrapper" process that exposes leveldb databases via gRPC.

## building

Set the location of the `cpp_leveldb` project in the `Makefile` using the `CPP_LEVELDB` variable.

The `cpp_leveldb` project must be built prior to building `cpp_leveldbr`.

`make all`

## running server

`bin/cpp_leveldb_server`

## running remote client test

`bin/cpp_leveldb_client`

