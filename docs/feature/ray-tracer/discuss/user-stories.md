# Ray Tracer -- User Stories

**Document ID**: US-RAYTRACER-001
**Date**: 2026-02-16
**Status**: Draft -- Pending DoR Validation

---

## Feature 0: Walking Skeleton (Minimal End-to-End Pipeline)

### US-000: Render a Single Sphere on a Plane to PPM

#### Problem (The Pain)
Elena Marchetti is a computer graphics student who wants to see her first ray-traced image. She has read about ray tracing theory but has never produced a rendered image from code. She finds it overwhelming to figure out which pieces of the pipeline need to exist before anything visible appears on screen.

#### Who (The User)
- CG student writing her first renderer in C++
- Working on a laptop (Ubuntu 22.04, g++ 12)
- Motivated by seeing a visible result as fast as possible to validate her understanding

#### Solution (What We Build)
A minimal but complete ray tracing pipeline that takes a hardcoded scene (one red sphere on a gray ground plane, one white point light, one pinhole camera) and writes a PPM image file showing a recognizably shaded sphere.

#### Domain Examples

##### Example 1: Happy Path -- Elena Renders Her First Image
Elena compiles and runs the ray tracer with no arguments. The program creates a file "output.ppm" (400x225 pixels). She opens it in GIMP and sees a red sphere sitting on a gray plane. The sphere is brighter on the upper-left side (toward the light) and darker on the lower-right side. The ground plane fades to a darker gray away from the light.

##### Example 2: Edge Case -- Camera Aimed Away from the Scene
Elena modifies the camera's lookat target to point away from the sphere (e.g., looking in the +z direction while the sphere is at the origin). The rendered image shows only the background color (dark blue gradient or black) with no sphere or plane visible.

##### Example 3: Boundary Case -- Sphere Completely Behind Camera
Elena places the sphere at z = +5 while the camera is at z = -1.5 looking toward z = 0. The sphere is entirely behind the camera's view. The image shows the ground plane stretching to the horizon with no sphere, confirming that objects behind the camera are not rendered.

#### UAT Scenarios (BDD)

##### Scenario: Elena renders a shaded sphere on a plane
```
Given Elena has compiled the ray tracer with default settings
  And the hardcoded scene contains a red sphere (center 0,0,0; radius 0.5; Lambertian albedo 0.7,0.1,0.1)
  And the hardcoded scene contains a gray ground plane (y = -0.5; Lambertian albedo 0.5,0.5,0.5)
  And there is a white point light at position (-2, 3, -1) with intensity 1.0
  And the pinhole camera is at position (0, 0, -1.5) looking at (0, 0, 0) with 90-degree vertical FOV
When Elena runs the ray tracer executable
Then a file "output.ppm" is created in the current directory
  And the file begins with "P3" followed by "400 225" and "255"
  And the pixel at the center of the image (200, 112) is a shade of red (R > 100, G < 50, B < 50)
  And the pixel at the top-left corner (0, 0) is not red (background or plane color)
```

##### Scenario: Valid PPM file structure
```
Given Elena has run the ray tracer and "output.ppm" exists
When Elena opens "output.ppm" in a text editor
Then the first line is "P3"
  And the second line is "400 225"
  And the third line is "255"
  And the remaining lines contain exactly 400 * 225 = 90000 pixel triplets
  And every RGB value is an integer between 0 and 255 inclusive
```

##### Scenario: Shading varies across the sphere surface
```
Given Elena has rendered the default scene
When Elena examines the pixel colors on the sphere surface
Then the pixels on the sphere facing the light source (upper-left quadrant of the sphere) have higher red intensity (R > 150)
  And the pixels on the sphere facing away from the light (lower-right quadrant) have lower red intensity (R < 100)
  And the transition between bright and dark is smooth (no abrupt color jumps between adjacent pixels on the sphere)
```

##### Scenario: Rays missing all objects produce background color
```
Given Elena has rendered the default scene
When Elena examines pixels in the upper portion of the image above the sphere and plane
Then those pixels show a gradient background color (e.g., blue-to-white sky gradient)
  And the color is distinctly different from both the sphere red and the plane gray
```

##### Scenario: Ground plane extends across the lower half of the image
```
Given Elena has rendered the default scene
When Elena examines pixels below the sphere
Then those pixels show gray shading from the ground plane
  And the plane's brightness decreases with distance from the light source
```

#### Acceptance Criteria
- [ ] Running the executable produces a valid PPM file (P3 format, 400x225, all RGB values 0-255)
- [ ] The sphere is visible and recognizably red with smooth shading variation (bright toward light, dark away from light)
- [ ] The ground plane is visible as a gray surface beneath the sphere
- [ ] Pixels not hitting any object display a background color distinct from object colors
- [ ] The image opens correctly in at least two image viewers (GIMP + one other)

#### Technical Notes
- Vec3 class needed for 3D point/vector/color operations (add, subtract, dot, cross, normalize, scale)
- Ray class: origin (Point3) + direction (Vec3)
- HitRecord struct: point, normal, t-value, front_face flag, material pointer
- Sphere::hit() implements the quadratic ray-sphere intersection
- Plane::hit() implements the ray-plane dot product intersection
- Lambertian shading: color = albedo * max(0, dot(N, L)) where N = surface normal, L = direction to light
- PPM writer: output "P3\n{width} {height}\n255\n" followed by R G B triplets
- Gamma correction: apply sqrt() to each color channel before converting to 0-255 integer

---

## Feature 1: Additional Geometric Primitives

### US-101: Render Triangles in a Scene

#### Problem (The Pain)
David Okonkwo is a hobbyist 3D artist who wants to place flat polygonal surfaces in his scene (a triangular mirror on a wall, a pyramid shape). With only spheres and infinite planes available, he cannot represent any flat surface with defined edges.

#### Who (The User)
- Hobbyist 3D artist defining scenes via the C++ API
- Wants to build geometric compositions with crisp, flat surfaces
- Comfortable editing C++ code to define scenes

#### Solution (What We Build)
A Triangle primitive defined by three vertex positions that supports ray intersection using the Moller-Trumbore algorithm and returns correct surface normals and barycentric coordinates.

#### Domain Examples

##### Example 1: David places a green triangle in front of the camera
David defines a triangle with vertices at (-1, -0.5, 0), (1, -0.5, 0), (0, 1, 0) and a green Lambertian material (albedo 0.1, 0.8, 0.1). The rendered image shows a large green triangle shape against the background.

##### Example 2: Ray passes through the triangle plane but outside the triangle
David casts a ray that hits the plane containing the triangle but at a point outside the triangle's edges (e.g., far to the right). The intersection test correctly returns no hit, and the background color is rendered at that pixel.

##### Example 3: Ray is parallel to the triangle
David aims the camera parallel to the triangle surface (camera at z = 0, looking along the x-axis, triangle in the xz-plane). The ray is parallel to the triangle's plane. The intersection test returns no hit without dividing by zero or producing NaN.

#### UAT Scenarios (BDD)

##### Scenario: David renders a triangle with correct shape
```
Given David has defined a triangle with vertices A(-1, -0.5, 0), B(1, -0.5, 0), C(0, 1, 0)
  And the triangle has a green Lambertian material (albedo 0.1, 0.8, 0.1)
  And the camera is at (0, 0.2, -2) looking at (0, 0.2, 0)
When David renders the scene at 400x400 pixels
Then the rendered image shows a green triangular shape
  And the triangle's edges appear as straight lines (within pixel resolution)
  And pixels outside the triangle show the background color
```

##### Scenario: Triangle normal is perpendicular to the surface
```
Given David has defined a triangle with vertices A(0,0,0), B(1,0,0), C(0,1,0)
When a ray hits the triangle at point (0.25, 0.25, 0)
Then the reported surface normal is (0, 0, -1) (or (0, 0, 1) depending on winding)
  And the normal is normalized (length = 1.0 within epsilon)
```

##### Scenario: Moller-Trumbore rejects ray parallel to triangle
```
Given David has defined a triangle in the xz-plane (y = 0)
  And a ray originates at (0, 0, -1) with direction (1, 0, 0) (parallel to triangle plane)
When the intersection test runs
Then no hit is reported
  And no division-by-zero or NaN occurs
```

#### Acceptance Criteria
- [ ] Triangle primitive accepts 3 vertex positions and computes a correct surface normal
- [ ] Ray-triangle intersection uses Moller-Trumbore and correctly identifies hits inside the triangle
- [ ] Rays that hit the triangle's plane but miss the triangle report no intersection
- [ ] Rays parallel to the triangle report no intersection without numerical errors
- [ ] Barycentric coordinates (u, v) are reported at the hit point for future normal interpolation
- [ ] Triangle provides a bounding box for BVH compatibility

#### Technical Notes
- Moller-Trumbore algorithm: see research doc Section 3.3
- Barycentric coordinates enable smooth shading when per-vertex normals are provided (future story)
- Winding order determines the default normal direction

---

### US-102: Render Axis-Aligned Boxes

#### Problem (The Pain)
David Okonkwo wants to place rectangular objects in his scenes (a table, a pedestal, building blocks) but has no way to represent a box shape. He would need 12 triangles to approximate a box, which is tedious to set up manually.

