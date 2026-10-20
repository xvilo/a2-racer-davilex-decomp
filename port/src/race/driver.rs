//! Who drives the other cars (driver.cpp). Each car has a driver that picks its lane and
//! speed every frame from what is on the road map ahead and behind:
//!   civilians keep to a lane and overtake slower cars;
//!   rivals take the lane with the most room, preferring their own lane;
//!   police park on the hard shoulder, chase a racer that speeds past (over 9),
//!   overtake and brake in front of it: catching the player means a fine.

use crate::race::{Race, RACE_FINE, RACE_RUNNING};

pub const DRIVER_CIVILIAN: i32 = 2;
pub const DRIVER_RIVAL: i32 = 3;
pub const DRIVER_POLICE: i32 = 4;

const MAP_LANES: usize = 11;

#[derive(Clone, Copy, Default)]
pub struct Driver {
    /// The car.
    pub opponent: usize,
    /// The lane the driver prefers.
    pub lane: i32,
    /// The speed the driver prefers (an int in the original's float slot).
    pub speed: f32,
    pub assigned: bool,
    /// DRIVER_*.
    pub kind: i32,
    /// The rival's or police car's own number.
    pub index: i32,
    pub used: bool,
}

/// A rival's state (driver_lane_data).
#[derive(Clone, Copy, Default)]
pub struct RivalState {
    /// 4: finished, slowing down to a stop.
    pub state: i32,
    /// The police car that stopped it, -1.
    pub stopped_by: i32,
    /// Frames left slowed down, or the speed after the finish.
    pub counter: i32,
}

/// A police car's state (driver_path_data).
#[derive(Clone, Copy, Default)]
pub struct PoliceState {
    /// 0 parked, 1 chasing, 2 going back.
    pub state: i32,
    /// The racer chased.
    pub target: i32,
    /// The racer seen was speeding.
    pub fast: bool,
    pub counter: i32,
    /// Chase speed.
    pub speed: i32,
    pub lane: i32,
}

pub struct Drivers {
    pub driver_list: Vec<Driver>,
    pub rival_states: Vec<RivalState>,
    police_states: Vec<PoliceState>,
    police_state_count: usize,
    pub racer_high_speed: f32,
    pub racer_low_speed: f32,
    pub racer_range: i32,
    /// The road map: which car is on which chunk and lane.
    opponent_map: Vec<[i32; MAP_LANES + 1]>,
}

impl Default for Drivers {
    fn default() -> Drivers {
        Drivers {
            driver_list: Vec::new(),
            rival_states: Vec::new(),
            police_states: Vec::new(),
            police_state_count: 0,
            racer_high_speed: 16.0,
            racer_low_speed: 13.0,
            racer_range: 2,
            opponent_map: Vec::new(),
        }
    }
}

/// The car ahead in a lane and whether there is one close behind, per lane.
struct Scan {
    front: [i32; MAP_LANES],
    back: [i32; MAP_LANES],
}

impl Race {
    // --- the road map ------------------------------------------------------------------

    pub fn init_opponent_map(&mut self) {
        self.drivers.opponent_map = vec![[-1; MAP_LANES + 1]; self.weg.chunk_count.max(0) as usize];
    }

    fn map_ok(&self, chunk: i32, lane: i32) -> bool {
        chunk >= 0 && (chunk as usize) < self.drivers.opponent_map.len() && (0..=MAP_LANES as i32).contains(&lane)
    }

    pub fn set_map_occupant(&mut self, opponent: i32, chunk: i32, lane: i32) {
        if self.map_ok(chunk, lane) {
            self.drivers.opponent_map[chunk as usize][lane as usize] = opponent;
        }
    }

    pub fn get_map_occupant(&self, chunk: i32, lane: i32) -> i32 {
        if self.map_ok(chunk, lane) { self.drivers.opponent_map[chunk as usize][lane as usize] } else { -1 }
    }

    pub fn clear_map_occupant(&mut self, chunk: i32, lane: i32) {
        self.set_map_occupant(-1, chunk, lane);
    }

    // --- drivers -----------------------------------------------------------------------

    pub fn init_drivers(&mut self, count: usize, rivals: usize, police: usize) {
        let rival_states = (0..rivals).map(|_| RivalState { state: self.rand() % 2, stopped_by: -1, counter: 0 }).collect();
        let d = &mut self.drivers;
        d.driver_list = vec![Driver::default(); count];
        d.rival_states = rival_states;
        d.police_states = vec![PoliceState::default(); police + 1];
        d.police_state_count = police;
    }

