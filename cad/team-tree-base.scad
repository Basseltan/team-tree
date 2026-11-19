// Team-Tree base + LED post for the "Modern Christmas Tree Lamp"
// (makerworld 796415, vase-mode shade). Own design: only the mating
// dimensions of the original base are reused.
//
// Parts (select with -D 'part="..."'):
//   base     - foot with upside-down ESP32-C3 Super Mini, USB-C at the back,
//              BOOT pressed from below through a flexure tongue
//   post     - hollow post, 12 WS2812B (144/m strip, 12 mm) wound as a helix
//   assembly - preview with a dummy board and the post in place
//
// All dimensions in mm. Print base and post upright, no supports.

part = "assembly";
team_name = "TR-EDX2";   // engraved under the base, above "Team-Tree"

/* ---------- Mating dimensions (from the original 3MF) ---------- */
// The shade is printed in vase mode WITHOUT bottom layers: only the outer
// contour remains, a single 0.5 mm wall. Its bottom 9 mm are a band with
// shallow scallops (outer r 34.33 .. 35.1); the collar has to clear the
// scallop valleys on the inside of that wall.
// (If the shade is printed as a solid model instead, its bottom lip has an
// inner radius of 31.55 - set collar_ro = 31.3 then.)
tree_r      = 35.1;   // outer radius of the shade's bottom band (peaks)
tree_valley_r = 34.33; // outer radius in the scallop valleys
tree_wall   = 0.5;    // vase-mode line width
base_d      = 2 * (tree_r + 0.1);  // flush with the shade
ledge_z     = 18;     // height of the ledge the tree stands on
collar_h    = 6;      // centring collar reaching into the tree (band is 9 mm)
collar_ro   = tree_valley_r - tree_wall - 0.3;  // slip fit inside the wall
collar_wall = 2;

/* ---------- Base ---------- */
wall     = 2.0;
floor_t  = 2.0;
chamfer  = 0.8;

/* ---------- ESP32-C3 Super Mini (measure your board!) ---------- */
pcb_l   = 22.52;   // along USB axis
pcb_w   = 18.0;
pcb_t   = 1.0;
usb_w   = 8.94;    // USB-C receptacle
usb_h   = 3.26;
usb_overhang = 1.84;  // receptacle sticks out over the PCB edge
// Buttons, measured on the COMPONENT side with USB-C pointing up:
//   *_from_left = left PCB edge -> button centre
//   *_from_usb  = USB-side PCB edge -> button centre
// (taken from a dimensioned vendor drawing, 46.8 px/mm)
boot_from_left = 6.13;
boot_from_usb  = 8.70;
rst_from_left  = 11.69;
rst_from_usb   = 8.70;
// Button height above the PCB. Rather too high than too low: a nub that
// presses BOOT permanently would put the ESP into download mode.
btn_h   = 2.2;
antenna_w = 7.0;   // ceramic antenna (6.6 mm) centred on the front edge, keep clear

/* ---------- LED post ---------- */
post_od     = 16;
post_id     = 11.6;
post_x      = 4;     // offset towards the front (board is at the back)
num_leds    = 15;    // must match kNumLeds in include/Config.h
strip_w     = 12;
strip_len   = num_leds * 1000 / 144;   // 15 LEDs at 144/m = 104.2
groove_d    = 0.8;
groove_w    = strip_w + 0.6;
helix_start = 24;    // above the post bottom (just above the base collar)
// Top ends 134 mm above the shade's bottom edge; up there the shade still
// leaves >= 2 mm around post + LEDs (the post is off-centre by post_x).
post_len    = 150;

/* ---------- Derived ---------- */
R      = base_d / 2;
cav_r  = R - wall;
collar_ri = collar_ro - collar_wall;
// Back edge of the PCB: as far back as possible while the corner pocket
// still leaves >= 0.8 mm of outer wall.
pcb_x0 = -sqrt(R * R - pow(pcb_w / 2 + 0.4, 2)) + 0.8 + 0.4;
usb_mouth_x = pcb_x0 - usb_overhang;
pcb_x1 = pcb_x0 + pcb_l;                 // front edge
pcb_bot = floor_t + 5.0;                 // component side (facing down)
pcb_top = pcb_bot + pcb_t;
usb_zc  = pcb_bot - usb_h / 2;
// board flipped about its long axis: component-side "left" ends up at +y
function btn_pos(from_left, from_usb) = [pcb_x0 + from_usb, pcb_w / 2 - from_left];
boot_xy = btn_pos(boot_from_left, boot_from_usb);
rst_xy  = btn_pos(rst_from_left, rst_from_usb);

groove_r = post_od / 2 - groove_d;
helix_c  = 2 * PI * groove_r;
helix_p  = sqrt(strip_len * strip_len - helix_c * helix_c);  // rise per turn (one turn)
helix_a  = atan(helix_p / helix_c);                           // slope angle
band_deg = (groove_w / sin(helix_a)) / groove_r * 180 / PI;   // horizontal sector
band_h   = helix_p + groove_w / cos(helix_a);

