//! The road ("weg", weg.cpp, wegtools.cpp). A track file (ETAPPE1..3.TRK) is a list of
//! centre points; each pair of consecutive points is a chunk. A section sits at each
//! point: the road's centre, its right and left edges ((lanes + 2) * 30 out), a guard
//! rail at each edge and a median at 5 either side of the centre (both 5 high), and a
//! strip of 60-wide ground tiles on each side. The ground is flat at height 0; only its
//! first row follows the road, so a raised road slopes down to it.

use crate::race::math3d::{normalize_vector, cross_product};
use crate::race::object3d::Vertex3d;
use crate::race::render::Mesh;
use crate::race::Race;

#[derive(Clone, Default)]
pub struct WegSection {
    /// The centre (vertex index).
    pub point: usize,
    /// centre + width * side
    pub edge_right: usize,
    pub edge_left: usize,
    /// Foot and top of the guard rail at the left edge.
    pub rail_left: [usize; 2],
    /// centre - 5 * side
    pub median_left: [usize; 2],
    pub rail_right: [usize; 2],
    pub median_right: [usize; 2],
    /// ground_count vertices from the left edge outwards.
    pub ground_left: Vec<usize>,
    pub ground_right: Vec<usize>,
    pub ground_count: usize,
    /// 0..1023
    pub heading: i32,
    /// height = -(a*x + c*z + d) / b
    pub plane_a: f32,
    pub plane_b: f32,
    pub plane_c: f32,
    pub plane_d: f32,
}

#[derive(Clone, Default)]
pub struct WegChunk {
    /// 2
    pub lanes: i32,
    /// Index into weg_sections.
    pub section: usize,
    /// Vertex indices of the two centre points.
    pub start: usize,
    pub end: usize,
    pub in_use: bool,
    /// In view this frame (weg_chunk_vertex_lists).
    pub visible: bool,
    /// Unit vector to the right, across the road.
    pub side: [f32; 3],
    /// Unit vector along the road.
    pub dir: [f32; 3],
    /// The execute buffer in the original.
    pub mesh: Option<Mesh>,
}

#[derive(Default)]
pub struct Weg {
    pub sections: Vec<WegSection>,
    pub chunks: Vec<WegChunk>,
    /// Chunk index per track point.
    pub chunk_quads: Vec<usize>,
    pub chunk_count: i32,
    /// Chunks allocated (weg_section_count).
    pub section_count: i32,
    lane_chunks: usize,
    pub road_texture: i32,
    pub grass_texture: i32,
    visible_first: usize,
    visible_last: usize,
    /// 8 x 8 tiles of 1/8 in the grass texture (init_tiles): each tile's four corners.
    tile_u: Vec<[f32; 4]>,
    tile_v: Vec<[f32; 4]>,
    pub loaded: bool,
}

impl Weg {
    fn init_tiles(&mut self) {
        self.tile_u = vec![[0.0; 4]; 64];
        self.tile_v = vec![[0.0; 4]; 64];
        for j in 0..8 {
            for k in 0..8 {
                let i = j * 8 + k;
                let (u, v) = (k as f32 * 0.125, j as f32 * 0.125);
                self.tile_u[i] = [u, u, u + 0.125, u + 0.125];
                self.tile_v[i] = [v + 0.125, v, v, v + 0.125];
            }
        }
    }

    /// The road's height at (x, z) on a chunk's plane.
    pub fn road_height_at(&self, chunk: i32, x: f32, z: f32) -> f32 {
        if chunk == -1 {
            return 0.0;
        }
        let s = &self.sections[chunk as usize];
        -((s.plane_c * z + s.plane_a * x + s.plane_d) / s.plane_b)
    }

    /// Which chunks to draw: [first, last).
    pub fn set_visible_weg_chunks(&mut self, first: i32, last: i32) {
        let last = if first < 0 { 0 } else { last };
        self.visible_first = first.max(0) as usize;
        self.visible_last = last.max(0) as usize;
    }
}

/// The heading of the line (x0, z0) to (x1, z1) in 0..1023 units (interpolate_weg_point).
pub fn interpolate_weg_point(x0: f32, z0: f32, x1: f32, z1: f32) -> i32 {
    let a = if x0 == x1 {
        if z0 <= z1 { std::f64::consts::FRAC_PI_2 } else { -std::f64::consts::FRAC_PI_2 }
    } else {
        ((z1 - z0) as f64).atan2((x1 - x0) as f64)
    };
    ((a as f32) as f64 * 1024.0 / 6.283185307) as i32
}