    pub fn get_new_driver(&mut self) -> usize {
        let d = self.drivers.driver_list.iter_mut().position(|d| !d.used).unwrap_or_else(|| panic!("not enough drivers available"));
        self.drivers.driver_list[d].used = true;
        d
    }

    pub fn assign_driver_to_opponent(&mut self, driver: usize, opponent: usize, kind: i32) {
        let o = &self.opp.data[opponent];
        let (direction, max_speed) = (o.direction, o.max_speed);
        let d = &mut self.drivers.driver_list[driver];
        if !d.used || d.assigned {
            panic!("cannot assign driver {driver} to opponent {opponent}");
        }
        d.opponent = opponent;
        // The lane a driver keeps to: the second (rivals) or third lane out.
        d.lane = match (direction == 1, kind == DRIVER_RIVAL) {
            (true, true) => 1,
            (true, false) => 2,
            (false, true) => 9,
            (false, false) => 8,
        };
        d.speed = max_speed;
        d.assigned = true;
        d.kind = kind;
        d.index = match kind {
            DRIVER_CIVILIAN => -1,
            DRIVER_RIVAL => opponent as i32 - 1,
            DRIVER_POLICE => opponent as i32 - 6,
            _ => d.index,
        };
    }

    fn wrap_chunk(&self, mut c: i32) -> i32 {
        let n = self.weg.chunk_count;
        while c < 0 {
            c += n;
        }
        while n <= c {
            c -= n;
        }
        c
    }

    /// scan_lanes_ahead: per lane the first car `ahead` chunks forward and `behind` back.
    fn scan_lanes_ahead(&self, me: usize, chunk: i32, ahead: i32, behind: i32, direction: i32) -> Scan {
        let step = if direction == 0 { -1 } else { 1 };
        let mut s = Scan { front: [-1; MAP_LANES], back: [-1; MAP_LANES] };
        let me = me as i32;
        for (list, dir, n) in [(&mut s.front, step, ahead), (&mut s.back, -step, behind)] {
            let mut c = chunk;
            for _ in 0..=n {
                for (l, slot) in list.iter_mut().enumerate() {
                    if *slot == -1 {
                        *slot = self.get_map_occupant(self.wrap_chunk(c), l as i32);
                        if *slot == me {
                            *slot = -1;
                        }
                    }
                }
                c += dir;
            }
        }
        s
    }

    /// scan_lanes_for_heading: which racers are on this side of the road near the chunk.
    fn scan_lanes_for_heading(&self, chunk: i32, range: i32, direction: i32) -> [bool; 6] {
        let step = if direction == 0 { -1 } else { 1 };
        let (first, last) = if direction == 0 { (6, 10) } else { (0, 4) };
        let mut seen = [false; 6];
        let mut c = chunk;
        for _ in 0..=range {
            for l in first..=last {
                let o = self.get_map_occupant(self.wrap_chunk(c), l);
                if (0..6).contains(&o) {
                    seen[o as usize] = true;
                }
            }
            c += step;
        }
        seen
    }

    /// racer_distance_margin: squared distance, less the radii (not squared: as the
    /// original).
    fn racer_distance_margin(&self, a: usize, b: usize) -> f32 {
        let (a, b) = (&self.opp.data[a], &self.opp.data[b]);
        let (dx, dz) = (a.x - b.x, a.z - b.z);
        dx * dx + dz * dz - (a.radius + b.radius)
    }

    /// How good a lane is: 0 someone close behind in it, 1 a car ahead within 240, 2
    /// within 320, 3 further, 4 nothing ahead; and the car ahead, if any.
    fn classify_lane_occupant(&self, me: usize, s: &Scan, lane: i32) -> (i32, Option<usize>) {
        if !(0..MAP_LANES as i32).contains(&lane) {
            return (0, None);
        }
        let l = lane as usize;
        if lane != self.opp.data[me].lane && s.back[l] != -1 {
            return (0, None);
        }
        if s.front[l] == -1 {
            return (4, None);
        }
        let occupant = s.front[l] as usize;
        let d = self.racer_distance_margin(occupant, me);
        let rating = if 57600.0 < d { if 102400.0 < d { 3 } else { 2 } } else { 1 };
        (rating, Some(occupant))
    }