#### Who (The User)
- Hobbyist 3D artist building geometric scenes
- Wants a convenient box primitive rather than manual triangle assembly
- Defining scenes programmatically in C++

#### Solution (What We Build)
An axis-aligned box primitive defined by two corner points (min, max) that supports ray intersection using the slab method and provides correct surface normals on each face.

#### Domain Examples

##### Example 1: David places a white box on the ground plane
David defines a box with min corner (-0.5, -0.5, -0.5) and max corner (0.5, 0.5, 0.5) with a white Lambertian material. The camera is positioned to see three faces of the box. The rendered image shows a recognizable rectangular solid with distinct shading on each visible face.

##### Example 2: Ray enters and exits the box
David traces a ray from (-2, 0, 0) in the +x direction through a box centered at the origin. The intersection test returns t_enter (hitting the -x face) and the surface normal (-1, 0, 0) pointing outward from the entry face.

##### Example 3: Ray originates inside the box
David places the camera inside a large box. The intersection test returns the exit point (ray leaving the box) with the front_face flag set to false (back-face hit), correctly rendering the interior walls.

#### UAT Scenarios (BDD)

##### Scenario: David renders a box with three visible faces
```
Given David has defined an axis-aligned box with corners (-0.5, -0.5, -0.5) and (0.5, 0.5, 0.5)
  And the box has a white Lambertian material (albedo 0.9, 0.9, 0.9)
  And the camera is at (2, 2, -3) looking at (0, 0, 0)
  And there is a point light at (3, 5, -2)
When David renders the scene
Then three faces of the box are visible in the image
  And each face has a distinct brightness level (face toward light is brightest)
  And the edges between faces are straight lines at pixel resolution
```

##### Scenario: Slab method correctly identifies miss
```
Given David has defined a unit box at the origin
  And a ray originates at (0, 5, 0) with direction (0, 0, -1) (passing above the box)
When the intersection test runs
Then no hit is reported
```

##### Scenario: Surface normal matches the hit face
```
Given David has defined a unit box at the origin
  And a ray originates at (-3, 0, 0) with direction (1, 0, 0)
When the ray hits the box
Then the reported hit point is approximately (-0.5, 0, 0)
  And the surface normal is (-1, 0, 0) (outward normal of the -x face)
```

#### Acceptance Criteria
- [ ] Box primitive accepts two corner points (min, max) and handles min > max by swapping
- [ ] Ray-box intersection uses the slab method and returns the nearest positive hit
- [ ] Surface normal at the hit point correctly identifies which face was struck
- [ ] Box provides a bounding box (itself) for BVH compatibility
- [ ] A ray originating inside the box hits the exit face with front_face = false

#### Technical Notes
- Slab method: see research doc Section 3.5
- The box must determine which face was hit to provide the correct normal; compare hit point coordinates against min/max with epsilon tolerance
- This AABB intersection is reused by BVH traversal (Feature 8)

---

### US-103: Render Triangle Meshes with Smooth Shading

#### Problem (The Pain)
Sofia Reyes is a technical artist who wants to render smooth curved objects like a teapot or a vase. Individual triangles produce faceted, polygonal surfaces. She needs the renderer to interpolate normals across triangle faces so that a mesh of triangles appears smoothly curved.

#### Who (The User)
- Technical artist creating product visualizations
- Works with meshes exported from 3D modeling tools
- Needs smooth, professional-quality surface rendering

#### Solution (What We Build)
A TriangleMesh class that stores shared vertices with per-vertex normals and interpolates normals at hit points using barycentric coordinates from the Moller-Trumbore intersection.

#### Domain Examples

##### Example 1: Sofia renders a smooth-shaded icosphere
Sofia defines a 42-vertex icosphere mesh with per-vertex normals (each normal pointing outward from the sphere center). The rendered image shows a smoothly shaded sphere with no visible triangle edges, closely resembling a true Sphere primitive render.

##### Example 2: Sofia renders a flat-shaded icosphere for comparison
Sofia defines the same icosphere but without per-vertex normals (using face normals only). The rendered image shows visible triangular facets across the surface, clearly different from the smooth-shaded version.

##### Example 3: Sofia renders a mesh with mixed smooth and hard edges
Sofia defines a mesh where some edges are "hard" (adjacent triangles have different per-vertex normals at the shared edge) and others are "smooth" (shared normals). The rendered image shows smooth curves except at the hard edges, which appear as visible creases.

#### UAT Scenarios (BDD)

##### Scenario: Smooth normals produce visually smooth shading
```
Given Sofia has defined a 42-vertex icosphere mesh centered at (0, 0, 0) with radius 1.0
  And each vertex has a normal equal to normalize(vertex_position - center)
  And the mesh has a white Lambertian material
When Sofia renders the scene at 800x800 pixels with a point light at (5, 5, -5)
Then the shading across the mesh surface is smooth (no abrupt brightness jumps between adjacent triangles)
  And the rendered result is visually similar to a Sphere primitive of the same size and material
```

##### Scenario: Barycentric interpolation produces correct interpolated normal
```
Given Sofia has a triangle with vertices V0(0,0,0), V1(1,0,0), V2(0,1,0)
  And per-vertex normals N0(0,0,-1), N1(0.5,0,-0.866), N2(0,0.5,-0.866)
When a ray hits the triangle at barycentric coordinates u=0.33, v=0.33
Then the interpolated normal is approximately normalize((1-0.33-0.33)*N0 + 0.33*N1 + 0.33*N2)
  And the interpolated normal is normalized (length = 1.0 within epsilon)
```

##### Scenario: Mesh with many triangles renders correctly
```
Given Sofia has defined a mesh with 200 triangles forming a torus shape
  And each triangle has per-vertex normals for smooth shading
When Sofia renders the scene
Then the torus appears as a smooth donut shape without visible triangle edges
  And all 200 triangles are intersection-tested (brute force) without errors
```

#### Acceptance Criteria
- [ ] TriangleMesh stores shared vertex positions and per-vertex normals
- [ ] Hit point normal is interpolated from per-vertex normals using barycentric coordinates
- [ ] When no per-vertex normals are provided, the face normal (cross product of edges) is used
- [ ] The interpolated normal is re-normalized after interpolation
- [ ] TriangleMesh provides a bounding box enclosing all vertices for BVH compatibility

#### Technical Notes
- Barycentric interpolation: N = (1-u-v)*N0 + u*N1 + v*N2
- Per-vertex normals come from the mesh definition (e.g., OBJ file or procedural generation)
- Future OBJ loader story will populate TriangleMesh from external files

---

## Feature 2: Enhanced Lighting

### US-201: Support Multiple Point Lights in a Scene

#### Problem (The Pain)
Elena Marchetti has her single-light scene working, but the scenes look flat and unrealistic with only one light source. She wants to add a fill light and a rim light (classic 3-point lighting setup) to produce more professional-looking renders, but the renderer only supports a single hardcoded light.

#### Who (The User)
- CG student learning about lighting setups
- Wants to experiment with multiple lights and observe their combined effect
- Defining scenes in C++ code

#### Solution (What We Build)
A scene lighting system that supports a list of point lights, accumulating each light's contribution to the final shading at every hit point.

#### Domain Examples

##### Example 1: Elena creates a 3-point lighting setup
Elena defines three point lights: a key light (white, position -2, 5, -3, intensity 0.8), a fill light (soft blue, position 3, 2, -2, intensity 0.3), and a rim light (warm yellow, position 0, 3, 3, intensity 0.5). The rendered sphere shows white highlights on one side, a subtle blue fill on the shadow side, and a warm yellow edge highlight on the back rim.

##### Example 2: Elena lights a scene with two colored lights
Elena places a red light on the left and a blue light on the right of a white sphere. The sphere shows red tinting on the left side, blue tinting on the right side, and a purple (blended) region where both lights illuminate the surface.

##### Example 3: A point on the surface is shadowed from one light but not the other
Elena places a small sphere between the main sphere and one of the lights. The shadow from that blocking sphere appears on the main sphere only for the occluded light; the other light still illuminates that region.

#### UAT Scenarios (BDD)

##### Scenario: Two lights produce additive illumination
```
Given Elena has defined a white sphere at the origin with Lambertian material (albedo 0.9, 0.9, 0.9)
  And there is a red point light at (-3, 3, -3) with color (1.0, 0.0, 0.0) and intensity 0.6
  And there is a blue point light at (3, 3, -3) with color (0.0, 0.0, 1.0) and intensity 0.6
  And the camera is at (0, 0, -3) looking at (0, 0, 0)
When Elena renders the scene
Then the left side of the sphere appears reddish (R dominant)
  And the right side of the sphere appears bluish (B dominant)
  And the front-center of the sphere appears purple/magenta (R and B both significant)
```

##### Scenario: Shadow from one light does not affect another light's contribution
```
Given Elena has a large sphere at the origin and a small sphere at (-1, 1, -0.5)
  And there is a point light at (-2, 2, -1) (occluded by the small sphere for part of the large sphere)
  And there is a point light at (2, 2, -1) (unoccluded view of the large sphere)
When Elena renders the scene
Then the region on the large sphere shadowed by the small sphere from light 1 is still partially illuminated by light 2
  And that shadowed region is dimmer than the fully lit region but brighter than a region shadowed from both lights
```

