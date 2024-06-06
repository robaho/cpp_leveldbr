#pragma once

#include "leveldbr.grpc.pb.h"
#include "slice.h"
#include "writebatch.h"
#include <atomic>
#include <memory>

class RemoteDatabase;

typedef std::shared_ptr<RemoteDatabase> RemoteDb;

class RemoteIterator {
friend class RemoteDatabase;
private:
    uint64_t id;
    RemoteDb db;
    std::vector<KeyValue> entries;
    int index=0;
    RemoteIterator(uint64_t id,RemoteDb db) : id(id), db(db){}
public:
    const KeyValue& next();
    RemoteIterator(const RemoteIterator& ri) = delete;
};

class RemoteSnapshot {
friend class RemoteDatabase;
private:
    uint64_t id;
    RemoteDb db;
public:
    ByteBuffer get(Slice key);
    RemoteIterator lookup(Slice lower,Slice upper);
};

typedef std::unique_ptr<grpc::ClientReaderWriter<remote::InMessage, remote::OutMessage>> Stream;
typedef std::unique_ptr<::remote::Leveldb::Stub> RemoteClient;

class RemoteDatabase {
friend class RemoteIterator;
friend class RemoteSnapshot;
private:
    std::atomic_flag closed = false;
    RemoteClient client;
    Stream stream;
    grpc::ClientContext *context;
    std::weak_ptr<RemoteDatabase> weakRef;
    ByteBuffer snapshotGet(uint64_t id,const Slice& key) const;
    RemoteIterator snapshotLookup(uint64_t id,const Slice& lower,const Slice& upper) const;
    RemoteDatabase(RemoteClient client, Stream stream, grpc::ClientContext *context) : client(std::move(client)), stream(std::move(stream)) {
        this->context = context;
    }
public:
    ~RemoteDatabase() { 
        close(); 
        free(context);
    }
    static RemoteDb open(std::string addr,std::string dbName,bool createIfNeeded,int timeout);
    static void remove(std::string addr,std::string dbName,int timeout);
    void close();
    ByteBuffer get(const Slice& key) const { return snapshotGet(0,key); }
    void put(const Slice& key,const Slice& value);
    void remove(const Slice& key);
    void write(const WriteBatch& batch);
    RemoteIterator lookup(const Slice& lower,const Slice& upper);
    RemoteSnapshot snapshot();
};