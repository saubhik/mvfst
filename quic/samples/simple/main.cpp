#include <glog/logging.h>

#include <fizz/crypto/Utils.h>
#include <folly/init/Init.h>
#include <folly/portability/GFlags.h>

#include <quic/samples/simple/SimpleClient.h>
#include <quic/samples/simple/SimpleServer.h>

DEFINE_string(host, "::1", "Simple server hostname/IP");
DEFINE_int32(port, 6666, "Simple server port");
DEFINE_string(mode, "server", "Mode to run in: 'client' or 'server'");

using namespace quic::samples;

int main(int argc, char* argv[]) {
  gflags::ParseCommandLineFlags(&argc, &argv, false);
  folly::Init init(&argc, &argv);
  fizz::CryptoUtils::init();

  if (FLAGS_mode == "server") {
    SimpleServer server(FLAGS_host, FLAGS_port);
    server.start();
  } else if (FLAGS_mode == "client") {
    if (FLAGS_host.empty() || FLAGS_port == 0) {
      LOG(ERROR) << "SimpleClient expected --host and --port";
      return -2;
    }

    SimpleClient client(FLAGS_host, FLAGS_port);
    client.start();
  } else {
    LOG(ERROR) << "Unknown mode specified: " << FLAGS_mode;
    return -1;
  }
  return 0;
}