/// One point of the track file: x, z, a word, the height, then two lists of `n` tile
/// numbers (left and right ground).
struct TrackReader<'a> {
    data: &'a [u8],
    pos: usize,
}

struct TrackPoint {
    x: f32,
    z: f32,
    y: f32,
    tex_left: Vec<i32>,
    tex_right: Vec<i32>,
}

impl TrackReader<'_> {
    fn read_point(&mut self) -> Option<TrackPoint> {
        let p = self.data.get(self.pos..self.pos + 20)?;
        let i32_at = |q: &[u8], o: usize| i32::from_le_bytes([q[o], q[o + 1], q[o + 2], q[o + 3]]);
        let x = f32::from_le_bytes([p[0], p[1], p[2], p[3]]);
        let z = f32::from_le_bytes([p[4], p[5], p[6], p[7]]);
        let y = i32_at(p, 12);
        let count = i32_at(p, 16);
        self.pos += 20;
        if !(0..=64).contains(&count) {
            return None;
        }
        let n = count as usize;
        let lists = self.data.get(self.pos..self.pos + n * 8)?;
        let tex_left = (0..n).map(|i| i32_at(lists, i * 4)).collect();
        let tex_right = (0..n).map(|i| i32_at(lists, (n + i) * 4)).collect();
        self.pos += n * 8;
        Some(TrackPoint { x, z, y: y as f32, tex_left, tex_right })
    }
}

/// The first 32 bytes of a vertex (all but its in-use flag), as the original's memcpys.
fn copy_vertex(to: &mut Vertex3d, from: &Vertex3d) {
    let in_use = to.in_use;
    *to = *from;
    to.in_use = in_use;
}

impl Race {
    fn new_vertex(&mut self) -> usize {
        let v = self.pools.get_new_vertices();
        self.pools.vertices3d[v].in_use = 1;
        v
    }

    /// A section with `ground_count` ground vertices per side (alloc_weg_lanes).
    fn alloc_weg_lanes(&mut self, ground_count: usize) -> usize {
        if self.weg.lane_chunks + 1 >= self.weg.sections.len() {
            panic!("not enough weg chunks ({})", self.weg.sections.len());
        }
        let ground_left = (0..ground_count).map(|_| self.new_vertex()).collect();
        let ground_right = (0..ground_count).map(|_| self.new_vertex()).collect();
        let point = self.new_vertex();
        let edge_right = self.new_vertex();
        let edge_left = self.new_vertex();
        let mut s = WegSection { ground_left, ground_right, ground_count, point, edge_right, edge_left, ..Default::default() };
        for k in 0..2 {
            s.rail_right[k] = self.new_vertex();
            s.rail_left[k] = self.new_vertex();
            s.median_right[k] = self.new_vertex();
            s.median_left[k] = self.new_vertex();
        }
        let i = self.weg.lane_chunks;
        self.weg.sections[i] = s;
        self.weg.lane_chunks += 1;
        i
    }

