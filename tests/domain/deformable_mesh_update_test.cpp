#include <gtest/gtest.h>
#include "domain/shapes/deformable_mesh.h"
#include "domain/hit_record.h"
#include "core/ray.h"
#include "core/vec3.h"
#include "core/aabb.h"
#include <cmath>
#include <vector>

using namespace nwave;

// Test Budget: 5 behaviors x 2 = 10 max unit tests (using 5)
//
// Behaviors tested:
// 1. After update_vertices(), hit() intersects new positions (not old)
// 2. Flat quad deformed to V-shape changes intersection point and normal
// 3. AABB is tight around new vertex positions after update
// 4. Normals point outward (dot with face winding direction > 0) after update
// 5. Smooth normals are area-weighted averages within tolerance

// Helper: 2-triangle flat quad in XY plane
//   v2(0,1,0) --- v3(1,1,0)
//   |           /  |
//   |  face0  / face1 |
//   |       /      |
//   v0(0,0,0) --- v1(1,0,0)
static std::vector<Point3> flat_quad_vertices() {
    return {
        Point3(0, 0, 0),  // v0
        Point3(1, 0, 0),  // v1
        Point3(0, 1, 0),  // v2
        Point3(1, 1, 0),  // v3
    };
}

static std::vector<int> quad_face_indices() {
    return {
        0, 1, 2,  // face 0: v0, v1, v2
        1, 3, 2,  // face 1: v1, v3, v2
    };
}

// V-shape: fold the quad along the y-axis, pushing v0 and v1 down in z
//   v2(0,1,0) --- v3(1,1,0)    (stay at z=0)
//   |           /  |
//   v0(0,0,-1) --- v1(1,0,-1)  (move to z=-1)
static std::vector<Point3> v_shape_quad_vertices() {
    return {
        Point3(0, 0, -1),  // v0 moved down
        Point3(1, 0, -1),  // v1 moved down
        Point3(0, 1, 0),   // v2 stays
        Point3(1, 1, 0),   // v3 stays
    };
}

// --- Behavior 1: After update_vertices(), hit() intersects new positions, not old ---

TEST(DeformableMeshUpdateTest, UpdateVerticesIntersectsNewPositionsNotOld) {
    auto face_indices = quad_face_indices();
    DeformableMesh mesh(std::move(face_indices), nullptr);

    // Initially place the quad at z=0
    mesh.update_vertices(flat_quad_vertices());

    // Verify ray hits at z=0
    Ray ray_down(Point3(0.5, 0.5, 5.0), Vec3(0, 0, -1));
    HitRecord rec;
    ASSERT_TRUE(mesh.hit(ray_down, 0.001, infinity, rec));
    EXPECT_NEAR(rec.point.z(), 0.0, 1e-6);

    // Now move the entire quad to z=3
    std::vector<Point3> moved = {
        Point3(0, 0, 3), Point3(1, 0, 3),
        Point3(0, 1, 3), Point3(1, 1, 3),
    };
    mesh.update_vertices(std::move(moved));

    // Ray from z=5 should now hit at z=3
    HitRecord rec2;
    ASSERT_TRUE(mesh.hit(ray_down, 0.001, infinity, rec2));
    EXPECT_NEAR(rec2.point.z(), 3.0, 1e-6);

    // Ray from z=-1 aimed upward should NOT hit (quad is at z=3, ray goes up from below)
    Ray ray_up(Point3(0.5, 0.5, -1.0), Vec3(0, 0, 1));
    HitRecord rec3;
    bool hit = mesh.hit(ray_up, 0.001, 2.0, rec3);
    // t_max=2.0 means the ray only reaches z=1.0, which is below z=3
    EXPECT_FALSE(hit);
}

// --- Behavior 2: Flat quad to V-shape changes intersection point and normal ---

TEST(DeformableMeshUpdateTest, FlatQuadToVShapeChangesNormal) {
    auto face_indices = quad_face_indices();
    DeformableMesh mesh(std::move(face_indices), nullptr);

    // Start with flat quad at z=0
    mesh.update_vertices(flat_quad_vertices());

    // Hit the center of the flat quad, normal should be (0,0,1)
    Ray ray(Point3(0.25, 0.25, 2.0), Vec3(0, 0, -1));
    HitRecord rec_flat;
    ASSERT_TRUE(mesh.hit(ray, 0.001, infinity, rec_flat));
    Vec3 flat_normal = rec_flat.normal;

    // Flat quad normal should be purely in z direction
    EXPECT_NEAR(std::fabs(flat_normal.z()), 1.0, 1e-6);
    EXPECT_NEAR(flat_normal.x(), 0.0, 1e-6);
    EXPECT_NEAR(flat_normal.y(), 0.0, 1e-6);

    // Now deform to V-shape
    mesh.update_vertices(v_shape_quad_vertices());

    // Ray aimed at a point on the lower face (face 0: v0, v1, v2)
    // The face is now tilted, so the intersection point and normal should change
    Ray ray2(Point3(0.25, 0.25, 2.0), Vec3(0, 0, -1));
    HitRecord rec_v;
    ASSERT_TRUE(mesh.hit(ray2, 0.001, infinity, rec_v));
    Vec3 v_normal = rec_v.normal;

    // The V-shape tilts faces, so normal should no longer be purely z
    // At minimum, the normal should have changed from the flat case
    double normal_diff = (v_normal - flat_normal).length();
    EXPECT_GT(normal_diff, 0.01) << "Normal should change after deforming flat quad to V-shape";

    // The intersection point z should also differ from 0.0 (the face is now tilted)
    // For face 0 with the V-shape, z varies between -1 and 0
    EXPECT_LT(rec_v.point.z(), 0.0)
        << "Intersection on V-shape lower face should be below z=0";
}

