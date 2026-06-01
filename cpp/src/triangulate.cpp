#include <iostream>
#include <list>
#include <vector>
#include <map>
#include <cmath>

#include "triangulate.h"
#include "json.hpp"

using json = nlohmann::json;

#include <CGAL/Exact_predicates_inexact_constructions_kernel.h>
#include <CGAL/linear_least_squares_fitting_3.h>
#include <CGAL/Constrained_Delaunay_triangulation_2.h>
#include <CGAL/Triangulation_vertex_base_2.h>
#include <CGAL/Triangulation_face_base_with_info_2.h>
#include <CGAL/Constrained_triangulation_face_base_2.h>
#include <CGAL/Triangulation_data_structure_2.h>

typedef CGAL::Exact_predicates_inexact_constructions_kernel Kernel;

typedef Kernel::Point_3 Point_3;
typedef Kernel::Point_2 Point_2;
typedef Kernel::Plane_3 Plane_3;

struct FaceInfo {
  int nesting_level;

  FaceInfo() : nesting_level(-1) {}

  bool in_domain() const {
    return nesting_level % 2 == 1;
  }
};

typedef CGAL::Triangulation_vertex_base_2<Kernel> Vb;
typedef CGAL::Triangulation_face_base_with_info_2<FaceInfo, Kernel> Fbb;
typedef CGAL::Constrained_triangulation_face_base_2<Kernel, Fbb> Fb;
typedef CGAL::Triangulation_data_structure_2<Vb, Fb> TDS;
typedef CGAL::Exact_predicates_tag Itag;
typedef CGAL::Constrained_Delaunay_triangulation_2<Kernel, TDS, Itag> CDT;

void mark_domains(CDT& cdt) {
  for (auto face = cdt.all_faces_begin(); face != cdt.all_faces_end(); ++face) {
    face->info().nesting_level = -1;
  }

  std::list<CDT::Face_handle> queue;

  CDT::Face_handle infinite_face = cdt.infinite_face();
  infinite_face->info().nesting_level = 0;
  queue.push_back(infinite_face);

  while (!queue.empty()) {
    CDT::Face_handle face = queue.front();
    queue.pop_front();

    for (int i = 0; i < 3; ++i) {
      CDT::Face_handle neighbor = face->neighbor(i);

      if (neighbor->info().nesting_level != -1) {
        continue;
      }

      if (cdt.is_constrained(std::make_pair(face, i))) {
        neighbor->info().nesting_level = face->info().nesting_level + 1;
      } else {
        neighbor->info().nesting_level = face->info().nesting_level;
      }

      queue.push_back(neighbor);
    }
  }
}

double signed_area_2d(const std::vector<Point_2>& ring) {
  double area = 0.0;

  for (size_t i = 0; i < ring.size(); i++) {
    const Point_2& p1 = ring[i];
    const Point_2& p2 = ring[(i + 1) % ring.size()];

    area += p1.x() * p2.y() - p2.x() * p1.y();
  }

  return 0.5 * area;
}

double signed_area_triangle_2d(const Point_2& a, const Point_2& b, const Point_2& c) {
  return 0.5 * (
    a.x() * (b.y() - c.y()) +
    b.x() * (c.y() - a.y()) +
    c.x() * (a.y() - b.y())
  );
}