    /// Builds the chunk from centre point a to b (build_weg_sections); its index.
    fn build_weg_sections(&mut self, a: usize, b: usize, ground_count: usize, lanes: i32) -> usize {
        if self.weg.chunk_count + 1 >= self.weg.section_count {
            panic!("not enough weg elements ({})", self.weg.section_count);
        }
        let c = self.weg.chunk_count as usize;
        self.weg.chunk_count += 1;
        let (pa, pb) = (self.pools.vertices3d[a], self.pools.vertices3d[b]);
        let section = self.alloc_weg_lanes(ground_count);
        self.weg.chunks[c] = WegChunk { start: a, end: b, lanes, in_use: true, visible: true, section, ..Default::default() };

        let mut dir = [pb.x - pa.x, pb.y - pa.y, pb.z - pa.z];
        let heading = interpolate_weg_point(pa.x, pa.z, pb.x, pb.z);
        normalize_vector(&mut dir);
        // The road's normal: straight up, made perpendicular to the direction.
        let d = dir[2] * 0.0 + dir[0] * 0.0 + dir[1];
        let mut normal = [0.0 - dir[0] * d, 1.0 - dir[1] * d, 0.0 - d * dir[2]];
        normalize_vector(&mut normal);
        let mut side = cross_product(&normal, &dir);
        normalize_vector(&mut side);
        {
            let s = &mut self.weg.sections[section];
            s.heading = heading;
            s.plane_a = normal[0];
            s.plane_b = normal[1];
            s.plane_c = normal[2];
            s.plane_d = -(s.plane_c * pa.z + s.plane_a * pa.x + s.plane_b * pa.y);
            if s.plane_b == 0.0 {
                s.plane_b = 0.1;
            }
        }
        self.weg.chunks[c].side = side;
        self.weg.chunks[c].dir = dir;

        let s = self.weg.sections[section].clone();
        let vs = &mut self.pools.vertices3d;
        let set = |v: &mut Vertex3d, x: f32, y: f32, z: f32| {
            v.x = x;
            v.y = y;
            v.z = z;
        };
        let set_normal = |v: &mut Vertex3d, n: &[f32; 3]| {
            v.nx = n[0];
            v.ny = n[1];
            v.nz = n[2];
        };
        set(&mut vs[s.point], pa.x, pa.y, pa.z);
        set_normal(&mut vs[s.point], &normal);
        vs[s.point].tu = 0.0;
        vs[s.point].tv = 0.0;
        let w = (lanes as f32 + 2.0) * 30.0;
        set(&mut vs[s.edge_right], w * side[0] + pa.x, pa.y, w * side[2] + pa.z);
        set_normal(&mut vs[s.edge_right], &normal);
        vs[s.edge_right].tu = 1.0;
        vs[s.edge_right].tv = 0.0;
        set(&mut vs[s.edge_left], pa.x - w * side[0], pa.y, pa.z - w * side[2]);
        set_normal(&mut vs[s.edge_left], &normal);
        vs[s.edge_left].tu = 1.0;
        vs[s.edge_left].tv = 0.0;

        // Guard rails at the edges and the median by the centre, 5 high along the
        // normal. The original's order of operations is kept: the top of the right
        // median gets its x and z from the centre and the side only.
        let (left, right, centre) = (vs[s.edge_left], vs[s.edge_right], vs[s.point]);
        let up5 = |v: &mut Vertex3d| {
            v.x += normal[0] * 5.0;
            v.y += normal[1] * 5.0;
            v.z += normal[2] * 5.0;
        };
        for (k, from) in [(0, left), (1, left)] {
            let r = &mut vs[s.rail_left[k]];
            copy_vertex(r, &from);
            if k == 1 {
                up5(r);
            }
            set_normal(r, &side);
        }
        for k in 0..2 {
            let r = &mut vs[s.rail_right[k]];
            copy_vertex(r, &right);
            if k == 1 {
                up5(r);
            }
            set_normal(r, &side);
        }
        for k in 0..2 {
            let r = &mut vs[s.median_left[k]];
            copy_vertex(r, &centre);
            r.x = pa.x - side[0] * 5.0;
            r.z = pa.z - side[2] * 5.0;
            if k == 1 {
                up5(r);
            }
            set_normal(r, &side);
        }
        {
            let r = &mut vs[s.median_right[0]];
            copy_vertex(r, &centre);
            r.x = side[0] * 5.0 + pa.x;
            r.z = side[2] * 5.0 + pa.z;
            set_normal(r, &side);
            let r = &mut vs[s.median_right[1]];
            copy_vertex(r, &centre);
            up5(r);
            r.x = side[0] * 5.0 + pa.x;
            r.z = side[2] * 5.0 + pa.z;
            set_normal(r, &side);
        }

        // The ground: flat at 0 beyond its first row, which follows the road's edge.
        let up = [0.0, 1.0, 0.0];
        let (mut x, mut z) = (left.x, left.z);
        for (j, &g) in s.ground_left.iter().enumerate() {
            let r = &mut vs[g];
            r.x = x;
            r.z = z;
            x -= side[0] * 60.0;
            z -= side[2] * 60.0;
            r.y = if j == 0 { pa.y } else { 0.0 };
            set_normal(r, &up);
        }
        let (mut x, mut z) = (right.x, right.z);
        for (j, &g) in s.ground_right.iter().enumerate() {
            let r = &mut vs[g];
            r.x = x;
            r.z = z;
            x += side[0] * 60.0;
            z += side[2] * 60.0;
            r.y = if j == 0 { pa.y } else { 0.0 };
            set_normal(r, &up);
        }
        c
    }

