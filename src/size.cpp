#include "size.hpp"

#include "graph.hpp"
#include "loop.hpp"

namespace osh {

template <typename EdgeLengths>
static Reals measure_edges_tmpl(Mesh* mesh, LOs a2e) {
  EdgeLengths measurer(mesh);
  auto ev2v = mesh->ask_verts_of(EDGE);
  auto na = a2e.size();
  Write<Real> lengths(na);
  auto f = LAMBDA(LO a) {
    auto e = a2e[a];
    auto v = gather_verts<2>(ev2v, e);
    lengths[a] = measurer.measure(v);
  };
  parallel_for(na, f);
  return lengths;
}

Reals measure_edges_real(Mesh* mesh, LOs a2e) {
  if (mesh->dim() == 3) {
    return measure_edges_tmpl<RealEdgeLengths<3>>(mesh, a2e);
  } else {
    CHECK(mesh->dim() == 2);
    return measure_edges_tmpl<RealEdgeLengths<2>>(mesh, a2e);
  }
}

Reals measure_edges_metric(Mesh* mesh, LOs a2e) {
  if (mesh->dim() == 3) {
    if (mesh->has_tag(VERT, "size")) {
      return measure_edges_tmpl<IsoEdgeLengths<3>>(mesh, a2e);
    }
    if (mesh->has_tag(VERT, "metric")) {
      return measure_edges_tmpl<MetricEdgeLengths<3>>(mesh, a2e);
    }
  } else {
    CHECK(mesh->dim() == 2);
    if (mesh->has_tag(VERT, "size")) {
      return measure_edges_tmpl<IsoEdgeLengths<2>>(mesh, a2e);
    }
    if (mesh->has_tag(VERT, "metric")) {
      return measure_edges_tmpl<MetricEdgeLengths<2>>(mesh, a2e);
    }
  }
  osh_fail("measure_edges(): no size field exists!\n");
  NORETURN(Reals());
}

Reals measure_edges_real(Mesh* mesh) {
  return measure_edges_real(mesh, LOs(mesh->nedges(), 0, 1));
}

Reals measure_edges_metric(Mesh* mesh) {
  return measure_edges_metric(mesh, LOs(mesh->nedges(), 0, 1));
}

Reals find_identity_size(Mesh* mesh) {
  CHECK(mesh->owners_have_all_upward(VERT));
  auto lens = measure_edges_real(mesh);
  auto v2e = mesh->ask_up(VERT, EDGE);
  auto nve = v2e.a2ab.last();
  auto weights = Reals(nve, 1.0);
  auto own_isos = graph_weighted_average(v2e, weights, lens, 1);
  auto synced_isos = mesh->sync_array(VERT, own_isos, 1);
  return synced_isos;
}

template <Int dim>
static Reals measure_elements_real_tmpl(Mesh* mesh) {
  RealElementSizes measurer(mesh);
  auto ev2v = mesh->ask_verts_of(dim);
  auto ne = mesh->nelems();
  Write<Real> sizes(ne);
  auto f = LAMBDA(LO e) {
    auto v = gather_verts<dim + 1>(ev2v, e);
    sizes[e] = measurer.measure(v);
  };
  parallel_for(ne, f);
  return sizes;
}

Reals measure_elements_real(Mesh* mesh) {
  if (mesh->dim() == 3) {
    return measure_elements_real_tmpl<3>(mesh);
  } else {
    CHECK(mesh->dim() == 2);
    return measure_elements_real_tmpl<2>(mesh);
  }
}

template <Int dim>
static Reals find_identity_metric_tmpl(Mesh* mesh) {
  CHECK(dim == mesh->dim());
  CHECK(mesh->owners_have_all_upward(VERT));
  auto coords = mesh->coords();
  auto ev2v = mesh->ask_verts_of(dim);
  auto elem_metrics_w = Write<Real>(mesh->nelems() * symm_dofs(dim));
  auto f0 = LAMBDA(LO e) {
    auto v = gather_verts<dim + 1>(ev2v, e);
    auto p = gather_vectors<dim + 1, dim>(coords, v);
    auto m = element_identity_metric(p);
    set_symm(elem_metrics_w, e, m);
  };
  parallel_for(mesh->nelems(), f0);
  auto elem_metrics = Reals(elem_metrics_w);
  auto elem_sizes = measure_elements_real(mesh);
  auto v2e = mesh->ask_up(VERT, dim);
  auto v2ve = v2e.a2ab;
  auto ve2e = v2e.ab2b;
  auto vert_metrics_w = Write<Real>(mesh->nverts() * symm_dofs(dim));
  auto f1 = LAMBDA(LO v) {
    Real ess = 0.0;
    auto iems = zero_matrix<dim, dim>();
    for (auto ve = v2ve[v]; ve < v2ve[v + 1]; ++ve) {
      auto e = ve2e[ve];
      auto em = get_symm<dim>(elem_metrics, e);
      auto es = elem_sizes[e];
      auto iem = invert(em);
      iems = iems + (iem * es);
      ess += es;
    }
    auto vm = invert(iems / ess);
    set_symm(vert_metrics_w, v, vm);
  };
  parallel_for(mesh->nverts(), f1);
  return vert_metrics_w;
}

Reals find_identity_metric(Mesh* mesh) {
  if (mesh->dim() == 3) {
    return find_identity_metric_tmpl<3>(mesh);
  } else {
    CHECK(mesh->dim() == 2);
    return find_identity_metric_tmpl<2>(mesh);
  }
}

}  // end namespace osh