##### Scenario: Zero lights produce ambient-only illumination
```
Given Elena has defined a scene with a sphere but no lights
When Elena renders the scene
Then the sphere appears uniformly dark or at ambient level
  And no shading variation is visible (no directional light contribution)
```

#### Acceptance Criteria
- [ ] The scene supports a list of 0 or more point lights
- [ ] Each light's contribution is computed independently and added to the surface color
- [ ] Shadow rays are cast toward each light independently
- [ ] Colored lights tint the surface color (light color * material albedo * diffuse factor)
- [ ] A scene with zero lights produces a valid image (ambient-only or black)

#### Technical Notes
- Shading loop: for each light, compute diffuse contribution = albedo * light_color * intensity * max(0, dot(N, L)) if not in shadow; sum all contributions
- Ambient term (small constant added regardless of lights) prevents completely black shadows

---

### US-202: Directional Light Support

#### Problem (The Pain)
David Okonkwo wants to simulate outdoor sunlight in his scenes, but point lights create a radial falloff pattern that looks like a nearby lamp, not the sun. He needs a light type where all rays arrive in parallel from a fixed direction, mimicking a light source at infinite distance.

#### Who (The User)
- Hobbyist 3D artist creating outdoor scenes
- Wants realistic sunlight-style illumination
- Building scenes programmatically in C++

#### Solution (What We Build)
A directional light type defined by a direction vector and color/intensity, where shadow rays are cast in the light's direction with infinite distance (no t_max limit from a position).

#### Domain Examples

##### Example 1: David renders a scene with overhead directional sun
David defines a directional light with direction (0, -1, 0) (straight down), color white, intensity 1.0. A sphere on a ground plane shows a highlight on its top and a shadow directly beneath it. The shadow has the same shape regardless of where on the plane the sphere is placed.

##### Example 2: David renders a scene with low-angle directional light
David defines a directional light with direction (-1, -0.3, 0) (low angle from the right). Shadows are long and cast to the left. The right sides of objects are brightly lit while left sides are dark.

##### Example 3: Directional light shadow is consistent across the scene
David places two identical spheres at different positions in the scene. Both spheres cast shadows in exactly the same direction and with the same angle, confirming the light source has no position (infinite distance).

#### UAT Scenarios (BDD)

##### Scenario: Directional light produces parallel shadow rays
```
Given David has defined a ground plane and two spheres at positions (-2, 0, 0) and (2, 0, 0)
  And there is a directional light with direction (-1, -1, 0) normalized
When David renders the scene
Then both spheres cast shadows in the same direction on the ground plane
  And the shadow directions are parallel (both offset in +x direction from their respective spheres)
```

##### Scenario: Shadow ray has no maximum distance for directional light
```
Given David has a sphere at the origin and a directional light from direction (0, -1, 0)
  And a second sphere is placed at (0, 100, 0) -- far above in the light's direction
When the shadow ray is cast from a point on the ground toward the directional light
Then the second sphere at y=100 occludes the light (shadow is detected)
  And this confirms the shadow ray travels to infinity (not limited by a light position)
```

##### Scenario: Directional light combined with point light
```
Given David has a directional light (sun) and a point light (lamp) in the same scene
When David renders the scene
Then the sphere receives illumination from both lights
  And the directional light produces consistent parallel shadows
  And the point light produces radial shadows (expanding with distance)
```

#### Acceptance Criteria
- [ ] Directional light is defined by direction, color, and intensity (no position)
- [ ] Shadow rays for directional light travel in the light direction with t_max = infinity
- [ ] Directional light produces parallel shadows across the entire scene
- [ ] Directional light integrates into the existing multi-light shading loop alongside point lights

#### Technical Notes
- Shadow ray direction = -light_direction (toward the light)
- Shadow ray t_max = infinity (or a very large number) since the light has no position
- No distance-based attenuation for directional lights (constant intensity)

---

### US-203: Hard Shadows from Point and Directional Lights

#### Problem (The Pain)
Elena Marchetti's rendered sphere floats above the ground plane with no shadow, making the image look unrealistic and making it impossible to tell where objects are relative to each other in the scene. She needs the renderer to detect when a surface point is blocked from a light source by another object and darken that point accordingly.

#### Who (The User)
- CG student building a realistic renderer
- Wants shadows to ground objects in the scene and add depth
- Needs to understand shadow acne and how to prevent it

#### Solution (What We Build)
Shadow ray casting from each hit point toward each light source. If any object intersects the shadow ray between the surface and the light, the light's contribution is zeroed for that point.

#### Domain Examples

##### Example 1: Elena sees a sphere's shadow on the ground plane
Elena renders the default scene (red sphere on gray plane, one point light above-left). The ground plane directly beneath and to the right of the sphere (opposite the light) shows a dark circular shadow matching the sphere's silhouette as seen from the light.

##### Example 2: Shadow acne is not visible
Elena renders a large flat plane with a point light overhead. The plane's surface is uniformly lit with no speckled dark spots (shadow acne). The epsilon offset on the shadow ray origin prevents the surface from self-shadowing.

##### Example 3: Object casts shadow on another object
Elena places a small sphere above and to the left of a larger sphere. The small sphere casts a shadow on the surface of the larger sphere, visible as a dark spot on the larger sphere's illuminated side.

#### UAT Scenarios (BDD)

##### Scenario: Sphere casts a shadow on the ground plane
```
Given Elena has a red sphere at (0, 0, 0) with radius 0.5
  And a gray ground plane at y = -0.5
  And a white point light at (-2, 3, -1)
When Elena renders the scene
Then the ground plane area directly beneath the sphere (roughly x in [-0.5, 0.5], z in [-0.5, 0.5]) is darker than the surrounding illuminated plane
  And the shadow shape is approximately circular (matching the sphere's silhouette from the light's perspective)
```

##### Scenario: No shadow acne on flat surfaces
```
Given Elena has a large ground plane with a point light directly above at (0, 10, 0)
When Elena renders the scene
Then the ground plane appears uniformly lit with no random dark specks or noise
  And the shadow ray origin is offset by epsilon * normal above the surface
```

##### Scenario: Object beyond the light does not cast a shadow
```
Given Elena has a sphere at the origin and a point light at (0, 2, 0)
  And a second sphere at (0, 5, 0) -- above and beyond the light
When Elena renders the scene
Then the second sphere does not cast a shadow on the first sphere
  And the shadow ray's t_max is limited to the distance to the light
```

##### Scenario: Self-shadow on sphere (terminator)
```
Given Elena has a sphere with a point light on its left side
When Elena renders the scene
Then the right side of the sphere (facing away from the light) is dark
  And the boundary between lit and unlit areas (the terminator) is a smooth curve across the sphere
```

#### Acceptance Criteria
- [ ] Shadow rays are cast from each hit point toward each light source
- [ ] If any object intersects the shadow ray between the surface and the light, the light's diffuse contribution is zero
- [ ] Shadow ray origin is offset by epsilon along the surface normal (epsilon = 0.001) to prevent shadow acne
- [ ] Objects beyond the light (t > t_light) do not cast shadows
- [ ] Self-shadowing on curved surfaces (terminator) produces smooth transitions

#### Technical Notes
- Shadow acne prevention: shadow_origin = hit_point + 0.001 * normal
- For point lights: t_max = distance(hit_point, light_position)
- For directional lights: t_max = infinity
- Shadow ray only needs an "any hit" test (can early-exit on first intersection, no need to find closest)

---

## Feature 3: Material System (Specular/Metallic)

### US-301: Metal Material with Mirror Reflection

#### Problem (The Pain)
David Okonkwo wants to create a scene with a chrome sphere that reflects its surroundings, but the Lambertian material scatters light in random directions, producing a matte surface with no visible reflections. He needs a material that bounces rays in the mirror reflection direction so the sphere reflects other objects in the scene.

#### Who (The User)
- Hobbyist 3D artist wanting reflective surfaces
- Building a scene with mixed materials (some matte, some reflective)
- Wants to see other objects reflected in a chrome sphere

#### Solution (What We Build)
A Metal material that reflects incident rays in the perfect mirror direction (R = I - 2(I dot N)N) and attenuates the reflected color by the metal's albedo (tint color).

#### Domain Examples

##### Example 1: David renders a chrome sphere next to a red matte sphere
David places a chrome metal sphere (albedo 0.8, 0.8, 0.8; fuzziness 0) to the left of a red Lambertian sphere. The chrome sphere's surface shows a reflection of the red sphere, the ground plane, and the sky. The reflection is sharp and undistorted on the metal surface.

##### Example 2: David renders a gold-tinted metal sphere
David defines a metal sphere with albedo (0.8, 0.6, 0.2) and fuzziness 0. The reflected surroundings appear with a warm gold tint because the metal absorbs some wavelengths from the reflected light.

##### Example 3: Mirror reflection recursion with two metal spheres
David places two metal spheres facing each other. Each sphere reflects the other, and the reflection of the reflection is visible (up to the recursion depth limit). The recursive reflections get progressively dimmer due to attenuation.