void triangulate_surfaces(json& j) {
  // Triangulates all LoD2.2 Solid surfaces and preserves semantic values while preserving ring orientation.
  int original_surfaces = 0;
  int created_triangles = 0;
  int kept_original_surfaces = 0;
  for (auto& item: j["CityObjects"].items()) {
    json& co = item.value();
    if (co.contains("type") && co["type"] == "Building") {
      for (auto& geom: co["geometry"]) {
        if (geom["type"] == "Solid" && geom.contains("lod") && geom["lod"] == "2.2") { // introduced The lod field check
          // shell → surface → ring → vertex indices structure
          for (size_t shell_id = 0; shell_id < geom["boundaries"].size(); shell_id++) {
            auto& shell = geom["boundaries"][shell_id];
            json new_shell = json::array();
            json new_semantic_values_for_shell = json::array();

            for (size_t surface_id = 0; surface_id < shell.size(); surface_id++) {
              auto& surface = shell[surface_id];
              std::string building_id = item.key();
              original_surfaces++;

              int sem_value = geom["semantics"]["values"][shell_id][surface_id].get<int>();

              std::vector<std::vector<int>> surface_indices;
              std::vector<std::vector<Point_3>> surface_points_3d;


              for (auto& ring : surface) {
                std::vector<int> ring_indices;
                std::vector<Point_3> ring_points_3d;

                for (auto& vertex_index : ring) {
                  int v_index = vertex_index.get<int>();
                  std::vector<int> vi = j["vertices"][v_index];
                  double x = (vi[0] * j["transform"]["scale"][0].get<double>()) + j["transform"]["translate"][0].get<double>();
                  double y = (vi[1] * j["transform"]["scale"][1].get<double>()) + j["transform"]["translate"][1].get<double>();
                  double z = (vi[2] * j["transform"]["scale"][2].get<double>()) + j["transform"]["translate"][2].get<double>();
                  ring_indices.push_back(v_index);
                  ring_points_3d.push_back(Point_3(x,y,z));
                }

                // if to avoid pushing empty ring.
                if (!ring_indices.empty()) {
                  surface_indices.push_back(ring_indices);
                  surface_points_3d.push_back(ring_points_3d);
                }
              }
              // Checking if surface has a valid outer ring. Can remove later when debugging finishes
              if (surface_points_3d.empty() || surface_points_3d[0].size() < 3) {
                std::cerr << "Warning: surface has no valid outer ring. Keeping original surface." << std::endl;

                new_shell.push_back(surface);
                new_semantic_values_for_shell.push_back(sem_value);
                kept_original_surfaces++;

                continue;
              }

              std::vector<Point_3> all_surface_points;

              for (const auto& ring_points : surface_points_3d) {
                for (const auto& p : ring_points) {
                  all_surface_points.push_back(p);
                }
              }

              if (all_surface_points.size() < 3) {
                std::cerr << "Warning: surface has fewer than 3 total points. Keeping original surface." << std::endl;

                new_shell.push_back(surface);
                new_semantic_values_for_shell.push_back(sem_value);
                kept_original_surfaces++;

                continue;
              }

              Plane_3 best_plane;
              CGAL::linear_least_squares_fitting_3(
                all_surface_points.begin(), all_surface_points.end(), best_plane, CGAL::Dimension_tag<0>());
              std::vector<std::vector<Point_2>> surface_points_2d;
              std::map<Point_2, int> point_to_index;

              for (size_t ring_id = 0; ring_id < surface_points_3d.size(); ring_id++) {
                std::vector<Point_2> ring_points_2d;

                for (size_t point_id = 0; point_id < surface_points_3d[ring_id].size(); point_id++) {
                  Point_3 p3 = surface_points_3d[ring_id][point_id];
                  Point_2 p2 = best_plane.to_2d(p3);
                  ring_points_2d.push_back(p2);

                  int original_index = surface_indices[ring_id][point_id];
                  point_to_index[p2] = original_index;
                }

                surface_points_2d.push_back(ring_points_2d);
              }
              std::vector<std::vector<Point_2>> cleaned_surface_points_2d;

              for (const auto& ring_points : surface_points_2d) {
                std::vector<Point_2> cleaned_ring;

                for (const auto& p : ring_points) {
                  if (cleaned_ring.empty() || p != cleaned_ring.back()) {
                    cleaned_ring.push_back(p);
                  }
                }

                // Removing repeated final point if the ring is explicitly closed
                if (cleaned_ring.size() > 1 && cleaned_ring.front() == cleaned_ring.back()) {
                  cleaned_ring.pop_back();
                }

                if (cleaned_ring.size() >= 3) {
                  cleaned_surface_points_2d.push_back(cleaned_ring);
                } else {
                  // std::cerr << "Warning: ring became invalid after cleaning." << std::endl;
                }
              }

              if (cleaned_surface_points_2d.empty() || cleaned_surface_points_2d[0].size() < 3) {
                if (surface.size() == 1 && surface[0].size() == 4) {
                  json tri1_ring = json::array();
                  tri1_ring.push_back(surface[0][0]);
                  tri1_ring.push_back(surface[0][1]);
                  tri1_ring.push_back(surface[0][2]);

                  json tri1_surface = json::array();
                  tri1_surface.push_back(tri1_ring);

                  json tri2_ring = json::array();
                  tri2_ring.push_back(surface[0][0]);
                  tri2_ring.push_back(surface[0][2]);
                  tri2_ring.push_back(surface[0][3]);

                  json tri2_surface = json::array();
                  tri2_surface.push_back(tri2_ring);

                  new_shell.push_back(tri1_surface);
                  new_semantic_values_for_shell.push_back(sem_value);

                  new_shell.push_back(tri2_surface);
                  new_semantic_values_for_shell.push_back(sem_value);

                  created_triangles += 2;

                  // std::cerr << "Fallback triangulated quad surface." << std::endl;
                  continue;
                }

                // std::cerr << "Warning: surface invalid after ring cleaning. Keeping original surface." << std::endl;

                new_shell.push_back(surface);
                new_semantic_values_for_shell.push_back(sem_value);
                kept_original_surfaces++;
                continue;
              }

              double original_area = signed_area_2d(cleaned_surface_points_2d[0]);

              if (std::abs(original_area) < 1e-12) {
                std::cerr << "Warning: original surface has near-zero area. Keeping original surface." << std::endl;

                new_shell.push_back(surface);
                new_semantic_values_for_shell.push_back(sem_value);
                kept_original_surfaces++;

                continue;
              }

              //Adding the 2d ring points as constraints to the triangulation.
              CDT constrained_dt;
              for (const auto& ring_points : cleaned_surface_points_2d) {
                if (ring_points.size() < 3) {
                  continue;
                }

                for (size_t i = 0; i < ring_points.size(); i++) {
                  const Point_2& p1 = ring_points[i];
                  const Point_2& p2 = ring_points[(i + 1) % ring_points.size()];

                  // To avoid adding zero length edge as constraint
                  if (p1 == p2) {
                    std::cerr << "Warning: zero-length constraint skipped." << std::endl;
                    continue;
                  }

                  constrained_dt.insert_constraint(p1, p2);
                }
              }

              // Checking whether the triangulation is usable before marking domains
              if (constrained_dt.number_of_vertices() < 3 || constrained_dt.number_of_faces() == 0) {
                std::cerr << "Warning: invalid CDT. Keeping original surface." << std::endl;

                new_shell.push_back(surface);
                new_semantic_values_for_shell.push_back(sem_value);
                kept_original_surfaces++;

                continue;
              }
              // Manually marking inside/outside faces.
              mark_domains(constrained_dt);

              // Extracting only interior triangles.
              for (auto face = constrained_dt.finite_faces_begin();
                   face != constrained_dt.finite_faces_end();
                   ++face) {

                if (!face->info().in_domain()) {
                  continue;
                }

                Point_2 a = face->vertex(0)->point();
                Point_2 b = face->vertex(1)->point();
                Point_2 c = face->vertex(2)->point();

                // edit started

                // Helper lambda: find the closest original vertex index for a CDT point.
                // Handles Steiner points (CDT-inserted vertices not in the original ring)
                // by snapping to the nearest original vertex by 3D distance.
                auto find_closest_index = [&](const Point_2& p2) -> int {
                    Point_3 p3 = best_plane.to_3d(p2);
                    int best_idx = -1;
                    double best_dist_sq = std::numeric_limits<double>::max();
                    for (size_t ring_id = 0; ring_id < surface_indices.size(); ring_id++) {
                        for (size_t pt_id = 0; pt_id < surface_indices[ring_id].size(); pt_id++) {
                            const Point_3& candidate = surface_points_3d[ring_id][pt_id];
                            double dx = p3.x() - candidate.x();
                            double dy = p3.y() - candidate.y();
                            double dz = p3.z() - candidate.z();
                            double dist_sq = dx*dx + dy*dy + dz*dz;
                            if (dist_sq < best_dist_sq) {
                                best_dist_sq = dist_sq;
                                best_idx = surface_indices[ring_id][pt_id];
                            }
                        }
                    }
                    return best_idx;
                };

                int ia = find_closest_index(a);
                int ib = find_closest_index(b);
                int ic = find_closest_index(c);

                if (ia < 0 || ib < 0 || ic < 0) {
                    std::cerr << "Warning: could not map triangle vertex to original index. Skipping triangle." << std::endl;
                    continue;
                }

                // edit ended

                double triangle_area = signed_area_triangle_2d(a, b, c);

                if (std::abs(triangle_area) < 1e-12) {
                  std::cerr << "Warning: degenerate triangle skipped." << std::endl;
                  continue;
                }

                if ((original_area > 0 && triangle_area < 0) || (original_area < 0 && triangle_area > 0)) {
                  std::swap(ib, ic);
                }

                json triangle_ring = json::array();
                triangle_ring.push_back(ia);
                triangle_ring.push_back(ib);
                triangle_ring.push_back(ic);

                json triangle_surface = json::array();
                triangle_surface.push_back(triangle_ring);

                new_shell.push_back(triangle_surface);
                new_semantic_values_for_shell.push_back(sem_value);
                created_triangles++;
              }
            }
            geom["boundaries"][shell_id] = new_shell;
            geom["semantics"]["values"][shell_id] = new_semantic_values_for_shell;
          }
        }
      }
    }
  }
std::cout << "Original surfaces: " << original_surfaces << std::endl;
std::cout << "Created triangle surfaces: " << created_triangles << std::endl;
std::cout << "Kept original surfaces: " << kept_original_surfaces << std::endl;
// std::cout << "=== Successfully Triangulated ===" << std::endl;
}