    /// The lanes either side: toward the median ("faster") and toward the edge
    /// ("slower"), only one of them at the outer or inner lane. How many there are.
    fn rate_lanes(&self, me: usize, s: &Scan, lanes: &mut [i32; 3], rating: &mut [i32; 3], occupant: &mut [Option<usize>; 3]) -> usize {
        let o = &self.opp.data[me];
        let d = if o.direction == 1 { 1 } else { -1 };
        let (outer, inner) = if o.direction == 1 { (0, 3) } else { (10, 7) };
        *lanes = [o.lane, o.lane + d, o.lane - d];
        (rating[0], occupant[0]) = self.classify_lane_occupant(me, s, lanes[0]);
        if o.lane == inner || o.lane == outer {
            if o.lane == inner {
                lanes[1] = lanes[2];
            }
            (rating[1], occupant[1]) = self.classify_lane_occupant(me, s, lanes[1]);
            return 2;
        }
        (rating[1], occupant[1]) = self.classify_lane_occupant(me, s, lanes[1]);
        (rating[2], occupant[2]) = self.classify_lane_occupant(me, s, lanes[2]);
        3
    }

    /// The lane choice rivals and chasing police share: the best rated lane, on a tie
    /// the one nearer the preferred lane. Full speed if the chosen lane has room, else
    /// the speed of the car ahead in it.
    fn choose_lane(&mut self, me: usize, d: Driver, n: usize, lanes: &[i32; 3], rating: &[i32; 3], occupant: &[Option<usize>; 3]) {
        let far = |a: i32| (d.lane - a).abs();
        let mut pick = 0;
        if n == 2 {
            if rating[1] == rating[0] {
                if rating[1] >= 2 && far(lanes[1]) < far(lanes[0]) {
                    pick = 1;
                }
            } else if rating[0] < rating[1] {
                pick = 1;
            }
        } else if rating[0] < rating[1] || rating[0] < rating[2] {
            pick = if rating[2] == rating[1] {
                if far(lanes[1]) > far(lanes[2]) { 2 } else { 1 }
            } else if rating[1] < rating[2] {
                2
            } else {
                1
            };
        } else if rating[0] == rating[1] {
            if far(lanes[1]) < far(lanes[0]) {
                pick = 1;
            }
        } else if rating[2] == rating[0] && far(lanes[0]) > far(lanes[2]) {
            pick = 2;
        }
        let occupant_speed = occupant[pick].map(|o| self.opp.data[o].speed);
        let o = &mut self.opp.data[me];
        if pick != 0 {
            o.target_lane = lanes[pick];
        }
        o.max_speed = match occupant_speed {
            Some(s) if rating[pick] <= 1 => s,
            _ => d.speed,
        };
    }

    /// drive_traffic in the recovered names: a rival's driving.
    fn drive_rival(&mut self, driver: usize) {
        let d = self.drivers.driver_list[driver];
        let me = d.opponent;
        let (chunk, direction) = (self.opp.data[me].chunk, self.opp.data[me].direction);
        let s = self.scan_lanes_ahead(me, chunk, 9, 3, direction);
        let (mut lanes, mut rating, mut occupant) = ([0; 3], [0; 3], [None; 3]);
        let n = self.rate_lanes(me, &s, &mut lanes, &mut rating, &mut occupant);
        let ri = d.index as usize;
        let rs = self.drivers.rival_states[ri];
        if rs.stopped_by != -1 {
            // Stopped by the police: brake to 2 and wait.
            let o = &mut self.opp.data[me];
            if o.max_speed != 2.0 {
                o.max_speed = (o.max_speed * 0.5).max(2.0);
                return;
            }
            let rs = &mut self.drivers.rival_states[ri];
            if rs.counter == 0 {
                return;
            }
            rs.counter -= 1;
            if rs.counter == 0 {
                rs.stopped_by = -1;
            }
            return;
        }
        let mut d = d;
        if rs.state == 4 {
            // Finished: to the outer lane, slowing to a stop.
            d.lane = if direction == 1 { 0 } else { 10 };
            d.speed = rs.counter as f32;
            self.drivers.driver_list[driver] = d;
            if self.opp.data[me].max_speed == 0.0 {
                let ob = self.opp.data[me].object as usize;
                self.pools.objects3d[ob].update_interval = -1;
            }
        }
        self.choose_lane(me, d, n, &lanes, &rating, &occupant);
    }