#### UAT Scenarios (BDD)

##### Scenario: Metal sphere reflects surrounding objects
```
Given David has a metal sphere at (-1, 0, 0) with albedo (0.8, 0.8, 0.8) and fuzziness 0
  And a red Lambertian sphere at (1, 0, 0)
  And a gray ground plane
  And a point light at (0, 5, -3)
  And the camera is at (0, 1, -3) looking at (0, 0, 0)
When David renders the scene with max recursion depth 10
Then the metal sphere surface shows a recognizable reflection of the red sphere
  And the metal sphere surface shows a reflection of the ground plane and sky
```

##### Scenario: Metal albedo tints the reflection
```
Given David has a gold metal sphere with albedo (0.8, 0.6, 0.2) and fuzziness 0
  And a white Lambertian sphere nearby
When David renders the scene
Then the white sphere's reflection in the gold sphere appears tinted warm/gold (R > G > B)
  And the tinting is consistent across the reflected image
```

##### Scenario: Recursion depth limits infinite reflections
```
Given David has two metal spheres facing each other at distance 2.0
  And max recursion depth is set to 5
When David renders the scene
Then reflections are visible up to 5 levels deep
  And the deepest reflection returns black (recursion terminated)
  And each successive reflection is dimmer than the previous (attenuated by albedo)
```

#### Acceptance Criteria
- [ ] Metal material reflects rays in the mirror direction: R = I - 2(I dot N)N
- [ ] Reflected color is attenuated by the metal's albedo (component-wise multiplication)
- [ ] Recursion depth is respected; at max depth, black is returned
- [ ] Metal with albedo (1,1,1) produces a perfect mirror (no color tinting)
- [ ] Metal with albedo (0.8, 0.6, 0.2) produces gold-tinted reflections

#### Technical Notes
- reflect(I, N) = I - 2*dot(I, N)*N (I is the incoming ray direction, N is unit normal)
- The scattered ray origin must be offset by epsilon along the normal to prevent self-intersection
- Recursion depth tracking: trace_ray(ray, scene, depth+1); terminate when depth >= max_depth

---

### US-302: Glossy (Fuzzy) Metal Reflections

#### Problem (The Pain)
David Okonkwo has mirror-perfect metal spheres working, but real metals are not perfect mirrors. A brushed aluminum surface or a well-worn copper pot has blurry reflections, not sharp ones. David wants to control the roughness of metal surfaces to create a range of appearances from mirror-sharp to blurry.

#### Who (The User)
- Hobbyist 3D artist wanting varied metal appearances
- Wants a single "fuzziness" parameter to control reflection sharpness
- Building scenes with brushed metal, polished copper, rough iron

#### Solution (What We Build)
Extend the Metal material with a fuzziness parameter (0 to 1) that perturbs the reflected ray direction by a random vector scaled by the fuzziness value.

#### Domain Examples

##### Example 1: David renders three metal spheres with varying fuzziness
David places three metal spheres side by side: fuzziness 0 (mirror), fuzziness 0.3 (slightly blurry), fuzziness 0.8 (very blurry). The mirror sphere shows sharp reflections. The fuzziness-0.3 sphere shows recognizable but softened reflections. The fuzziness-0.8 sphere shows very diffuse, almost unrecognizable reflections.

##### Example 2: Fuzzy reflection ray scatters below the surface
David renders a highly fuzzy metal sphere (fuzziness 0.95). Some reflected rays, after perturbation, point below the surface (dot(perturbed, normal) < 0). Those rays are absorbed (contribute black), creating a slightly darker overall appearance.

##### Example 3: Fuzziness 0 produces identical result to plain mirror metal
David renders two scenes: one with Metal(fuzziness=0) and one with plain mirror Metal. The rendered images are visually identical (same sharp reflections).

#### UAT Scenarios (BDD)

##### Scenario: Increasing fuzziness progressively blurs reflections
```
Given David has three metal spheres with fuzziness 0.0, 0.3, and 0.8 respectively
  And all three have albedo (0.8, 0.8, 0.8)
  And a colorful scene surrounds them (red and blue spheres, ground plane)
When David renders the scene with 50 samples per pixel
Then the fuzziness-0.0 sphere shows sharp, clearly defined reflections
  And the fuzziness-0.3 sphere shows recognizable but blurry reflections
  And the fuzziness-0.8 sphere shows very diffuse, barely recognizable reflections
```

##### Scenario: Fuzzy reflection below surface is absorbed
```
Given David has a metal sphere with fuzziness 1.0 and albedo (0.8, 0.8, 0.8)
When a reflected ray is perturbed such that dot(perturbed_direction, normal) <= 0
Then that ray is absorbed (no recursive trace; contributes black)
  And the sphere still renders without errors
```

##### Scenario: Fuzziness is clamped to [0, 1]
```
Given David defines a metal material with fuzziness 1.5
When the material is created
Then the fuzziness is clamped to 1.0
  And the material behaves as if fuzziness were 1.0
```

#### Acceptance Criteria
- [ ] Metal material accepts a fuzziness parameter (0.0 to 1.0, clamped)
- [ ] Fuzziness 0.0 produces mirror-sharp reflections identical to US-301
- [ ] Fuzziness > 0 adds random perturbation: scattered_dir = reflect(I, N) + fuzziness * random_in_unit_sphere()
- [ ] Perturbed rays below the surface (dot < 0) are absorbed
- [ ] Higher fuzziness produces visually blurrier reflections (verified at 50+ SPP)

#### Technical Notes
- random_in_unit_sphere(): generate random point inside unit sphere using rejection sampling
- The fuzziness parameter scales the random perturbation vector
- Multiple samples per pixel are required to see the blur effect (single-sample renders will look noisy)

---

## Feature 4: Transparency and Refraction

### US-401: Dielectric (Glass) Material with Refraction

#### Problem (The Pain)
Elena Marchetti wants to render a glass sphere that bends light passing through it, like a real glass marble. Her current materials (Lambertian, Metal) either scatter randomly or reflect. She has no way to make light pass through an object, bending according to Snell's law, to produce the characteristic glass appearance with see-through transparency and distorted views of objects behind the glass.

#### Who (The User)
- CG student learning about light transport
- Wants to visualize Snell's law and the Fresnel effect in practice
- Building scenes with glass, water, and diamond materials

#### Solution (What We Build)
A Dielectric material that refracts rays through the surface using Snell's law, chooses between reflection and refraction using Schlick's Fresnel approximation, and handles total internal reflection when the angle exceeds the critical angle.

#### Domain Examples

##### Example 1: Elena renders a glass sphere (IOR 1.5) in front of a checkered plane
Elena places a glass sphere (IOR 1.5) over a checkered ground plane. Looking through the glass sphere, the checkered pattern appears magnified and inverted (the sphere acts as a converging lens). The edges of the sphere show stronger reflections (Fresnel effect) while the center is mostly transparent.

##### Example 2: Elena renders a diamond sphere (IOR 2.42)
Elena creates a sphere with IOR 2.42 (diamond). The high refractive index produces stronger refraction, more prominent total internal reflection, and a more "sparkly" appearance with brighter edge reflections compared to glass.

##### Example 3: Total internal reflection inside a glass sphere
A ray inside the glass sphere hits the inner surface at a steep angle (beyond the critical angle for glass-to-air). The ray reflects internally instead of exiting the sphere. This produces the bright caustic ring visible at the edge of glass spheres.

#### UAT Scenarios (BDD)

##### Scenario: Glass sphere transmits and refracts light
```
Given Elena has a glass sphere at (0, 0, 0) with radius 0.5 and IOR 1.5
  And a red Lambertian sphere at (0, 0, 2) behind the glass sphere
  And the camera is at (0, 0, -2) looking at (0, 0, 0)
When Elena renders the scene with max depth 10
Then the red sphere is visible through the glass sphere
  And the red sphere appears distorted (magnified/inverted) due to refraction
  And the edges of the glass sphere show more reflection than the center (Fresnel effect)
```

##### Scenario: Total internal reflection occurs at steep angles
```
Given Elena has a glass sphere with IOR 1.5
  And a ray is traveling inside the sphere and hits the inner surface at an angle greater than arcsin(1/1.5) = 41.8 degrees from the normal
When the material scatter function evaluates
Then the ray reflects internally (total internal reflection)
  And no refracted ray exits the sphere at that point
```

##### Scenario: Schlick approximation governs reflection probability
```
Given Elena has a glass sphere with IOR 1.5
  And a ray hits the sphere surface at normal incidence (straight on)
When the Schlick reflectance is computed
Then reflectance is approximately ((1 - 1.5) / (1 + 1.5))^2 = 0.04 (4%)
  And approximately 96% of rays are refracted at this angle
```

##### Scenario: Glass with IOR 1.0 is invisible
```
Given Elena has a sphere with dielectric material and IOR 1.0
  And the sphere is between the camera and a colored background
When Elena renders the scene
Then the sphere is effectively invisible (no bending, no reflection)
  And the background appears undistorted through the sphere
```

