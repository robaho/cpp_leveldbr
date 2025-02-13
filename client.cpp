#include <chrono>
#include <string>

#include "exceptions.h"
#include "grpcpp/create_channel.h"
#include "grpcpp/security/credentials.h"
#include "client.h"
#include "keyvalue.h"
#include "leveldbr.grpc.pb.h"
#include "leveldbr.pb.h"
#include "exceptions.h"

static void mapError(std::string error) {
    if(error=="no database found") throw DatabaseNotFound();
    if(error=="end of iterator") throw EndOfIterator();
    if(error=="database corrupted") throw DatabaseCorrupted();
    throw DatabaseException("database error");
}

RemoteDb RemoteDatabase::open(std::string addr,std::string dbName,bool createIfNeeded,int timeout){
    auto channel = grpc::CreateChannel(addr,grpc::InsecureChannelCredentials());
    auto stub = ::remote::Leveldb::NewStub(channel);

    grpc::ClientContext *context = new grpc::ClientContext();

    auto stream = stub->Connection(context);
    if(stream.get()==nullptr) {
        throw DatabaseOpenFailed("failed to connect");
    }

    auto db = new RemoteDatabase(std::move(stub),std::move(stream),context);

    ::remote::InMessage msg;
    auto request = msg.mutable_open();
    request->set_dbname(dbName);
    request->set_create(createIfNeeded);
    if(!db->stream->Write(msg)) throw DatabaseOpenFailed(("failed to write "+context->debug_error_string()).c_str());

    ::remote::OutMessage reply;
    db->stream->Read(&reply);
    auto response = reply.open();
    if(response.error()!="") mapError(response.error());

    RemoteDb remote(db);
    db->weakRef = remote;
    return remote;
}

void RemoteDatabase::remove(std::string addr,std::string dbName,int timeout){
    auto channel = grpc::CreateChannel(addr,grpc::InsecureChannelCredentials());
    auto stub = ::remote::Leveldb::NewStub(channel);

    grpc::ClientContext context;
    if(timeout!=0) {
        std::chrono::time_point deadline = std::chrono::system_clock::time_point();
        context.set_deadline(deadline);
    }

    ::remote::RemoveRequest request;
    ::remote::RemoveReply reply;
    request.set_dbname(dbName);
    auto status = stub->Remove(&context,request,&reply);
    if(!status.ok()) throw DatabaseException("remove rpc failed");
    if(reply.error()!="") mapError(reply.error());
}

void RemoteDatabase::close() {
    if(!closed.test_and_set()) return;

    ::remote::InMessage msg;
    msg.mutable_close();
    stream->Write(msg);

    ::remote::OutMessage reply;
    stream->Read(&reply);
    auto response = reply.close();
    if(response.error()!="") mapError(response.error());
}

ByteBuffer RemoteDatabase::snapshotGet(uint64_t id,const Slice& key) const {
    ::remote::InMessage request;
    auto get = request.mutable_get();
    get->set_snapshot(id);
    get->set_key(key);
    stream->Write(request);

    ::remote::OutMessage response;
    if(!stream->Read(&response)) throw DatabaseException("snapshot failed");
    auto reply = response.get();
    if(reply.error()!="") mapError(reply.error());
    return reply.value();
}

void RemoteDatabase::put(const Slice& key,const Slice& value) {
    ::remote::InMessage request;
    auto put = request.mutable_put();
    put->set_key(key);
    put->set_value(value);
    stream->Write(request);

    ::remote::OutMessage response;
    if(!stream->Read(&response)) throw DatabaseException("put failed");
    auto reply = response.put();
    if(reply.error()!="") mapError(reply.error());
}

void RemoteDatabase::write(const WriteBatch& batch) {
    ::remote::InMessage request;
    auto write = request.mutable_write();
    for(auto e : batch.entries) {
        auto entry = write->add_entries();
        entry->set_key(e.key);
        entry->set_value(e.value);
    }
    stream->Write(request);

    ::remote::OutMessage response;
    if(!stream->Read(&response)) throw DatabaseException("stream failed");
    auto reply = response.write();
    if(reply.error()!="") mapError(reply.error());
}

RemoteIterator RemoteDatabase::lookup(const Slice& lower,const Slice& upper) {
    return snapshotLookup(0,lower,upper);
}

RemoteIterator RemoteDatabase::snapshotLookup(uint64_t id,const Slice& lower,const Slice& upper) const {
    ::remote::InMessage request;
    auto lookup = request.mutable_lookup();
    lookup->set_snapshot(id);
    lookup->set_lower(lower);
    lookup->set_upper(upper);
    stream->Write(request);

    ::remote::OutMessage response;
    if(!stream->Read(&response)) throw DatabaseException("snapshot failed");
    auto reply = response.lookup();
    if(reply.error()!="") mapError(reply.error());

    return RemoteIterator(reply.id(),RemoteDb(weakRef));
}

const KeyValue& RemoteIterator::next() {
    if(index<entries.size()) {
        return entries[index++];
    }

    ::remote::InMessage request;
    auto next = request.mutable_next();
    next->set_id(id);
    db->stream->Write(request);

    ::remote::OutMessage response;
    if(!db->stream->Read(&response)) throw DatabaseException("next failed");
    auto reply = response.next();
    try {
        if(reply.error()!="") mapError(reply.error());
    } catch(EndOfIterator& ex) {
        return KeyValue::EMPTY();
    }

    entries.clear();
    entries.reserve(reply.entries_size());
    
    for(auto e : reply.entries()) {
        entries.push_back(KeyValue(e.key(),e.value()));
    }
    index=1;
    return entries[0];
}