// --- Behavior 3: AABB tight around new vertex positions after update ---

TEST(DeformableMeshUpdateTest, AABBTightAfterUpdate) {
    auto face_indices = quad_face_indices();
    DeformableMesh mesh(std::move(face_indices), nullptr);

    // Start with flat quad
    mesh.update_vertices(flat_quad_vertices());
    AABB box1 = mesh.bounding_box();
    EXPECT_NEAR(box1.minimum().z(), 0.0, 1e-10);
    EXPECT_NEAR(box1.maximum().z(), 0.0, 1e-10);

    // Update to V-shape: z range should be [-1, 0]
    mesh.update_vertices(v_shape_quad_vertices());
    AABB box2 = mesh.bounding_box();

    EXPECT_NEAR(box2.minimum().x(), 0.0, 1e-10);
    EXPECT_NEAR(box2.maximum().x(), 1.0, 1e-10);
    EXPECT_NEAR(box2.minimum().y(), 0.0, 1e-10);
    EXPECT_NEAR(box2.maximum().y(), 1.0, 1e-10);
    EXPECT_NEAR(box2.minimum().z(), -1.0, 1e-10);
    EXPECT_NEAR(box2.maximum().z(), 0.0, 1e-10);

    // Update to a completely different configuration
    std::vector<Point3> scattered = {
        Point3(-5, -3, 2), Point3(7, 0, -4),
        Point3(0, 8, 1),   Point3(3, -1, 6),
    };
    mesh.update_vertices(std::move(scattered));
    AABB box3 = mesh.bounding_box();

    EXPECT_NEAR(box3.minimum().x(), -5.0, 1e-10);
    EXPECT_NEAR(box3.maximum().x(), 7.0, 1e-10);
    EXPECT_NEAR(box3.minimum().y(), -3.0, 1e-10);
    EXPECT_NEAR(box3.maximum().y(), 8.0, 1e-10);
    EXPECT_NEAR(box3.minimum().z(), -4.0, 1e-10);
    EXPECT_NEAR(box3.maximum().z(), 6.0, 1e-10);
}

// --- Behavior 4: Normals point outward after update ---

TEST(DeformableMeshUpdateTest, NormalsPointOutwardAfterUpdate) {
    // Single triangle in XY plane: v0(0,0,0), v1(1,0,0), v2(0,1,0)
    // Winding: CCW when viewed from +z, so outward normal should be +z direction
    std::vector<int> face_indices = {0, 1, 2};
    DeformableMesh mesh(std::move(face_indices), nullptr);

    // Place triangle at z=0
    std::vector<Point3> verts = {
        Point3(0, 0, 0), Point3(1, 0, 0), Point3(0, 1, 0)
    };
    mesh.update_vertices(std::move(verts));

    // Ray coming from +z direction (front face)
    Ray ray(Point3(0.2, 0.2, 1.0), Vec3(0, 0, -1));
    HitRecord rec;
    ASSERT_TRUE(mesh.hit(ray, 0.001, infinity, rec));

    // The face normal from winding is cross(v1-v0, v2-v0) = cross((1,0,0),(0,1,0)) = (0,0,1)
    // So dot(normal, face_winding_direction) should be > 0
    Vec3 face_winding = cross(
        Point3(1, 0, 0) - Point3(0, 0, 0),
        Point3(0, 1, 0) - Point3(0, 0, 0)
    );
    Vec3 face_winding_dir = normalize(face_winding);

    // The rec.normal should agree with face winding for front face hit
    EXPECT_TRUE(rec.front_face);
    double agreement = dot(rec.normal, face_winding_dir);
    EXPECT_GT(agreement, 0.0) << "Normal should point in same half-space as face winding";

    // Now update: move triangle to z=5, same winding
    std::vector<Point3> moved = {
        Point3(0, 0, 5), Point3(1, 0, 5), Point3(0, 1, 5)
    };
    mesh.update_vertices(std::move(moved));

    Ray ray2(Point3(0.2, 0.2, 7.0), Vec3(0, 0, -1));
    HitRecord rec2;
    ASSERT_TRUE(mesh.hit(ray2, 0.001, infinity, rec2));

    EXPECT_TRUE(rec2.front_face);
    double agreement2 = dot(rec2.normal, face_winding_dir);
    EXPECT_GT(agreement2, 0.0) << "Normal should still point outward after vertex update";
}