#### Acceptance Criteria
- [ ] Dielectric material refracts rays using Snell's law: sin(theta_t) = (n1/n2) * sin(theta_i)
- [ ] Schlick's approximation determines the probability of reflection vs. refraction
- [ ] Total internal reflection occurs when sin^2(theta_t) > 1.0
- [ ] The material correctly tracks whether the ray is entering or exiting the medium (using front_face)
- [ ] IOR 1.5 produces a recognizable glass appearance; IOR 2.42 produces a diamond appearance
- [ ] IOR 1.0 produces an effectively invisible sphere

#### Technical Notes
- Refraction direction: see research doc Section 7.2
- Schlick's approximation: F = F0 + (1-F0)*(1-cos_theta)^5 where F0 = ((n1-n2)/(n1+n2))^2
- When entering (front_face = true): eta = 1.0 / ior; when exiting: eta = ior
- Recursion depth must be sufficient (10+) for glass to look correct (ray bounces inside the sphere)

---

### US-402: Hollow Glass Sphere (Glass Shell Effect)

#### Problem (The Pain)
Elena Marchetti renders a solid glass sphere but it looks like a solid glass marble, not a thin glass bubble or ornament. She wants to create a hollow glass sphere -- like a Christmas ornament -- where the glass is a thin shell with air inside. She finds it non-obvious how to achieve this with the existing dielectric material.

#### Who (The User)
- CG student experimenting with glass effects
- Wants to understand the "negative radius trick" for hollow spheres
- Building a scene with both solid and hollow glass objects

#### Solution (What We Build)
Support for hollow glass spheres by placing a smaller sphere with an inverted normal (negative radius convention) inside a larger glass sphere, creating an air pocket that changes the refraction behavior.

#### Domain Examples

##### Example 1: Elena renders a hollow glass sphere vs. a solid one
Elena places two glass spheres side by side: one solid (single sphere, IOR 1.5) and one hollow (outer sphere radius 0.5 with inner sphere radius -0.45, both IOR 1.5). The solid sphere magnifies and inverts the background; the hollow sphere shows a thinner refraction effect with a characteristic bright ring at the edge and a less distorted view through the center.

##### Example 2: Elena renders a glass bubble with a colored background
Elena places a hollow glass sphere in front of a striped background. Through the hollow sphere, the background appears less distorted than through a solid sphere, and the edge of the sphere shows a bright ring (caustic from the thin shell).

##### Example 3: Elena nests spheres to create concentric glass shells
Elena creates three concentric spheres (radii 1.0, -0.9, 0.8) all with IOR 1.5, creating two thin glass shells. The render shows complex internal reflections and refractions from the multiple glass-air boundaries.

#### UAT Scenarios (BDD)

##### Scenario: Hollow glass sphere differs visually from solid
```
Given Elena has a solid glass sphere at (-1, 0, 0) with radius 0.5 and IOR 1.5
  And a hollow glass sphere at (1, 0, 0) made of outer sphere radius 0.5 IOR 1.5 and inner sphere radius -0.45 IOR 1.5
  And a striped background is visible behind both spheres
When Elena renders the scene with max depth 10 and 50 SPP
Then the solid sphere shows strong magnification/inversion of the background
  And the hollow sphere shows less distortion through its center
  And the hollow sphere shows a bright ring at its edges
```

##### Scenario: Negative radius inverts the surface normal
```
Given Elena has defined a sphere with radius -0.45
When a ray intersects this sphere
Then the surface normal points inward (opposite to the standard outward normal)
  And this causes the front_face determination to flip
  And refraction calculations treat the ray as exiting glass into air at the inner surface
```

##### Scenario: Hollow sphere is visually distinguishable from a solid sphere
```
Given Elena renders both a solid and hollow glass sphere under identical lighting
When Elena compares the center pixels of both spheres
Then the center of the hollow sphere is closer in color to the background (less distortion)
  And the center of the solid sphere shows stronger distortion of the background
```

#### Acceptance Criteria
- [ ] A sphere with negative radius inverts its surface normals
- [ ] The combination of outer positive-radius sphere and inner negative-radius sphere produces a hollow glass shell
- [ ] The hollow glass sphere is visually distinct from a solid glass sphere (less central distortion, bright edge ring)
- [ ] Multiple nested spheres with alternating radii produce correct multi-layer refraction

#### Technical Notes
- Negative radius convention: when radius < 0, use abs(radius) for intersection math but negate the computed normal
- The inner sphere creates an air-glass boundary where the normal points inward, causing the refraction to reverse
- This is a well-known technique from "Ray Tracing in One Weekend"

---

## Feature 5: Reflections (Mirror and Glossy)

### US-501: Recursive Reflection with Configurable Depth

#### Problem (The Pain)
Prof. Kenji Tanaka is using the ray tracer in his coursework to demonstrate recursive ray tracing. He needs students to observe how reflection depth affects visual quality and render time, and to understand why unbounded recursion is dangerous (two mirrors facing each other). Currently there is no way to configure the maximum recursion depth or observe its effect.

#### Who (The User)
- CS instructor demonstrating ray tracing concepts
- Needs configurable recursion depth for teaching purposes
- Wants students to see the visual difference between depth 1, 5, and 50

#### Solution (What We Build)
A configurable maximum recursion depth that terminates ray tracing when reached, returning black. The depth parameter is accessible as a render setting.

#### Domain Examples

##### Example 1: Prof. Tanaka demonstrates depth 1 vs. depth 10
Prof. Tanaka renders a scene with a metal sphere reflecting a colorful environment. At depth 1, the sphere shows only direct reflections (one bounce). At depth 10, the sphere shows reflections of reflections, visible as smaller and dimmer copies in the metallic surface.

##### Example 2: Prof. Tanaka shows two mirrors with depth 50
Prof. Tanaka places two flat metal planes facing each other with a colored sphere between them. At depth 50, the reflections show a long tunnel of diminishing copies of the sphere stretching into the distance, each slightly dimmer.

##### Example 3: Prof. Tanaka shows depth 0 produces no reflections
At depth 0, the metal sphere appears black (no secondary rays traced at all), demonstrating that depth 0 means "no light transport beyond the first hit."

#### UAT Scenarios (BDD)

##### Scenario: Increasing depth reveals more reflection bounces
```
Given Prof. Tanaka has two metal spheres facing each other with a red sphere between them
When he renders the scene at depth 1, then at depth 5, then at depth 10
Then the depth-1 image shows the red sphere reflected once in each metal sphere
  And the depth-5 image shows 5 levels of nested reflections
  And the depth-10 image shows 10 levels, each progressively dimmer
```

##### Scenario: Depth 0 returns black for all secondary rays
```
Given Prof. Tanaka has a scene with a metal sphere and a point light
When he renders with max_depth = 0
Then the metal sphere appears black (reflection ray would exceed depth)
  And the Lambertian ground plane also appears black (scatter ray would exceed depth)
```

##### Scenario: Depth limit prevents infinite recursion
```
Given Prof. Tanaka has two parallel mirror planes with max_depth = 100
When he renders the scene
Then rendering completes in finite time
  And no stack overflow occurs
  And the deepest reflections return black (depth exhausted)
```

#### Acceptance Criteria
- [ ] Maximum recursion depth is configurable (integer, default 10, range 0-100)
- [ ] When depth is exhausted, trace_ray returns black (0, 0, 0)
- [ ] Increasing depth from 1 to 10 produces visibly more reflection bounces
- [ ] Depth 0 produces an entirely black image (no secondary rays)
- [ ] Rendering with two parallel mirrors at depth 100 completes without stack overflow

#### Technical Notes
- trace_ray(ray, scene, depth): if depth >= max_depth return Color(0,0,0)
- Each recursive call increments depth: trace_ray(scattered_ray, scene, depth + 1)
- Default max_depth = 10 provides good visual quality for most scenes

---

## Feature 6: Camera Enhancements

### US-601: Configurable Pinhole Camera (Position, Target, FOV)

#### Problem (The Pain)
Elena Marchetti has a working renderer but the camera is hardcoded. She cannot change the viewpoint, aim the camera at a different part of the scene, or adjust the field of view to zoom in/out. Every change requires recompiling the code with manually computed ray directions.

#### Who (The User)
- CG student who wants to explore a scene from different angles
- Wants intuitive camera parameters (where am I, what am I looking at, how wide is my view)
- Modifying camera values in the scene definition

#### Solution (What We Build)
A pinhole camera configurable by position (lookfrom), target (lookat), up vector (vup), vertical field of view (vfov in degrees), and aspect ratio. The camera constructs an orthonormal basis and generates correct rays for any configuration.

#### Domain Examples

##### Example 1: Elena orbits the camera around the scene
Elena renders three images: camera at (0, 0, -3), then (3, 0, 0), then (0, 3, 0), all looking at the origin. Each image shows the scene from a different angle -- front, side, and top-down views.

##### Example 2: Elena zooms in by reducing FOV
Elena renders with vfov = 90 (wide angle) and then vfov = 20 (narrow, zoomed in). The narrow FOV image shows a zoomed-in view of the center of the scene, as if using a telephoto lens.

