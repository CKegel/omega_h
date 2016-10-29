#include <iostream>
#include <Omega_h.hpp>

int main(int argc, char** argv) {
  auto lib = Omega_h::Library(&argc, &argv);
  if (argc != 3) {
    std::cout << "usage: " << argv[0] << " input.osh output.mesh[b]\n";
    return -1;
  }
  Omega_h::Mesh mesh(&lib);
  Omega_h::binary::read(argv[1], lib.world(), &mesh);
  Omega_h::meshb::write(&mesh, argv[2], 2);
  return 0;
}

