#pragma once

#include <unordered_map>

#include "leveldbr.grpc.pb.h"
#include "database.h"
#include "leveldbr.pb.h"

struct ConnState;
struct OpenDatabase;

typedef ::grpc::ServerReaderWriter< ::remote::OutMessage, ::remote::InMessage>* Stream;

class LevelDbService final : public remote::Leveldb::Service {
private:
    const std::string path;
    std::unordered_map<string,OpenDatabase*> open_dbs;
    std::mutex mu;
    void Open(Stream stream,ConnState& state,const ::remote::OpenRequest& msg);
    void Get(Stream stream,ConnState& state,const ::remote::GetRequest& msg);
    void Put(Stream stream,ConnState& state,const ::remote::PutRequest& msg);
    void Write(Stream stream,ConnState& state,const ::remote::WriteRequest& msg);
    void Lookup(Stream stream,ConnState& state,const ::remote::LookupRequest& msg);
    void LookupNext(Stream stream,ConnState& state,const ::remote::LookupNextRequest& msg);
    void Snapshot(Stream stream,ConnState& state,const ::remote::SnapshotRequest& msg);
    void Close(Stream stream,ConnState& state,const ::remote::CloseRequest& msg);
    void closedb(ConnState& state);
public:
    LevelDbService(std::string path) : path(path){}
    ::grpc::Status Connection(::grpc::ServerContext* context, Stream stream) override;
    ::grpc::Status Remove(::grpc::ServerContext* context, const ::remote::RemoveRequest* request, ::remote::RemoveReply* response) override;
};