##### Example 3: Elena tilts the camera by changing the up vector
Elena sets vup to (1, 0, 0) instead of (0, 1, 0). The rendered image is rotated 90 degrees (the scene appears tilted on its side) because the camera's "up" is now the world's "right."

#### UAT Scenarios (BDD)

##### Scenario: Camera position and target control the viewpoint
```
Given Elena has a red sphere at the origin and a green sphere at (3, 0, 0)
When Elena places the camera at (0, 0, -5) looking at (0, 0, 0)
Then the red sphere is centered in the image and the green sphere is to the right
When Elena places the camera at (5, 0, -5) looking at (3, 0, 0)
Then the green sphere is centered and the red sphere is to the left
```

##### Scenario: Field of view controls zoom level
```
Given Elena has a sphere at the origin
When Elena renders with vfov = 90
Then the sphere occupies roughly 1/4 of the image width
When Elena renders with vfov = 20
Then the sphere occupies roughly the full image width (zoomed in)
```

##### Scenario: Orthonormal basis is correctly constructed
```
Given Elena sets lookfrom = (0, 0, -3), lookat = (0, 0, 0), vup = (0, 1, 0)
When the camera constructs its basis vectors
Then w = normalize((0,0,-3) - (0,0,0)) = (0, 0, -1)
  And u = normalize(cross((0,1,0), (0,0,-1))) = (1, 0, 0)
  And v = cross((0,0,-1), (1,0,0)) = (0, 1, 0)
```

#### Acceptance Criteria
- [ ] Camera accepts lookfrom, lookat, vup, vfov (degrees), and aspect_ratio parameters
- [ ] An orthonormal basis (u, v, w) is constructed from the camera parameters
- [ ] Rays are generated correctly for all pixels across the viewport
- [ ] Changing lookfrom moves the viewpoint; changing lookat re-aims the camera
- [ ] Changing vfov adjusts zoom (smaller FOV = more zoom)
- [ ] Changing vup rotates the camera roll

#### Technical Notes
- Camera basis: w = normalize(lookfrom - lookat), u = normalize(cross(vup, w)), v = cross(w, u)
- Viewport: h = 2 * tan(vfov_radians / 2), viewport_height = h, viewport_width = h * aspect_ratio
- Ray for pixel (px, py): direction = viewport_upper_left + px*pixel_delta_u + py*pixel_delta_v - lookfrom

---

### US-602: Thin Lens Camera with Depth of Field

#### Problem (The Pain)
Sofia Reyes is creating product visualization renders where she wants the product in the foreground to be sharp while the background is artistically blurred (shallow depth of field). The pinhole camera renders everything in perfect focus, which looks artificial and does not match real photography. She needs a camera model that simulates a real lens with controllable focus and blur.

#### Who (The User)
- Technical artist creating product photography-style renders
- Wants selective focus: sharp foreground, blurred background (or vice versa)
- Needs to control aperture size and focus distance

#### Solution (What We Build)
A thin lens camera that samples ray origins across a disc (the aperture) and converges all rays through each pixel at the focal plane distance. Objects at the focus distance are sharp; objects closer or farther are blurred proportionally to the aperture size.

#### Domain Examples

##### Example 1: Sofia focuses on a foreground sphere with a blurred background
Sofia places a red sphere at z = -2 and a blue sphere at z = -8, with focus_distance = 2.0 and aperture = 0.5. The red sphere at the focus distance is perfectly sharp. The blue sphere in the background is noticeably blurred (defocused).

##### Example 2: Sofia renders with aperture 0 (pinhole equivalent)
Sofia sets aperture = 0, which eliminates the lens disc (all rays originate from a single point). The result is identical to a pinhole camera -- everything is in focus regardless of distance.

##### Example 3: Sofia renders with a very large aperture for extreme blur
Sofia sets aperture = 2.0 (very large). Only objects exactly at the focus distance are sharp; everything else is extremely blurred, creating a dramatic tilt-shift effect.

#### UAT Scenarios (BDD)

##### Scenario: Objects at focus distance are sharp; distant objects are blurred
```
Given Sofia has a red sphere at z = -3 (focus_distance = 3.0)
  And a blue sphere at z = -10
  And the camera has aperture = 0.5 and 100 samples per pixel
When Sofia renders the scene
Then the red sphere at the focus distance has sharp edges (color transitions within 1-2 pixels)
  And the blue sphere in the background has blurred edges (color transitions spanning 5+ pixels)
```

##### Scenario: Aperture 0 produces pinhole-equivalent result
```
Given Sofia renders a scene with aperture = 0 and focus_distance = 5.0
  And then renders the same scene with a pinhole camera at the same position
When Sofia compares both images
Then the images are visually identical (within sampling noise)
  And all objects are in focus in both images
```

##### Scenario: Larger aperture produces more blur
```
Given Sofia renders three images of the same scene with aperture 0.1, 0.5, and 2.0
  And focus_distance = 3.0 in all three
When Sofia compares the out-of-focus regions
Then aperture 0.1 shows minimal blur
  And aperture 0.5 shows moderate blur
  And aperture 2.0 shows extreme blur
```

#### Acceptance Criteria
- [ ] Thin lens camera accepts aperture diameter and focus_distance parameters in addition to pinhole params
- [ ] Ray origins are randomly sampled from a disc of diameter = aperture centered at the camera position
- [ ] All rays for a given pixel converge at the same point on the focal plane
- [ ] Objects at focus_distance appear sharp; objects nearer/farther appear blurred proportionally to aperture
- [ ] Aperture = 0 produces results identical to a pinhole camera
- [ ] Depth of field effect requires multiple samples per pixel to be visible (SPP >= 10)

#### Technical Notes
- Random point on lens disc: random_in_unit_disk() * (aperture / 2)
- Offset = u * random_disc.x + v * random_disc.y
- Ray origin = camera_position + offset; ray direction = focal_point - ray_origin
- Focus distance must be > 0

---

## Feature 7: Anti-Aliasing and Sampling

### US-701: Random Supersampling Anti-Aliasing

#### Problem (The Pain)
Elena Marchetti notices that the edges of her rendered spheres look jagged (staircase pattern) because each pixel is sampled by a single ray through its center. She knows that real images have smooth edges, and she wants to reduce this aliasing by casting multiple rays per pixel with slight random offsets and averaging the results.

#### Who (The User)
- CG student learning about sampling theory
- Wants smoother edges and less noise in rendered images
- Willing to trade render time for image quality

#### Solution (What We Build)
Multi-sample rendering where each pixel fires N rays with random offsets within the pixel area, and the final pixel color is the average of all sample colors.

#### Domain Examples

##### Example 1: Elena renders at 1 SPP and 100 SPP
Elena renders the same scene at 1 sample per pixel (jagged edges, noisy reflections) and 100 SPP (smooth edges, clean reflections). The difference is dramatic -- the 100 SPP image looks professional.

##### Example 2: Elena renders a thin diagonal line
Elena places a very thin cylinder at a diagonal angle. At 1 SPP, some pixels miss the cylinder entirely, producing a broken dotted line. At 16 SPP, the line appears continuous with smooth anti-aliased edges.

##### Example 3: Elena observes linear time scaling
Elena measures render time at 1, 10, 50, and 100 SPP. Each increase in SPP increases render time approximately linearly (50 SPP takes roughly 5x longer than 10 SPP).

#### UAT Scenarios (BDD)

##### Scenario: Higher SPP reduces visible aliasing
```
Given Elena renders a scene with a sphere against a contrasting background
When she renders at 1 SPP
Then the sphere edge shows visible staircase/jagged pixels
When she renders at 100 SPP
Then the sphere edge shows smooth anti-aliased transitions (intermediate colors at boundaries)
```

##### Scenario: Random offsets are within the pixel area
```
Given Elena renders with N samples per pixel
When each sample is generated for pixel (x, y)
Then the ray passes through the point (x + random[0,1), y + random[0,1))
  And no sample falls outside the pixel's unit square
```

##### Scenario: SPP = 1 degrades gracefully to single-sample rendering
```
Given Elena renders with 1 sample per pixel
When the image is produced
Then the result is a valid image (identical to the walking skeleton output)
  And the ray passes through approximately the pixel center (with a single random offset)
```

##### Scenario: Average of samples produces correct pixel color
```
Given Elena renders a pixel that is 50% covered by a red sphere and 50% background
  And she uses 1000 SPP
When the pixel color is computed
Then the pixel color is approximately 50% red + 50% background (within statistical noise)
```

#### Acceptance Criteria
- [ ] Samples per pixel (SPP) is configurable (default 10, range 1-10000)
- [ ] Each sample ray has a random offset within the pixel area [x, x+1) x [y, y+1)
- [ ] Final pixel color is the arithmetic mean of all sample colors
- [ ] Higher SPP produces visibly smoother edges
- [ ] SPP = 1 produces a valid (aliased) image
- [ ] Render time scales approximately linearly with SPP

#### Technical Notes
- For each pixel: accumulate color from N random rays, then divide by N
- Random offset: u = (px + random_double()) / image_width; v = (py + random_double()) / image_height
- Gamma correction applied after averaging, not per-sample

---

### US-702: Stratified (Jittered) Sampling

