#include <exception>
#include <grpc/grpc.h>
#include <grpcpp/security/server_credentials.h>
#include <grpcpp/server.h>
#include <grpcpp/server_builder.h>
#include <grpcpp/server_context.h>
#include <stdexcept>

#include "server.h"

const char *PATH = "databases";

int main(int argc,char **argv) {
    LevelDbService service(PATH);

    std::string server_address("0.0.0.0:8501");

    grpc::ServerBuilder builder;
    int tcp_port;
    builder.AddListeningPort(server_address, grpc::InsecureServerCredentials(), &tcp_port);
    builder.AddChannelArgument(GRPC_ARG_ALLOW_REUSEPORT, 0);
    builder.RegisterService(&service);

    std::unique_ptr<grpc::Server> server(builder.BuildAndStart());
    if(server.get()==nullptr) throw std::runtime_error("unable to start server ");
    server->Wait();
}