    /// The chunk's mesh (build_weg_execute_buffers): `side_length` ground vertices per
    /// side are used, tile textures from the track file.
    fn build_weg_execute_buffers(&mut self, chunk: usize, side_length: usize, tex_left: &[i32], tex_right: &[i32]) -> Mesh {
        if side_length < 2 {
            panic!("weg: illegal side length {side_length}");
        }
        let w = &self.weg;
        let s0 = w.sections[w.chunks[w.chunk_quads[chunk]].section].clone();
        let s1 = w.sections[w.chunks[w.chunk_quads[chunk + 1]].section].clone();
        let (tu, tv) = (w.tile_u.clone(), w.tile_v.clone());
        let tiles = side_length - 1;
        let mut m = Mesh::new(side_length * 8 + 0x10, 4 + 16 + tiles * 4);
        let mut slot = 0;
        let vs = &mut self.pools.vertices3d;
        let mut put = |m: &mut Mesh, vertex: usize, u: f32, v: f32| {
            vs[vertex].tu = u;
            vs[vertex].tv = v;
            m.set_vertex(slot, &vs[vertex]);
            slot += 1;
        };
        // The road, centre to edge on each side, the texture once across each half.
        put(&mut m, s0.point, 0.0, 0.0);
        put(&mut m, s0.edge_right, 1.0, 0.0);
        put(&mut m, s1.point, 0.0, 1.0);
        put(&mut m, s1.edge_right, 1.0, 1.0);
        put(&mut m, s0.point, 0.0, 0.0);
        put(&mut m, s0.edge_left, 1.0, 0.0);
        put(&mut m, s1.point, 0.0, 1.0);
        put(&mut m, s1.edge_left, 1.0, 1.0);
        // A side's guard rail or median: the grass texture's top row, stretched along the
        // chunk; both faces are drawn.
        for (a, b) in [(s0.rail_left, s1.rail_left), (s0.median_left, s1.median_left), (s0.rail_right, s1.rail_right),
            (s0.median_right, s1.median_right)]
        {
            put(&mut m, a[0], tu[0][0], tv[0][0]);
            put(&mut m, a[1], tu[0][1], tv[0][1]);
            put(&mut m, b[0], tu[7][3], tv[7][3]);
            put(&mut m, b[1], tu[7][2], tv[7][2]);
        }
        for t in 0..tiles {
            let tile = (tex_left[t] & 63) as usize;
            put(&mut m, s0.ground_left[t + 1], tu[tile][0], tv[tile][0]);
            put(&mut m, s0.ground_left[t], tu[tile][1], tv[tile][1]);
            put(&mut m, s1.ground_left[t + 1], tu[tile][3], tv[tile][3]);
            put(&mut m, s1.ground_left[t], tu[tile][2], tv[tile][2]);
        }
        for t in 0..tiles {
            let tile = (tex_right[t] & 63) as usize;
            put(&mut m, s0.ground_right[t], tu[tile][0], tv[tile][0]);
            put(&mut m, s0.ground_right[t + 1], tu[tile][1], tv[tile][1]);
            put(&mut m, s1.ground_right[t], tu[tile][3], tv[tile][3]);
            put(&mut m, s1.ground_right[t + 1], tu[tile][2], tv[tile][2]);
        }
        let used = slot;
        m.texture(self.weg.road_texture);
        m.triangle(0, 2, 3);
        m.triangle(0, 3, 1);
        m.triangle(4, 7, 6);
        m.triangle(4, 5, 7);
        m.texture(self.weg.grass_texture);
        for b in (8..24).step_by(4) {
            m.triangle(b, b + 2, b + 3);
            m.triangle(b, b + 3, b + 1);
            m.triangle(b + 3, b + 2, b);
            m.triangle(b + 1, b + 3, b);
        }
        for j in (24..used).step_by(4) {
            m.triangle(j, j + 2, j + 3);
            m.triangle(j, j + 3, j + 1);
        }
        m
    }