#### Problem (The Pain)
Elena Marchetti is using random supersampling but notices that at low SPP (4-16), the randomness sometimes clusters samples in one part of the pixel, leaving other areas undersampled. This produces uneven noise. She wants a sampling strategy that guarantees even coverage of the pixel area while still avoiding the regular grid patterns that cause Moire artifacts.

#### Who (The User)
- CG student studying sampling theory
- Wants better quality at the same SPP count
- Understands the tradeoff between random and stratified approaches

#### Solution (What We Build)
Stratified sampling that divides each pixel into a grid of N x N strata and places one jittered (randomly offset) sample within each stratum, guaranteeing full coverage with randomized positions.

#### Domain Examples

##### Example 1: Elena compares random vs. stratified at 16 SPP
Elena renders the same scene with 16 random samples and 16 stratified samples (4x4 grid). The stratified version shows noticeably less noise and smoother edges at the same sample count.

##### Example 2: Elena uses 4 SPP stratified (2x2 grid)
Elena renders with sqrt_spp = 2 (4 total samples in a 2x2 grid). Each quadrant of the pixel gets exactly one sample, with random jitter within that quadrant. The result is less noisy than 4 purely random samples.

##### Example 3: Elena uses 1 SPP stratified (1x1 grid)
Elena renders with sqrt_spp = 1 (1 sample per pixel). This degenerates to a single randomly jittered sample, equivalent to random supersampling with SPP = 1.

#### UAT Scenarios (BDD)

##### Scenario: Stratified sampling produces less noise than random at equal SPP
```
Given Elena renders a scene with a glass sphere (complex lighting) at 16 SPP
When she renders with random supersampling
  And then renders with stratified sampling (4x4 grid)
Then the stratified image shows less noise in the glass sphere's reflections and refractions
  And both images have comparable overall brightness and color
```

##### Scenario: Strata divide the pixel evenly
```
Given Elena renders with sqrt_spp = 4 (16 total samples, 4x4 grid)
When samples are generated for pixel (x, y)
Then sample (i, j) falls within the sub-pixel region [x + i/4, x + (i+1)/4) x [y + j/4, y + (j+1)/4)
  And exactly one sample falls within each of the 16 sub-regions
```

##### Scenario: Jittering within strata prevents Moire patterns
```
Given Elena renders a scene with a fine striped pattern at 16 SPP stratified
When Elena examines the rendered stripes
Then no Moire interference pattern is visible
  And the jitter within each stratum introduces enough randomness to break regular sampling artifacts
```

#### Acceptance Criteria
- [ ] Stratified sampler divides each pixel into sqrt_spp x sqrt_spp strata
- [ ] Each stratum gets exactly one sample with random jitter within the stratum bounds
- [ ] Total samples per pixel = sqrt_spp^2
- [ ] Stratified sampling produces visibly less noise than random sampling at equal SPP
- [ ] sqrt_spp = 1 degenerates to single-sample rendering

#### Technical Notes
- For stratum (i, j): u = (px + (i + random()) / sqrt_spp) / image_width
- Total samples = sqrt_spp * sqrt_spp; the SPP parameter should accept perfect squares (4, 9, 16, 25, 64, 100)
- Stratified sampling is orthogonal to the camera model (works with both pinhole and thin lens)

---

## Feature 8: Acceleration Structures

### US-801: Bounding Volume Hierarchy (BVH) for Scene Acceleration

#### Problem (The Pain)
Sofia Reyes has built a scene with 500 spheres (random positions, sizes, and materials) to create a complex composition. The render takes over 10 minutes because every ray tests against every sphere (250,000 intersection tests per ray at 1 SPP). She needs the renderer to skip objects that are clearly not in the ray's path so that complex scenes render in reasonable time.

#### Who (The User)
- Technical artist building complex scenes with hundreds of objects
- Needs render time to scale sub-linearly with object count
- Expects no change in visual output when acceleration is enabled

#### Solution (What We Build)
A Bounding Volume Hierarchy (BVH) that recursively partitions scene objects into a binary tree of axis-aligned bounding boxes. During rendering, rays traverse the tree, skipping entire subtrees when the ray misses the bounding box.

#### Domain Examples

##### Example 1: Sofia's 500-sphere scene renders 10x faster with BVH
Sofia renders her 500-sphere scene without BVH (brute force): 10 minutes. She enables BVH and re-renders: under 1 minute. The two images are pixel-identical.

##### Example 2: Sofia's scene with 2 objects shows minimal BVH overhead
Sofia renders a simple 2-object scene with and without BVH. The BVH version is equally fast (minimal overhead for tree construction and traversal on tiny scenes).

##### Example 3: BVH handles degenerate cases
Sofia creates a scene where all 100 spheres have the same center point (degenerate overlap). BVH construction completes without error, falling back to a single leaf node. The render produces the correct result (front-most sphere visible).

#### UAT Scenarios (BDD)

##### Scenario: BVH produces identical images to brute force
```
Given Sofia has a scene with 100 random spheres with varied materials
When she renders with brute-force intersection (no BVH)
  And then renders with BVH acceleration
Then both images are pixel-identical (same RGB values for every pixel)
```

##### Scenario: BVH provides measurable speedup on large scenes
```
Given Sofia has a scene with 500 spheres
When she renders at 400x225 with 1 SPP using brute force and records the time
  And then renders the same scene with BVH and records the time
Then the BVH render is at least 2x faster than brute force
```

##### Scenario: BVH construction handles edge cases
```
Given Sofia has a scene with 1 sphere
When BVH is constructed
Then the BVH has a single leaf node containing the sphere
  And the bounding box equals the sphere's bounding box
  And rendering produces the correct image
```

##### Scenario: BVH with empty scene
```
Given Sofia has a scene with 0 objects
When BVH is constructed
Then the BVH is empty (root is null or has zero primitives)
  And rendering produces an image showing only the background color
```

#### Acceptance Criteria
- [ ] BVH is constructed from the scene's object list before rendering begins
- [ ] BVH traversal produces identical renders to brute-force traversal
- [ ] BVH provides at least 2x speedup for scenes with 50+ objects
- [ ] BVH handles edge cases: 0 objects, 1 object, overlapping objects
- [ ] BVH construction uses longest-axis midpoint split (initial implementation)
- [ ] Every Hittable object provides a bounding_box() method for BVH construction

#### Technical Notes
- BVH node: AABB bounding box + left/right children (interior) or primitive list (leaf)
- Construction: recursively split along longest axis at midpoint of object centroids
- Leaf node max size: 4 primitives
- Traversal: test ray against node's AABB; if miss, skip entire subtree
- Future optimization: Surface Area Heuristic (SAH) for split quality (separate story)

---

## Feature 9: Scene File Loading

### US-901: Load Scene Definition from YAML File

#### Problem (The Pain)
Elena Marchetti is defining scenes by editing C++ code and recompiling every time she wants to move a sphere or change a color. This edit-compile-run cycle takes 30+ seconds per change and requires her to understand C++ syntax. She wants to describe her scene in a human-readable configuration file that the renderer loads at runtime, so she can iterate on scenes without recompiling.

#### Who (The User)
- CG student iterating on scene designs
- Wants fast iteration: edit file, run renderer, see result
- Comfortable with YAML/JSON syntax but not necessarily C++

#### Solution (What We Build)
A YAML scene file parser that reads camera, lights, objects (with materials), and render settings from a .yaml file and constructs the corresponding scene objects at runtime.

#### Domain Examples

##### Example 1: Elena creates a scene file for the classic "three spheres" scene
Elena writes a YAML file defining three spheres (Lambertian, Metal, Glass), a ground plane, a camera, and a point light. She runs the renderer with `./raytracer scene.yaml` and gets the rendered image without editing any C++ code.

```yaml
camera:
  lookfrom: [0, 1, -3]
  lookat: [0, 0, 0]
  vup: [0, 1, 0]
  vfov: 60
  aspect_ratio: 1.778

render:
  image_width: 800
  samples_per_pixel: 100
  max_depth: 10
  output: three_spheres.ppm

materials:
  - name: ground
    type: lambertian
    albedo: [0.5, 0.5, 0.5]
  - name: center_matte
    type: lambertian
    albedo: [0.7, 0.3, 0.3]
  - name: left_glass
    type: dielectric
    ior: 1.5
  - name: right_metal
    type: metal
    albedo: [0.8, 0.6, 0.2]
    fuzziness: 0.1

objects:
  - type: plane
    point: [0, -0.5, 0]
    normal: [0, 1, 0]
    material: ground
  - type: sphere
    center: [0, 0, 0]
    radius: 0.5
    material: center_matte
  - type: sphere
    center: [-1, 0, 0]
    radius: 0.5
    material: left_glass
  - type: sphere
    center: [1, 0, 0]
    radius: 0.5
    material: right_metal

lights:
  - type: point
    position: [-2, 3, -1]
    color: [1, 1, 1]
    intensity: 1.0
```

##### Example 2: Elena makes an error in the scene file
Elena misspells a material name: `material: glas` instead of `material: left_glass`. The renderer outputs: `Error: scene.yaml line 35: object 'sphere' references undefined material 'glas'. Available materials: ground, center_matte, left_glass, right_metal` and exits without rendering.

