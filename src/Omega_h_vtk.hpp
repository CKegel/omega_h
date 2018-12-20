#ifndef OMEGA_H_VTK_HPP
#define OMEGA_H_VTK_HPP

#include <iosfwd>
#include <string>
#include <vector>

#include <Omega_h_comm.hpp>
#include <Omega_h_defines.hpp>
#include <Omega_h_file.hpp>

namespace Omega_h {

class Mesh;

namespace vtk {

std::string get_pvtu_path(std::string const& step_path);

std::string get_pvd_path(std::string const& root_path);

void write_pvtu(std::ostream& stream, Mesh* mesh, Int cell_dim,
    std::string const& piecepath, TagSet const& tags);

void write_pvtu(std::string const& filename, Mesh* mesh, Int cell_dim,
    std::string const& piecepath, TagSet const& tags);

std::streampos write_initial_pvd(
    std::string const& root_path, Real restart_time);
void update_pvd(std::string const& root_path, std::streampos* pos_inout,
    I64 step, Real time);

void read_vtu(std::istream& stream, CommPtr comm, Mesh* mesh);
void read_vtu_ents(std::istream& stream, Mesh* mesh);

void read_pvtu(std::istream& stream, CommPtr comm, I32* npieces_out,
    std::string* vtupath_out, Int* nghost_layers_out);

void read_pvtu(std::string const& pvtupath, CommPtr comm, I32* npieces_out,
    std::string* vtupath_out, Int* nghost_layers_out);

void read_pvd(std::istream& stream, std::vector<Real>* times_out,
    std::vector<std::string>* pvtupaths_out);

void read_pvd(std::string const& pvdpath, std::vector<Real>* times_out,
    std::vector<std::string>* pvtupaths_out);

template <bool is_signed, std::size_t size>
struct IntTraits;

template <std::size_t size>
struct FloatTraits;

void write_p_tag(std::ostream& stream, TagBase const* tag, Int space_dim);

template <typename T>
void write_p_data_array(std::ostream& stream, std::string const& name,
    Int ncomps);

template <typename T_osh, typename T_vtk = T_osh>
void write_array(std::ostream& stream, std::string const& name,
    Int ncomps, Read<T_osh> array, bool compress);

#define OMEGA_H_EXPL_INST_DECL(T) \
extern template void write_p_data_array<T>(std::ostream& stream, \
    std::string const& name, Int ncomps); \
extern template void write_array(std::ostream& stream, \
    std::string const& name, Int ncomps, Read<T> array, bool compress);
OMEGA_H_EXPL_INST_DECL(I8)
OMEGA_H_EXPL_INST_DECL(I32)
OMEGA_H_EXPL_INST_DECL(I64)
OMEGA_H_EXPL_INST_DECL(Real)
#undef OMEGA_H_EXPL_INST_DECL

extern template void write_array<Real, std::uint8_t>(std::ostream& stream,
    std::string const& name, Int ncomps, Read<Real> array, bool compress);

}  // namespace vtk

}  // end namespace Omega_h

#endif
