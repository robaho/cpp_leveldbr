## summary

This is the C++ port of my [keydb](https://github.com/robaho/leveldbr) key/value database client/server module.

## background

leveldb does not support multi-process access. This is a "wrapper" process that exposes leveldb databases via gRPC.

## todo

Only the server component is currently available. This can be access with either the Go or Java remote clients. The C++ client module is being developed.

