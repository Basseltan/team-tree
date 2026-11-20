// Team-Tree base + LED post for the "Modern Christmas Tree Lamp"
// (makerworld 796415, vase-mode shade). Own design: only the mating
// dimensions of the original base are reused.
//
// Parts (select with -D 'part="..."'):
//   base     - foot with upside-down ESP32-C3 Super Mini, USB-C at the back,
//              BOOT / RST pressed from below with a ballpoint refill
//   post     - round foot + twisted blade carrying the WS2812B strip
//              (144/m, 12 mm). A 12 mm strip cannot be wound as a helix
//              around a thin post (it would have to bend across its width),
//              but it twists easily along its length.
//   collar_test - only the ledge + collar of the base, to check the fit
//                 in the shade before printing the whole base
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
// Conical collar: the shade slides on until it wedges. A cylindrical slip fit
// (r 33.53) turned out far too loose, so the cone starts with a little
// interference at the ledge (the 0.5 mm vase wall flexes) and covers inner
// radii from collar_r_top to collar_r_bot.
collar_r_bot = tree_valley_r - tree_wall + 0.25;  // 34.08 at the ledge
collar_r_top = collar_r_bot - 1.2;                // 32.88 at the top
collar_ro   = collar_r_top;
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
press_d = 3.0;     // press holes for a ballpoint refill (refills are ~2.3-2.5 mm)

/* ---------- LED post ---------- */
post_od     = 16;    // round foot, sits in the socket
post_id     = 11.6;
post_x      = 4;     // offset towards the front (board is at the back)
num_leds    = 15;    // must match kNumLeds in include/Config.h
strip_w     = 12;
strip_len   = num_leds * 1000 / 144;   // 15 LEDs at 144/m = 104.2
strip_start = 30;    // bottom end of the strip (DIN) above the post bottom
// Twist of the blade over the strip length. 180 deg: the LEDs at the bottom
// face one side, the middle ones the front, the top ones the other side.
// More twist stretches the strip edges too much; 0 = straight.
strip_twist = 180;
blade_w     = strip_w + 2;
blade_t     = 3.5;
groove_d    = 0.6;   // shallow groove that keeps the strip centred
groove_w    = strip_w + 0.6;
post_len    = strip_start + strip_len + 3;

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

$fn = 120;
eps = 0.01;

echo(str("post length = ", post_len, " mm, strip ", strip_len, " mm"));
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
    cylinder(r1 = collar_r_bot, r2 = collar_r_top, h = collar_h - 0.6 + eps);
    cylinder(r = collar_r_top - 0.6, h = collar_h + eps);
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
  // supports under the front corners, clear of the ceramic antenna and of
  // the latch slot in the floor
  for (s = [-1, 1])
    translate([pcb_x1 - 2.2, s > 0 ? antenna_w / 2 + 1 : -pcb_w / 2 + 0.3, 0])
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
  // guide tubes for the press holes (stop below the buttons)
  for (b = [boot_xy, rst_xy])
    translate([b[0], b[1], 0]) cylinder(d = press_d + 2, h = pcb_bot - btn_h - 0.8, $fn = 32);
}

// Front latch: a vertical snap hook bends across the print layers and broke.
// This arm stands on the build plate and is only anchored at one end, so it
// flexes sideways, i.e. along the layer lines, which PLA tolerates well.
latch_t   = 1.0;
latch_y0  = -pcb_w / 2 + 1.5;   // anchored end
latch_len = 13;
latch_x   = pcb_x1 + 0.15;
latch_h   = pcb_top + 1.55;

module front_latch() {
  // anchor pillar
  translate([latch_x, latch_y0 - 2, 0]) cube([latch_t + 1.5, 2, latch_h]);
  // arm
  translate([latch_x, latch_y0 - eps, 0]) cube([latch_t, latch_len + eps, latch_h]);
  // nose over the PCB with lead-in, on the free part of the arm
  translate([latch_x, latch_y0 + 4, pcb_top + 0.15]) hull() {
    cube([eps, latch_len - 4, 1.4]);
    translate([-0.8, 0, 0]) cube([0.8 + eps, latch_len - 4, 0.4]);
  }
}

module latch_slot() {
  // frees the arm from the floor; room for 0.8 mm deflection to the front
  translate([latch_x - 0.6, latch_y0, -1])
    cube([latch_t + 0.6 + 1.2, latch_len + 0.8, floor_t + 1 + eps]);
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
  translate([boot_xy[0], boot_xy[1] + 5, -eps])
    linear_extrude(0.5) mirror([0, 1, 0])
      text("BOOT", size = 3, halign = "center", valign = "center", font = "Liberation Sans:style=Bold");
  translate([rst_xy[0], rst_xy[1] - 4.8, -eps])
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
    }
    latch_slot();
    for (b = [boot_xy, rst_xy])  // press holes for a ballpoint refill
      translate([b[0], b[1], -1]) cylinder(d = press_d, h = 20, $fn = 32);
    labels();
  }
  front_latch();
}

module collar_test() {
  // ledge + collar only (~10 min print) to check the fit in the shade
  translate([0, 0, -(ledge_z - 2)]) intersection() {
    base();
    translate([0, 0, ledge_z - 2]) cylinder(r = R + 1, h = collar_h + 3);
  }
}

/* ======================= LED post ======================= */

module blade_profile() {
  // groove for the strip on the +x face
  difference() {
    square([blade_t, blade_w], center = true);
    translate([blade_t / 2 - groove_d, -groove_w / 2]) square([groove_d + 1, groove_w]);
  }
}

module post() {
  // round, hollow foot; the wires run down inside it
  difference() {
    cylinder(d = post_od, h = strip_start);
    translate([0, 0, -eps]) cylinder(d = post_id, h = strip_start - 2);
    // wire entry next to the strip's bottom end (DIN), in front of the blade
    rotate([0, 0, strip_twist / 2]) translate([blade_t / 2 + 2.6, 0, strip_start - 4])
      cylinder(d = 4, h = 5, $fn = 24);
    // wire exit at the bottom, aligned with the socket slot (-x)
    translate([-post_od, -3, -eps]) cube([post_od, 6, 6]);
  }
  // twisted blade: the middle of the strip faces the front (+x)
  translate([0, 0, strip_start - eps])
    rotate([0, 0, strip_twist / 2])
      linear_extrude(height = strip_len + 2 * eps, twist = strip_twist, slices = 120, convexity = 4)
        blade_profile();
  // short straight end above the strip
  translate([0, 0, strip_start + strip_len])
    rotate([0, 0, -strip_twist / 2]) linear_extrude(height = 3) blade_profile();
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
else if (part == "collar_test") collar_test();
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