##### Example 3: Elena runs with no arguments
Elena runs `./raytracer` with no command-line argument. The renderer outputs usage help: `Usage: raytracer <scene.yaml>` and exits.

#### UAT Scenarios (BDD)

##### Scenario: Valid YAML file produces correct render
```
Given Elena has a valid YAML scene file "scene.yaml" with the three-spheres setup
When Elena runs ./raytracer scene.yaml
Then the renderer loads the scene without errors
  And produces an image "three_spheres.ppm" as specified in the render section
  And the image shows three spheres (matte red, glass, gold metal) on a gray ground plane
```

##### Scenario: Undefined material reference produces clear error
```
Given Elena has a YAML file where a sphere references material "crystal" which is not defined
When Elena runs ./raytracer scene.yaml
Then the renderer outputs an error message containing "undefined material 'crystal'"
  And the error message lists the available material names
  And no image file is produced
```

##### Scenario: Missing required field produces clear error
```
Given Elena has a YAML file where a sphere object is missing the "radius" field
When Elena runs ./raytracer scene.yaml
Then the renderer outputs an error message containing "sphere missing required field 'radius'"
  And no image file is produced
```

##### Scenario: Empty scene file produces background-only image
```
Given Elena has a YAML file with camera and render settings but no objects or lights
When Elena runs ./raytracer scene.yaml
Then the renderer produces a valid image showing only the background color
```

#### Acceptance Criteria
- [ ] The renderer accepts a YAML file path as a command-line argument
- [ ] Camera, lights, materials, objects, and render settings are parsed from the YAML file
- [ ] All currently supported primitives and materials are loadable from YAML
- [ ] Undefined material references produce clear error messages with available material names
- [ ] Missing required fields produce clear error messages identifying the field and object
- [ ] The scene file format is documented in a comment/header within the scene file itself
- [ ] Running with no arguments prints usage instructions

#### Technical Notes
- Use a YAML parsing library (e.g., yaml-cpp)
- Scene file parser is a separate module from the rendering engine
- Validate all references (materials, etc.) before starting the render
- Support both inline material definitions and shared material references by name

---

## Feature 10: Advanced Features

### US-1001: Area Lights with Soft Shadows

#### Problem (The Pain)
David Okonkwo's rendered scenes have sharp, hard-edged shadows that look artificial. Real-world light sources have physical size (a lamp, a window, the sun), which produces soft shadows with gradual penumbras. He needs a light source with area so that shadows blend smoothly from fully lit to fully shadowed.

#### Who (The User)
- Hobbyist 3D artist seeking realistic shadow quality
- Wants soft shadows without manually configuring dozens of point lights
- Willing to accept longer render times for better quality

#### Solution (What We Build)
An area light defined by a rectangular or disc shape, position, and emission. Multiple shadow rays are cast to random points on the light surface; the fraction of unoccluded rays determines shadow softness.

#### Domain Examples

##### Example 1: David renders a scene with a rectangular ceiling light
David places a rectangular area light (2m x 1m) on the ceiling above a sphere on a table. The sphere casts a shadow with a sharp umbra (fully shadowed) directly beneath it and a soft penumbra (gradually fading) around the edges. The shadow is not perfectly circular -- it is softened proportionally to the light's size.

##### Example 2: David compares point light vs. area light shadows
David renders the same scene with a point light at the center of the area light's position, and then with the area light. The point light version has a perfectly sharp shadow boundary. The area light version has the same shadow center but with a gradual soft edge.

##### Example 3: David renders with a very small area light
David defines an area light with dimensions 0.01 x 0.01 (nearly a point). The shadow is almost identical to a point light shadow, confirming that area light degenerates to point light behavior at small sizes.

#### UAT Scenarios (BDD)

##### Scenario: Area light produces soft shadow penumbra
```
Given David has a sphere on a ground plane
  And a rectangular area light (width 2.0, height 1.0) centered at (0, 4, 0) facing downward
  And the renderer casts 64 shadow samples per light per hit point
When David renders the scene at 100 SPP
Then the sphere's shadow has a sharp inner region (umbra, < 10% lit)
  And the shadow has a gradual outer region (penumbra, 10-90% lit) surrounding the umbra
  And outside the penumbra, the ground is fully lit (> 90% lit)
```

##### Scenario: More shadow samples produce smoother penumbra
```
Given David renders a scene with an area light using 4 shadow samples
  And then renders the same scene with 64 shadow samples
When David compares the shadow regions
Then the 64-sample version has a smoother penumbra (less noise in the transition region)
  And the 4-sample version shows noticeable noise in the penumbra
```

##### Scenario: Area light with near-zero size matches point light
```
Given David has an area light with dimensions 0.001 x 0.001
  And a point light at the same position
When David renders both scenes
Then the shadow shapes are nearly identical
  And both show hard-edged shadows
```

#### Acceptance Criteria
- [ ] Area light is defined by position, dimensions (width, height), normal direction, color, and intensity
- [ ] Multiple shadow rays (configurable count) are cast to random points on the light surface
- [ ] The fraction of unoccluded shadow rays determines the illumination contribution (soft shadows)
- [ ] Area light produces visible penumbra around shadows
- [ ] More shadow samples produce smoother penumbra with less noise
- [ ] Area light with near-zero size behaves like a point light

#### Technical Notes
- Random point on rectangular light: light_pos + random(-w/2, w/2) * u_axis + random(-h/2, h/2) * v_axis
- Shadow softness = unoccluded_rays / total_shadow_rays
- Shadow samples per light is separate from pixel SPP
- Area lights significantly increase render time (N shadow rays per light per hit point per pixel sample)

---

### US-1002: Gamma Correction and Tone Mapping

#### Problem (The Pain)
Elena Marchetti's rendered images look too dark in the midtones even though the math is correct. This is because her monitor expects gamma-corrected values but the renderer outputs linear-space colors. She also encounters pixel values exceeding 1.0 in bright areas (specular highlights, light sources) that get clamped to white, losing all detail in bright regions.

#### Who (The User)
- CG student learning about color spaces
- Wants rendered images that look correct on a standard monitor
- Needs to handle HDR scenes without losing bright detail

#### Solution (What We Build)
Gamma correction (gamma 2.0: output = sqrt(linear_color)) applied to all pixel colors before writing. Optional Reinhard tone mapping for HDR scenes.

#### Domain Examples

##### Example 1: Elena compares linear vs. gamma-corrected output
Elena renders the same scene and saves two images: one with linear colors (no gamma) and one with gamma correction (sqrt). The linear image appears much darker overall. The gamma-corrected image matches her visual expectation of the scene.

##### Example 2: Elena renders a scene with a very bright specular highlight
Elena's scene has a metal sphere with a bright specular highlight (color value 3.5 in linear space). Without tone mapping, the highlight is clamped to (255, 255, 255) and the area around it also clamps, creating a large white blob. With Reinhard tone mapping (color / (1 + color)), the highlight is compressed to a bright but detailed region.

##### Example 3: Elena renders a dark scene
Elena renders a scene with dim lighting where most color values are below 0.1 in linear space. Gamma correction (sqrt) brightens these values, making shadow detail visible that would otherwise be lost in near-black pixels.

#### UAT Scenarios (BDD)

##### Scenario: Gamma correction brightens midtones
```
Given Elena renders a scene with a gray sphere (albedo 0.5, 0.5, 0.5) under uniform lighting
When she outputs without gamma correction
Then the center pixel of the sphere has approximate value (128, 128, 128) or darker in the PPM
When she outputs with gamma 2.0 correction (sqrt)
Then the center pixel has approximate value (181, 181, 181) in the PPM -- visibly brighter midtones
```

##### Scenario: Color values are clamped to [0, 255] after gamma
```
Given Elena renders a scene with very bright areas (linear color > 1.0)
When gamma correction and clamping are applied
Then no pixel value in the PPM file is less than 0 or greater than 255
  And the bright areas are clamped to 255 (unless tone mapping is applied first)
```

##### Scenario: Reinhard tone mapping preserves highlight detail
```
Given Elena has a scene with specular highlights reaching linear value 5.0
When she applies Reinhard tone mapping followed by gamma correction
Then the brightest pixels are less than 255 (compressed, not clamped)
  And highlight detail is visible (gradual brightness falloff rather than flat white)
```

#### Acceptance Criteria
- [ ] Gamma 2.0 correction (sqrt(color)) is applied to all pixel colors by default
- [ ] RGB values are clamped to [0, 255] after gamma correction before writing to PPM
- [ ] Gamma correction visibly brightens midtones compared to linear output
- [ ] Optional Reinhard tone mapping (color / (1 + color)) can be applied before gamma correction
- [ ] No NaN or negative values pass through to the output (defensive clamping)

#### Technical Notes
- Gamma correction: output = sqrt(linear_color) per channel
- Clamp: clamp(0.0, 1.0, channel) then multiply by 255 and convert to int
- Reinhard tone mapping: mapped = color / (1.0 + color) per channel, applied before gamma
- NaN check: if (color != color) color = 0 (NaN guard)
