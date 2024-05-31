# set this to the cpp_leveldb top-level project directory
CPP_LEVELDB = /Users/robertengels/cpp_leveldb

CXX = clang++
# CXXFLAGS = -std=c++20 -Wall -fsanitize=address -fno-omit-frame-pointer -pedantic-errors -g -I include
# CXXFLAGS = -std=c++20 -Wall -pedantic-errors -g -I include
CXXFLAGS = -std=c++20 -O3 -Wall -pedantic-errors -Wno-gcc-compat -g -I. -I${CPP_LEVELDB}/include

HEADERS = ${wildcard *.h}

SRCS = main_server.cpp main_client.cpp server.cpp client.cpp

PROTO_OBJS = bin/leveldbr.pb.o bin/leveldbr.grpc.pb.o

client.h: generate

leveldbr.pb.cc: leveldbr.proto
	protoc --cpp_out=. leveldbr.proto

bin/leveldbr.pb.o: leveldbr.pb.cc
	@ mkdir -p bin
	${CXX} ${CXXFLAGS} -c $(notdir $(basename $@).cc) -o $@

leveldbr.grpc.pb.cc: leveldbr.proto
	protoc --grpc_out=. --plugin=protoc-gen-grpc=`which grpc_cpp_plugin` leveldbr.proto

bin/leveldbr.grpc.pb.o: leveldbr.grpc.pb.cc
	@ mkdir -p bin
	${CXX} ${CXXFLAGS} -c $(notdir $(basename $@).cc) -o $@

SERVER_OBJS = bin/main_server.o bin/server.o ${PROTO_OBJS}
SERVER = bin/cpp_leveldb_server

CLIENT_OBJS = bin/main_client.o bin/client.o ${PROTO_OBJS}
CLIENT = bin/cpp_leveldb_client

.PRECIOUS: bin/%.o

all: ${SERVER} ${CLIENT}
	@echo compile finished

${SERVER}: ${SERVER_OBJS}
	${CXX} ${CXXFLAGS} ${SERVER_OBJS} ${CPP_LEVELDB}/bin/cpp_leveldb.a -o ${SERVER} -lprotobuf -labsl_log_internal_message -labsl_log_internal_check_op -labsl_status -l absl_cord -labsl_strings -lgrpc++ -lgrpc -lgpr

${CLIENT}: ${CLIENT_OBJS}
	${CXX} ${CXXFLAGS} ${CLIENT_OBJS} ${CPP_LEVELDB}/bin/cpp_leveldb.a -o ${CLIENT} -lprotobuf -labsl_log_internal_message -labsl_log_internal_check_op -labsl_status -l absl_cord -labsl_strings -lgrpc++ -lgrpc -lgpr

bin/%.o: %.cpp ${HEADERS}
	@ mkdir -p bin
	${CXX} ${CXXFLAGS} -c $(notdir $(basename $@).cpp) -o $@

generate: ${PROTO_OBJS}
	@ echo generated proto files

clean:
	rm -rf bin *~.
	rm -f leveldbr.pb.cc
	rm -f leveldbr.pb.h
	rm -f leveldbr.grpc.pb.cc
	rm -f leveldbr.grpc.pb.h