$fn = 120;
eps = 0.01;

echo(str("helix rise per turn = ", helix_p, " mm, slope = ", helix_a, " deg"));
echo(str("total height with collar = ", ledge_z + collar_h, " mm"));

/* ======================= Base ======================= */

module chamfered_cyl(r, h, c) {
  hull() {
    translate([0, 0, c]) cylinder(r = r, h = h - 2 * c);
    cylinder(r = r - c, h = h);
  }
}

module base_body() {
  chamfered_cyl(R, ledge_z, chamfer);
  // collar with lead-in chamfer
  translate([0, 0, ledge_z - eps]) hull() {
    cylinder(r = collar_ro, h = collar_h - 1 + eps);
    cylinder(r = collar_ro - 1, h = collar_h + eps);
  }
}

module cavity() {
  // main cavity, tapering to the collar so the inner step prints without support
  translate([0, 0, floor_t]) cylinder(r = cav_r, h = ledge_z - floor_t - 3);
  translate([0, 0, ledge_z - 3 - eps])
    cylinder(r1 = cav_r, r2 = collar_ri, h = (cav_r - collar_ri) + eps);
  translate([0, 0, floor_t]) cylinder(r = collar_ri, h = 100);
}

module rrect(size, r) {  // rounded rectangle in YZ, extruded along X
  hull() for (y = [-1, 1], z = [-1, 1])
    translate([0, y * (size[0] / 2 - r), z * (size[1] / 2 - r)])
      rotate([0, 90, 0]) cylinder(r = r, h = size[2], $fn = 32);
}

module usb_window() {
  // recess for the overmould of common USB-C cables, ends at the mouth
  translate([-R - 2, 0, usb_zc]) rrect([13.5, 8.5, usb_mouth_x + 0.2 + R + 2], 2.5);
  // passage for the plug / receptacle
  translate([usb_mouth_x - 0.1, -(usb_w / 2 + 0.4), usb_zc - usb_h / 2 - 0.4])
    cube([usb_overhang + 1, usb_w + 0.8, usb_h + 0.8]);
  // flat pocket inside the back wall for the PCB corners
  translate([pcb_x0 - 0.4, -(pcb_w / 2 + 0.4), pcb_bot - 0.3])
    cube([4, pcb_w + 0.8, pcb_t + 0.6]);
}

module board_holder() {
  rail_t = 1.2;
  // support under the USB receptacle (short, keeps the BOOT tongue free)
  translate([usb_mouth_x, -usb_w / 2, 0]) cube([usb_overhang + 3.5, usb_w, usb_zc - usb_h / 2]);
  // supports under the front corners, clear of the ceramic antenna
  for (s = [-1, 1])
    translate([pcb_x1 - 1.6, s > 0 ? antenna_w / 2 + 1 : -pcb_w / 2 + 0.3, 0])
      cube([1.6, pcb_w / 2 - 0.3 - antenna_w / 2 - 1, pcb_bot]);
  // side rails
  for (s = [-1, 1])
    translate([pcb_x0 + 3, s > 0 ? pcb_w / 2 + 0.25 : -pcb_w / 2 - 0.25 - rail_t, 0])
      cube([pcb_l - 6, rail_t, pcb_top + 0.8]);
  // tabs over the back edge (PCB slides under them), between the USB
  // shell and the 5V / pin-9 solder pads in the corners
  tab_y0 = usb_w / 2 + 1.0;
  tab_y1 = pcb_w / 2 - 2.3;
  for (s = [-1, 1])
    translate([pcb_x0 - 1.5, s > 0 ? tab_y0 : -tab_y1, pcb_top + 0.15])
      cube([2.5, tab_y1 - tab_y0, 1.4]);
  // ...and carry them down to the floor so they print without support
  for (s = [-1, 1])
    translate([pcb_x0 - 1.5, s > 0 ? tab_y0 : -tab_y1, 0])
      cube([1.1, tab_y1 - tab_y0, pcb_top + 1.55]);
  // flexible snap hook at the front edge
  hook_t = 1.2;
  translate([pcb_x1 + 0.15, -3, 0]) {
    cube([hook_t, 6, pcb_top + 1.6]);
    // hook nose with lead-in
    translate([0, 0, pcb_top + 0.15]) hull() {
      cube([eps, 6, 1.4]);
      translate([-0.8, 0, 0]) cube([0.8 + eps, 6, 0.4]);
    }
  }
}

tongue_w = 6;
tongue_l = 11;
tongue_t = 1.2;
slot     = 0.7;