    /// drive_racer in the recovered names: a police car.
    fn drive_police(&mut self, driver: usize) {
        let d = self.drivers.driver_list[driver];
        let me = d.opponent;
        let pi = d.index as usize;
        let (dir, chunk, lane) = (self.opp.data[me].direction, self.opp.data[me].chunk, self.opp.data[me].lane);
        let step = if dir == 1 { 1 } else { -1 };
        let outer = if dir == 1 { 0 } else { 10 };
        if self.drivers.police_states[pi].state == 0 {
            // Parked: watch for a racer speeding past.
            self.opp.data[me].max_speed = 0.0;
            let seen = self.scan_lanes_for_heading(chunk, 6, dir);
            for k in 0..6 {
                if !seen[k] {
                    continue;
                }
                let fast = self.opp.data[k].speed > 9.0;
                self.drivers.police_states[pi].fast = fast;
                if !fast {
                    continue;
                }
                // "At most two cars on a racer" - but the original compares each police
                // car's target with that car's own number, not with the racer; kept.
                let chasing = (0..self.drivers.police_state_count).filter(|&j| self.drivers.police_states[j].target == j as i32).count();
                if chasing >= 2 {
                    continue;
                }
                // The player two times in three; a rival only by an even-numbered car,
                // very rarely (the original's rand() == 0), during the race.
                let chase = if k == 0 {
                    self.rand() % 3 != 0
                } else {
                    d.index % 2 == 0 && ((self.rand() == 0) as i32) % 3 != 0 && self.race_state == RACE_RUNNING
                };
                if chase {
                    let p = &mut self.drivers.police_states[pi];
                    p.state = 1;
                    p.target = k as i32;
                    p.speed = d.speed as i32;
                    p.lane = d.lane;
                    self.opp.data[me].max_speed = d.speed;
                    return;
                }
            }
            return;
        }
        let s = self.scan_lanes_ahead(me, chunk, 9, 3, dir);
        let mut d = d;
        if self.drivers.police_states[pi].state == 1 {
            let p = self.drivers.police_states[pi];
            let t = self.opp.data[p.target as usize].clone();
            if t.chunk == chunk || step * t.chunk > step * chunk {
                // Behind the racer: chase in its lane.
                d.speed = p.speed as f32;
                d.lane = t.lane;
                self.drivers.driver_list[driver] = d;
            } else if t.lane == lane {
                // In front of it in its lane: brake, and stop it when close.
                d.speed = ((t.speed - 1.0) as i32 as f32).max(0.0);
                d.lane = t.lane;
                self.drivers.driver_list[driver] = d;
                if (chunk - t.chunk).abs() < 2 {
                    if p.target == 0 {
                        self.set_race_state(RACE_FINE);
                    } else {
                        let ri = self.drivers.driver_list[t.field_e0 as usize].index as usize;
                        let rs = &mut self.drivers.rival_states[ri];
                        rs.stopped_by = me as i32;
                        rs.counter = 100;
                    }
                    let target = p.target;
                    for q in self.drivers.police_states.iter_mut().take(self.drivers.police_state_count) {
                        if q.target == target {
                            q.state = 2;
                            q.target = -1;
                            q.counter = 350;
                        }
                    }
                }
            } else {
                // Alongside: move into its lane at its speed.
                let o = &mut self.opp.data[me];
                o.target_lane = t.lane;
                o.max_speed = t.speed.max(0.0);
                return;
            }
        }
        let (mut lanes, mut rating, mut occupant) = ([0; 3], [0; 3], [None; 3]);
        let n = self.rate_lanes(me, &s, &mut lanes, &mut rating, &mut occupant);
        if self.drivers.police_states[pi].state == 2 {
            // Back to the hard shoulder and park.
            if self.opp.data[me].lane != outer {
                self.opp.data[me].target_lane = outer;
                self.drivers.police_states[pi].counter = 350;
                return;
            }
            if self.drivers.police_states[pi].counter != 0 {
                self.opp.data[me].max_speed *= 0.99;
                self.drivers.police_states[pi].counter -= 1;
                if rating[0] < 2 {
                    if let Some(o) = occupant[0] {
                        self.opp.data[me].max_speed = self.opp.data[o].speed;
                    }
                }
                return;
            }
            self.opp.data[me].max_speed = 0.0;
            self.drivers.police_states[pi].state = 0;
            return;
        }
        self.choose_lane(me, d, n, &lanes, &rating, &occupant);
    }

    fn set_max(&mut self, me: usize, v: f32) {
        self.opp.data[me].max_speed = v;
    }

    /// The speed behind a car: its speed, at most ours, not below 0.
    fn follow(&self, other: usize, own: f32) -> f32 {
        let s = self.opp.data[other].speed;
        (if s < own { s } else { own }).max(0.0)
    }