    /// Loads a track; `max_side` limits the ground strips (20 in the original).
    pub fn load_weg_data(&mut self, filename: &str, mut max_side: i32) -> bool {
        let Some(data) = self.files.race_disc_file(filename).filter(|d| d.len() >= 4) else {
            eprintln!("could not open weg data {filename}");
            return false;
        };
        let count = i32::from_le_bytes([data[0], data[1], data[2], data[3]]).max(0);
        self.weg.init_tiles();
        self.weg.sections = vec![WegSection::default(); count as usize];
        self.weg.lane_chunks = 0;
        if count <= max_side {
            max_side = ((count - 1) / 2) * 2;
        }
        let _ = max_side;
        self.weg.chunks = vec![WegChunk::default(); count as usize];
        self.weg.chunk_quads = vec![0; count as usize];
        self.weg.section_count = count;
        self.weg.chunk_count = 0;

        let (a, b) = (self.new_vertex(), self.new_vertex());
        let mut r = TrackReader { data: &data, pos: 4 };
        let bad = || {
            eprintln!("error reading weg data {filename}");
            false
        };
        let Some(p) = r.read_point() else { return bad() };
        self.set_point(a, &p);
        for i in 0..(count - 1).max(0) as usize {
            let Some(p) = r.read_point() else { return bad() };
            self.set_point(b, &p);
            self.weg.chunk_quads[i] = self.build_weg_sections(a, b, p.tex_left.len(), 2);
            let vb = self.pools.vertices3d[b];
            let va = &mut self.pools.vertices3d[a];
            va.x = vb.x;
            va.y = vb.y;
            va.z = vb.z;
        }

        // Second pass: the meshes, with each chunk's tiles from the point after it. The
        // original reads WEG/COMPLETE from d3d_lib.ini; without it half the ground strip
        // is drawn. The last chunk would join the first section (its next entry in
        // weg_chunk_quads is 0), so it gets no mesh.
        let mut r = TrackReader { data: &data, pos: 4 };
        r.read_point();
        for i in 0..(count - 1).max(0) as usize {
            let Some(p) = r.read_point() else { break };
            if i + 1 < (count - 1) as usize {
                let mesh = self.build_weg_execute_buffers(i, p.tex_left.len() / 2, &p.tex_left, &p.tex_right);
                let q = self.weg.chunk_quads[i];
                self.weg.chunks[q].mesh = Some(mesh);
            }
        }
        self.weg.loaded = true;
        self.weg.visible_first = 0;
        self.weg.visible_last = self.weg.chunk_count as usize;
        true
    }

    fn set_point(&mut self, v: usize, p: &TrackPoint) {
        let v = &mut self.pools.vertices3d[v];
        v.x = p.x;
        v.z = p.z;
        v.y = p.y;
    }

    pub fn free_weg_data(&mut self) {
        self.weg = Weg::default();
    }

    /// execute_weg_buffer: the road, drawn after the objects.
    pub fn draw_weg(&self) {
        if !self.weg.loaded {
            return;
        }
        let w = &self.weg;
        for ch in w.chunks.iter().take(w.visible_last.min(w.chunk_count as usize)).skip(w.visible_first) {
            if ch.visible {
                if let Some(m) = &ch.mesh {
                    self.render.mesh_draw(m, None);
                }
            }
        }
    }

    // --- wegtools.cpp ------------------------------------------------------------------

    /// Which side of the line across the road at a section (right edge to left edge) the
    /// point is on, in whole units as the original (whose overflow on far-off points
    /// wraps).
    fn point_side_of_chunk_edge(&self, section: usize, x: f32, z: f32) -> i32 {
        let s = &self.weg.sections[section];
        let vs = &self.pools.vertices3d;
        let (x1, z1) = (vs[s.edge_right].x as i32, vs[s.edge_right].z as i32);
        let (x2, z2) = (vs[s.edge_left].x as i32, vs[s.edge_left].z as i32);
        let d = (x as i32).wrapping_sub(x1).wrapping_mul(z2.wrapping_sub(z1))
            .wrapping_sub(x2.wrapping_sub(x1).wrapping_mul((z as i32).wrapping_sub(z1)));
        d.signum()
    }

    fn point_between_chunk_edges(&self, chunk: usize, x: f32, z: f32) -> bool {
        self.point_side_of_chunk_edge(chunk, x, z) != self.point_side_of_chunk_edge(chunk + 1, x, z)
    }

    /// The chunk containing (x, z), searched outwards from `chunk`.
    pub fn update_chunk_for_position(&self, chunk: i32, x: f32, z: f32) -> i32 {
        if chunk < 0 || self.weg.sections.len() as i32 <= chunk {
            panic!("chunk {chunk} out of array bounds ({})", self.weg.sections.len());
        }
        // Outwards from the chunk: chunk, chunk + 1, chunk - 1, chunk + 2, ... The last
        // section's neighbour does not exist, so the search stays below it.
        let (mut down, mut up) = (chunk, chunk + 1);
        let n = self.weg.chunk_count;
        while down > 0 || up < n {
            if down > 0 {
                let q = self.weg.chunk_quads[down as usize];
                if self.point_between_chunk_edges(q, x, z) {
                    return q as i32;
                }
                down -= 1;
            }
            if up < n {
                let q = self.weg.chunk_quads[up as usize];
                if self.point_between_chunk_edges(q, x, z) {
                    return q as i32;
                }
                up += 1;
            }
        }
        eprintln!("weg chunk is -1");
        chunk
    }

    /// The road's height at (x, z) on a chunk's plane.
    pub fn road_height_at(&self, chunk: i32, x: f32, z: f32) -> f32 {
        self.weg.road_height_at(chunk, x, z)
    }
}