module tongue_cut(xy) {
  // U-shaped slot; the tongue's root is towards the front (+x)
  translate([xy[0] - 3, xy[1], -eps]) difference() {
    translate([-slot, -tongue_w / 2 - slot, 0]) cube([tongue_l + slot, tongue_w + 2 * slot, floor_t + 1]);
    translate([0, -tongue_w / 2, -1]) cube([tongue_l + 1, tongue_w, floor_t + 3]);
  }
  // thin the tongue from below (also keeps the table from pressing it)
  translate([xy[0] - 3 - slot - 1, xy[1] - tongue_w / 2 - slot - 1, -eps])
    cube([tongue_l + slot + 2, tongue_w + 2 * slot + 2, floor_t - tongue_t]);
}

module tongue_nub(xy) {
  translate([xy[0], xy[1], floor_t - eps])
    cylinder(d1 = 3.2, d2 = 2.4, h = pcb_bot - btn_h - 0.3 - floor_t, $fn = 32);
}

module post_socket() {
  translate([post_x, 0, 0]) difference() {
    cylinder(d = post_od + 5, h = floor_t + 8);
    translate([0, 0, floor_t]) cylinder(d = post_od + 0.4, h = 20);
    // wire slot towards the board
    translate([-post_od, -3, floor_t]) cube([post_od, 6, 20]);
  }
}

module labels() {
  // engraved underneath (mirrored so it reads correctly from below)
  translate([boot_xy[0] + tongue_l / 2 - 3, boot_xy[1] + tongue_w / 2 + 3.5, -eps])
    linear_extrude(0.5) mirror([0, 1, 0])
      text("BOOT", size = 3, halign = "center", valign = "center", font = "Liberation Sans:style=Bold");
  translate([rst_xy[0], rst_xy[1] - 4.5, -eps])
    linear_extrude(0.5) mirror([0, 1, 0])
      text("RST", size = 2.5, halign = "center", valign = "center", font = "Liberation Sans:style=Bold");
  translate([14, 0, -eps])
    linear_extrude(0.5) mirror([0, 1, 0]) {
      translate([0, 4]) text(team_name, size = 5.5, halign = "center", valign = "center", font = "Liberation Sans:style=Bold");
      translate([0, -4.5]) text("Team-Tree", size = 4.5, halign = "center", valign = "center", font = "Liberation Sans:style=Bold");
    }
}

module base() {
  difference() {
    union() {
      difference() { base_body(); cavity(); usb_window(); }
      // added after the window cut so the back tabs survive; clipped to
      // stay inside the outer skin
      intersection() {
        cylinder(r = R - 0.8, h = ledge_z);
        union() { board_holder(); post_socket(); }
      }
      tongue_nub(boot_xy);
    }
    tongue_cut(boot_xy);
    translate([rst_xy[0], rst_xy[1], -1]) cylinder(d = 2.0, h = floor_t + 2, $fn = 24);  // paper clip
    labels();
  }
}

/* ======================= LED post ======================= */

module helix_band() {
  translate([0, 0, helix_start])
    linear_extrude(height = band_h, twist = -360 * band_h / helix_p, slices = 200, convexity = 4)
      difference() {
        // sector of band_deg
        polygon(concat([[0, 0]], [for (a = [0 : band_deg / 24 : band_deg]) (post_od) * [cos(a), sin(a)]]));
        circle(r = groove_r);
      }
}

module post() {
  difference() {
    cylinder(d = post_od, h = post_len);
    translate([0, 0, -eps]) cylinder(d = post_id, h = post_len + 2 * eps);
    helix_band();
    // wire entry at the bottom end of the strip (DIN)
    translate([0, 0, helix_start + 4]) rotate([0, 0, band_deg / 2]) rotate([0, 90, 0])
      cylinder(d = 4.5, h = post_od, $fn = 24);
    // wire exit at the bottom, aligned with the socket slot (-x)
    translate([-post_od, -3, -eps]) cube([post_od, 6, 6]);
  }
  // arrow on the post: strip runs bottom -> top (DIN at the bottom)
}

/* ======================= Preview ======================= */

module dummy_board() {
  color("RoyalBlue") translate([pcb_x0, -pcb_w / 2, pcb_bot]) cube([pcb_l, pcb_w, pcb_t]);
  color("Silver") translate([usb_mouth_x, -usb_w / 2, pcb_bot - usb_h]) cube([7.5, usb_w, usb_h]);
  for (b = [boot_xy, rst_xy])
    color("Black") translate([b[0] - 1.5, b[1] - 2, pcb_bot - btn_h]) cube([3, 4, btn_h]);
}

if (part == "base") base();
else if (part == "post") post();
else if (part == "section") {  // cut through the BOOT button, for checking
  difference() {
    union() { color("WhiteSmoke") base(); dummy_board(); }
    translate([-50, boot_xy[1], -1]) cube([100, 50, 50]);
  }
}
else {
  color("WhiteSmoke") base();
  dummy_board();
  color("Gold", 0.9) translate([post_x, 0, floor_t]) post();
}
