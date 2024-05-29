#include <cstddef>
#include <exception>
#include <grpcpp/support/status.h>
#include <mutex>

#include "server.h"
#include "exceptions.h"
#include "leveldbr.pb.h"
#include "keyvalue.h"

using namespace remote;

struct OpenDatabase {
	int refcount = 0;
	DatabaseRef db;
	std::string dbname;
};

struct ConnState {
	OpenDatabase *opendb = nullptr;
    std::unordered_map<uint64_t,LookupRef> itrs;
    std::unordered_map<uint64_t,SnapshotRef> snapshots;
    uint64_t next; // next iterator id
};

::grpc::Status LevelDbService::Remove(::grpc::ServerContext *context, const ::remote::RemoveRequest *request, ::remote::RemoveReply *response) {
    std::lock_guard<std::mutex> lock(mu);

	std::cout << "remove database " << request->dbname() << "\n";

	auto fullpath = path+"/"+request->dbname();
    std::string error = "";

    try {
        Database::remove(fullpath);
    } catch(std::exception& ex) {
        error = ex.what();
    }
    response->set_error(error);

    return ::grpc::Status::OK;
}

::grpc::Status LevelDbService::Connection(::grpc::ServerContext* context, Stream stream) {
    ConnState state;

    std::cout << "connection opened from " << context->peer() << "\n";

    remote::InMessage msg;
    while(stream->Read(&msg)) {
        if(msg.has_open()) {
            Open(stream,state,msg.open());
        } else if (msg.has_get()) {
            Get(stream,state,msg.get());
        } else if (msg.has_put()) {
            Put(stream,state,msg.put());
        } else if (msg.has_write()) {
            Write(stream,state,msg.write());
        } else if (msg.has_lookup()) {
            Lookup(stream,state,msg.lookup());
        } else if (msg.has_next()) {
            LookupNext(stream,state,msg.next());
        } else if (msg.has_snapshot()) {
            Snapshot(stream,state,msg.snapshot());
        } else if (msg.has_close()) {
            Close(stream,state,msg.close());
        } else {
            std::cout << "unsupported message " << msg.DebugString() << "\n";
            break;
        }
	}
    std::cout << "connection terminated\n";
    try {
        closedb(state);
    } catch(std::exception& ex) {
        std::cout << "closing open db failed " << ex.what() << "\n";
    }
    return ::grpc::Status::OK;
}

void LevelDbService::closedb(ConnState& state) {
    std::lock_guard<std::mutex> lock(mu);

	if(!state.opendb) {
		return;
	}

	auto dbname = state.opendb->dbname;
	state.opendb = nullptr;

	std::cout << "closing database " << dbname << "\n";

    auto opendb = open_dbs[dbname];
    if(!opendb) throw DatabaseClosed();

	opendb->refcount--;

	if(opendb->refcount == 0) {
        opendb->db->close();
        open_dbs[dbname] = nullptr;
        free(opendb);
	}
}

void LevelDbService::Open(Stream stream,ConnState& state,const ::remote::OpenRequest& msg) {
    std::lock_guard<std::mutex> lock(mu);
    auto fullpath = path+"/"+msg.dbname();

    std::cout << "request to open " << msg.dbname() << "\n";
    auto opendb = open_dbs[fullpath];
    std::string error = "";
    Options options;

    if(opendb==nullptr) {
        opendb = new OpenDatabase();
        opendb->dbname = msg.dbname();
        opendb->refcount = 1;
        options.createIfNeeded = msg.create();
        try {
            opendb->db = Database::open(path+"/"+opendb->dbname, options);
            open_dbs[opendb->dbname] = opendb;
            state.opendb = opendb;
            std::cout << "opened " << opendb->dbname << "\n";
        } catch(std::exception& ex) {
            error = ex.what();
            free(opendb);
        }
    } else {
        opendb->refcount++;
        state.opendb = opendb;
    }
    ::remote::OutMessage out;
    auto reply = out.mutable_open();
    reply->set_error(error);
    stream->Write(out);
}

