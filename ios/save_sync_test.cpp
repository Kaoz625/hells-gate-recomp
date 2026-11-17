// Host test for src/save_sync.cpp (macOS/Linux): clang++ -std=c++17 -O1 -Isrc ios/save_sync_test.cpp src/save_sync.cpp
//   ./a.out <root> [game] [device]   -> SHA-256 vectors, then a real launch sync + one in-game save against the
//   savesync store (default: the TEST game dante-inferno-synctest, never the live save). Log: <root>/../savesync.log
#include "save_sync.h"
#include <cassert>
#include <chrono>
#include <cstdio>
#include <fstream>
#include <thread>
int main(int argc, char** argv) {
  assert(save_sync::Sha256Hex("") == "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");
  assert(save_sync::Sha256Hex("abc") == "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
  assert(save_sync::Sha256Hex(std::string(1000000, 'a')) ==
         "cdc76e5c9914fb9281a1c7e284d73e67f1809a48a497200e046d39ccc7112cd0");
  if (argc < 2) { puts("sha256 ok"); return 0; }
  save_sync::Config c;
  c.root = argv[1];
  c.game = argc > 2 ? argv[2] : "dante-inferno-synctest";
  if (argc > 3) c.device = argv[3];
  c.poll_seconds = 1;
  save_sync::Start(c);
  std::ofstream(c.root / "achievements" / "454108CF.toml", std::ios::app) << "# host test edit\n";
  std::this_thread::sleep_for(std::chrono::seconds(5));
  puts("done - see savesync.log");
}