// --- Behavior 5: Area-weighted normals correct within tolerance ---

TEST(DeformableMeshUpdateTest, AreaWeightedNormalsCorrectWithinTolerance) {
    // Two triangles sharing edge v0-v1, meeting at a right angle:
    //
    // Face 0 (in XY plane): v0(0,0,0), v1(1,0,0), v2(0,1,0)
    //   area = 0.5, face normal = (0,0,1)
    //
    // Face 1 (in XZ plane): v0(0,0,0), v3(0,0,1), v1(1,0,0)
    //   edge1 = v3-v0 = (0,0,1), edge2 = v1-v0 = (1,0,0)
    //   face_normal = cross((0,0,1),(1,0,0)) = (0,1,0) -- wait, let me recalculate
    //   Actually: cross((0,0,1),(1,0,0)) = (0*0 - 1*0, 1*1 - 0*0, 0*0 - 0*1) = (0,1,0)
    //   area = 0.5, face normal direction = (0,1,0)
    //
    // The un-normalized cross products are:
    //   Face 0: cross((1,0,0),(0,1,0)) = (0,0,1) -- magnitude 1 (area = 0.5)
    //   Face 1: cross((0,0,1),(1,0,0)) = (0,1,0) -- magnitude 1 (area = 0.5)
    //
    // Both faces have equal area (0.5), so:
    //   Vertex v0 (shared by both faces): accumulated = (0,0,1) + (0,1,0) = (0,1,1)
    //     normalized = (0, 1/sqrt(2), 1/sqrt(2))
    //   Vertex v1 (shared by both faces): accumulated = (0,0,1) + (0,1,0) = (0,1,1)
    //     normalized = (0, 1/sqrt(2), 1/sqrt(2))
    //   Vertex v2 (only face 0): accumulated = (0,0,1), normalized = (0,0,1)
    //   Vertex v3 (only face 1): accumulated = (0,1,0), normalized = (0,1,0)

    std::vector<int> face_indices = {
        0, 1, 2,  // face 0 in XY plane
        0, 3, 1,  // face 1 in XZ plane
    };
    DeformableMesh mesh(std::move(face_indices), nullptr);

    std::vector<Point3> verts = {
        Point3(0, 0, 0),  // v0
        Point3(1, 0, 0),  // v1
        Point3(0, 1, 0),  // v2
        Point3(0, 0, 1),  // v3
    };
    mesh.update_vertices(std::move(verts));

    // Hit face 0 near v0 to get the interpolated smooth normal
    // At v0, the smooth normal should be (0, 1/sqrt(2), 1/sqrt(2))
    // We shoot a ray that hits very close to v0 on face 0
    // Using Moller-Trumbore: for face 0 (v0, v1, v2), point near v0 means u~0, v~0
    // The hit point is at w*v0 + u*v1 + v*v2 where w=1-u-v
    // Near v0: w ~ 1, u ~ 0, v ~ 0
    // Interpolated normal = w*n0 + u*n1 + v*n2 ~ n0
    Ray ray_near_v0(Point3(0.01, 0.01, 1.0), Vec3(0, 0, -1));
    HitRecord rec;
    ASSERT_TRUE(mesh.hit(ray_near_v0, 0.001, infinity, rec));

    // Expected normal at v0: (0, 1/sqrt(2), 1/sqrt(2))
    double inv_sqrt2 = 1.0 / std::sqrt(2.0);
    Vec3 expected_n0 = Vec3(0.0, inv_sqrt2, inv_sqrt2);

    // The interpolated normal near v0 should be close to expected_n0
    // Since we're not exactly at v0 (u and v are small but nonzero),
    // use a slightly relaxed tolerance
    EXPECT_NEAR(rec.normal.x(), expected_n0.x(), 0.05);
    EXPECT_NEAR(rec.normal.y(), expected_n0.y(), 0.05);
    EXPECT_NEAR(rec.normal.z(), expected_n0.z(), 0.05);

    // Hit face 0 near v2 to verify single-face vertex normal
    // At v2 (only in face 0), smooth normal should be (0, 0, 1)
    // Near v2: u~0, v~1 in Moller-Trumbore parametrization
    Ray ray_near_v2(Point3(0.01, 0.98, 1.0), Vec3(0, 0, -1));
    HitRecord rec2;
    ASSERT_TRUE(mesh.hit(ray_near_v2, 0.001, infinity, rec2));

    EXPECT_NEAR(rec2.normal.x(), 0.0, 0.05);
    EXPECT_NEAR(rec2.normal.y(), 0.0, 0.05);
    EXPECT_NEAR(rec2.normal.z(), 1.0, 0.05);

    // Verify normals are unit length
    EXPECT_NEAR(rec.normal.length(), 1.0, 1e-6);
    EXPECT_NEAR(rec2.normal.length(), 1.0, 1e-6);
}