void LevelDbService::Close(Stream stream,ConnState& state,const ::remote::CloseRequest& msg) {
    std::cout << "request to close database\n";
    std::string error = "";
    try {
        closedb(state);
    } catch(std::exception& ex) {
        error = ex.what();
    }

    ::remote::OutMessage out;
    auto reply = out.mutable_close();
    reply->set_error(error);
    stream->Write(out);
}

void LevelDbService::Get(Stream stream,ConnState& state,const ::remote::GetRequest& msg) {
    std::string error = "";
    ByteBuffer value;
    if(msg.snapshot()==0) {
        try {
            state.opendb->db->get(msg.key(),value);
        } catch(std::exception& ex) {
            error = ex.what();
        }
    } else {
        auto snapshot = state.snapshots[msg.snapshot()];
        if(snapshot.get()==nullptr) {
            error = "invalid snapshot id";
        } else {
            try {
                snapshot->get(msg.key(),value);
            } catch(std::exception& ex) {
                error = ex.what();
            }
        }
    }
    ::remote::OutMessage out;
    auto reply = out.mutable_get();
    if(value.empty()) error = "key not found";
    reply->set_error(error);
    reply->set_value(value);
    stream->Write(out);
}

void LevelDbService::Put(Stream stream,ConnState& state,const ::remote::PutRequest& msg) {
    std::string error = "";
    try {
        state.opendb->db->put(msg.key(),msg.value());
    } catch(std::exception& ex) {
        error = ex.what();
    }
    ::remote::OutMessage out;
    auto reply = out.mutable_put();
    reply->set_error(error);
    stream->Write(out);
}

void LevelDbService::Write(Stream stream,ConnState& state,const ::remote::WriteRequest& msg) {
    std::string error = "";
    try {
        WriteBatch wb;
        for(auto e : msg.entries()) {
            wb.put(e.key(),e.value());
        }
        state.opendb->db->write(wb);
    } catch(std::exception& ex) {
        error = ex.what();
    }
    ::remote::OutMessage out;
    auto reply = out.mutable_write();
    reply->set_error(error);
    stream->Write(out);
}

void LevelDbService::Lookup(Stream stream,ConnState& state,const ::remote::LookupRequest& msg) {
	LookupRef itr;
    std::string error = "";

	if(msg.snapshot() == 0) {
		itr = state.opendb->db->lookup(msg.lower(),msg.upper());
	} else {
		auto snapshot = state.snapshots[msg.snapshot()];
        if(snapshot==nullptr) {
            error = "invalid snapshot id";
		} else {
			itr = snapshot->lookup(msg.lower(), msg.upper());
		}
	}

    uint64_t id = 0;

	if(itr.get() != nullptr) {
		state.next++;
		id = state.next;
		state.itrs[id] = itr;
	}
    ::remote::OutMessage out;
    auto reply = out.mutable_lookup();
    reply->set_error(error);
    reply->set_id(id);
    stream->Write(out);
}

void LevelDbService::LookupNext(Stream stream,ConnState& state,const ::remote::LookupNextRequest& msg) {
	LookupRef itr;
    std::string error = "";

    ::remote::OutMessage out;
    auto reply = out.mutable_next();

    itr = state.itrs[msg.id()];
    if(itr.get()==nullptr) {
        error = "invalid iterator id";
    } else {
		int count = 0;
        std::vector<::remote::KeyValue> entries(64);

        ::KeyValue kv;
		while(count<64) {
            itr->next(kv);
            if(!kv.key.empty()) {
                auto pbe = reply->add_entries();
                pbe->set_key(kv.key);
                pbe->set_value(kv.value);
			} else {
				if(count > 0) {
					break;
				}
                state.itrs[msg.id()]=nullptr;
                error = "end of iterator";
				break;
			}
			count++;
		}
    }
    reply->set_error(error);
    stream->Write(out);
}

void LevelDbService::Snapshot(Stream stream,ConnState& state,const ::remote::SnapshotRequest& msg) {
    std::string error = "";

    uint64_t id = 0;
	auto snapshot = state.opendb->db->snapshot();

	if(snapshot.get()!=nullptr) {
		state.next++;
		id = state.next;
		state.snapshots[id] = snapshot;
	}

    ::remote::OutMessage out;
    auto reply = out.mutable_snapshot();
    reply->set_id(id);
    reply->set_error(error);
    stream->Write(out);
}