    /// Civilians (drive_opponent's own case).
    fn drive_civilian(&mut self, d: Driver) {
        let me = d.opponent;
        let (chunk, dir, lane) = (self.opp.data[me].chunk, self.opp.data[me].direction, self.opp.data[me].lane);
        let step = if dir == 1 { 1 } else { -1 };
        let s = self.scan_lanes_ahead(me, chunk, 9, 3, dir);
        // Off the road (lane -1) the original read outside its lane arrays; the port
        // keeps the car at its speed until it is back in a lane.
        if !(0..MAP_LANES as i32).contains(&lane) {
            self.set_max(me, d.speed);
            return;
        }
        let at = |l: i32, list: &[i32; MAP_LANES]| list[l as usize];
        if d.lane != lane {
            // Out of its lane (overtaking): back when there is room.
            let mut other = lane - step;
            if !(0..MAP_LANES as i32).contains(&other) {
                other = lane;
            }
            if at(lane, &s.front) == -1 {
                if at(other, &s.back) != -1 {
                    self.set_max(me, d.speed);
                    return;
                }
                if at(other, &s.front) == -1 {
                    self.opp.data[me].target_lane = other;
                    self.set_max(me, d.speed);
                    return;
                }
                let o2 = at(other, &s.front) as usize;
                if self.opp.data[o2].speed < d.speed {
                    self.set_max(me, d.speed); // faster than the slow lane
                    return;
                }
                let dist = self.racer_distance_margin(o2, me);
                if 57600.0 < dist {
                    self.opp.data[me].target_lane = other;
                    let v = if 102400.0 <= dist { d.speed } else { self.follow(o2, d.speed) };
                    self.set_max(me, v);
                    return;
                }
                self.set_max(me, d.speed);
                return;
            }
            let o2 = at(lane, &s.front) as usize;
            let dist = self.racer_distance_margin(o2, me);
            if 57600.0 < dist {
                if 102400.0 <= dist {
                    self.set_max(me, d.speed);
                    return;
                }
                if self.opp.data[o2].speed < d.speed {
                    let v = self.opp.data[o2].speed.max(0.0);
                    self.set_max(me, v);
                    return;
                }
                if at(other, &s.back) == -1 {
                    if at(other, &s.front) == -1 {
                        self.set_max(me, d.speed);
                        self.opp.data[me].target_lane = other;
                        return;
                    }
                    let o4 = at(other, &s.front) as usize;
                    if self.opp.data[o4].speed < d.speed {
                        self.set_max(me, d.speed);
                        return;
                    }
                    let d4 = self.racer_distance_margin(o4, me);
                    if 57600.0 < d4 {
                        self.opp.data[me].target_lane = other;
                        let v = if d4 < 102400.0 { self.follow(o4, d.speed) } else { d.speed };
                        self.set_max(me, v);
                        return;
                    }
                }
            } else if at(other, &s.back) == -1 {
                if at(other, &s.front) == -1 {
                    self.set_max(me, d.speed);
                    self.opp.data[me].target_lane = other;
                    return;
                }
                let o3 = at(other, &s.front) as usize;
                let v = self.opp.data[o3].speed.max(0.0);
                self.set_max(me, v);
                self.opp.data[me].target_lane = other;
                return;
            }
            // Follow the car ahead.
            let v = self.follow(o2, d.speed);
            self.set_max(me, v);
            return;
        }
        if at(lane, &s.front) == -1 {
            self.set_max(me, d.speed); // nothing in sight
            return;
        }
        let o2 = at(lane, &s.front) as usize;
        let dist = self.racer_distance_margin(o2, me);
        let o2_speed = self.opp.data[o2].speed;
        if dist <= 57600.0 {
            let other = lane + step;
            if d.speed <= o2_speed || self.opp.data[me].speed < o2_speed || !(0..MAP_LANES as i32).contains(&other) {
                // Slowing down behind it.
                let v = self.follow(o2, d.speed);
                self.set_max(me, v);
                return;
            }
            if at(other, &s.back) == -1 {
                // Overtaking.
                self.opp.data[me].target_lane = other;
                self.set_max(me, d.speed);
                return;
            }
            self.set_max(me, o2_speed.max(0.0));
            return;
        }
        if 102400.0 <= dist {
            self.set_max(me, d.speed);
            return;
        }
        // Following, or getting ready to overtake.
        if d.speed <= o2_speed {
            let v = self.follow(o2, d.speed);
            self.set_max(me, v);
            return;
        }
        self.set_max(me, d.speed);
    }

    pub fn drive_opponent(&mut self, driver: usize) {
        let d = self.drivers.driver_list[driver];
        if !d.used || !d.assigned {
            panic!("cannot drive driver {driver}");
        }
        match d.kind {
            0 | 1 => {}
            DRIVER_CIVILIAN => self.drive_civilian(d),
            DRIVER_RIVAL => self.drive_rival(driver),
            DRIVER_POLICE => self.drive_police(driver),
            k => panic!("illegal driver type {k}"),
        }
    }
}
