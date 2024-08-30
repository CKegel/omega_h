#include <Omega_h_mesh.hpp>
#include <Omega_h_for.hpp> //parallel_for
#include <Omega_h_array_ops.hpp> //get_min
#include <Omega_h_int_scan.hpp> //offset_scan

using namespace Omega_h;

#if defined(OMEGA_H_USE_KOKKOS)
#include <Kokkos_NestedSort.hpp> //sort_team

namespace {
[[nodiscard]] Graph adj_segment_sort(Graph& g) {
  using ExecSpace = Kokkos::DefaultExecutionSpace;
  using TeamPol = Kokkos::TeamPolicy<ExecSpace>;
  using TeamMem = typename TeamPol::member_type;
  auto offsets = g.a2ab;
  auto elms_r = g.ab2b.view(); //read only
  Kokkos::View<LO*, ExecSpace> elms("elms", elms_r.size());
  Kokkos::deep_copy(elms, elms_r);
  auto segment_sort = KOKKOS_LAMBDA(const TeamMem& t) {
    auto i = t.league_rank();
    auto patch = Kokkos::subview(elms, Kokkos::make_pair(offsets[i], offsets[i+1]));
    Kokkos::Experimental::sort_team(t, patch);
  };
  Kokkos::parallel_for(TeamPol(g.nnodes(), Kokkos::AUTO()), segment_sort);
  return Graph(offsets,Write<LO>(elms));
}

[[nodiscard]] Graph remove_duplicate_edges(Graph g) {
  auto offsets = g.a2ab;
  auto values = g.ab2b;
  Write<I8> keep(g.nedges());
  auto markDups = OMEGA_H_LAMBDA(LO i) {
    keep[offsets[i]] = 1; //always keep the first edge in the segment
    for(int j=offsets[i]+1; j<offsets[i+1]; j++) {
      keep[j] = (values[j-1] != values[j]);
    }
  };
  parallel_for(g.nnodes(), markDups);
  auto filtered = filter_graph_edges(g,keep);
  return filtered;
}

[[nodiscard]] Read<I8> patchSufficient(Graph patches, Int minPatchSize) {
  const auto num_patches = patches.nnodes();
  auto offsets = patches.a2ab;
  Write<I8> done(num_patches);
  parallel_for(num_patches, OMEGA_H_LAMBDA(LO i) {
    done[i] = ((offsets[i+1]-offsets[i]) >= minPatchSize);
  });
  return read(done);
}

/**
 * \brief expand the patches
 * \param m (in) mesh of simplices
 * \param patches (in) graph of key entities to elements
 * \param adjElms (in) second order element-to-element adjacencies
 *                     used for expansion
 * \return an expanded graph from key entities to elements
*/
//TODO use Omega_h_map and Omega_h_graph functions
[[nodiscard]] Graph expandPatches(Mesh& m, Graph patches, Graph adjElms, Read<I8> patchDone) {
  auto adjElms_offsets = adjElms.a2ab;
  auto adjElms_elms = adjElms.ab2b;
  const auto num_patches = patches.nnodes();
  auto patch_offsets = patches.a2ab;
  auto patch_elms = patches.ab2b;
  Write<LO> degree(num_patches);
  parallel_for(num_patches, OMEGA_H_LAMBDA(LO patch) {
    degree[patch] = patch_offsets[patch+1] - patch_offsets[patch];
    if(!patchDone[patch]) {
      for(int j=patch_offsets[patch]; j<patch_offsets[patch+1]; j++) {
        auto elm = patch_elms[j];
        degree[patch] += adjElms_offsets[elm+1]-adjElms_offsets[elm]; //counts duplicates
      }
    }
  });
  auto patchExpDup_offsets = offset_scan(read(degree));
  Write<LO> patchExpDup_elms(patchExpDup_offsets.last());
  parallel_for(num_patches, OMEGA_H_LAMBDA(LO patch) {
    auto idx = patchExpDup_offsets[patch];
    for(int j=patch_offsets[patch]; j<patch_offsets[patch+1]; j++) {
      patchExpDup_elms[idx++] = patch_elms[j];
      if(!patchDone[patch]) {
        auto elm = patch_elms[j];
        for(int k=adjElms_offsets[elm]; k<adjElms_offsets[elm+1]; k++) {
          patchExpDup_elms[idx++] = adjElms_elms[k];
        }
      }
    }
  });
  Graph patchExpDup(patchExpDup_offsets,patchExpDup_elms);
  auto sorted = adj_segment_sort(patchExpDup);
  auto dedup = remove_duplicate_edges(sorted);
  return dedup;
}
}//end anonymous namespace

[[nodiscard]] Graph Mesh::get_vtx_patches(Int minPatchSize) {
  OMEGA_H_CHECK(minPatchSize > 0);
  auto patches = ask_up(VERT,dim());
  auto patchDone = patchSufficient(patches, minPatchSize);
  if( get_min(patchDone) == 1 )
    return patches;
  auto adjElms = ask_dual();
  //assuming each iteration adds at least one element then
  //the minPatchSize is a conservative upper bound on the
  //iteration count
  for(Int iter = 0; iter < minPatchSize; iter++) {
    patches = expandPatches(*this, patches, adjElms, patchDone);
    patchDone = patchSufficient(patches, minPatchSize);
    if( get_min(patchDone) == 1 ) {
      return patches;
    }
  }
  return Graph();
}

#else
[[nodiscard]] Graph Mesh::get_vtx_patches(Int minPatchSize) {
  const auto message = "get_vtx_patches requires Kokkos.  Please rebuild with Kokkos enabled.\n";
  static_assert(false,message);
  if( library_->self() == 0 ) {
    std::cerr << message;
    exit(EXIT_FAILURE);
  }
  return Graph();
}
#endif
