/*
Danger from the Deep - Open source submarine simulation
Copyright (C) 2003-2006  Thorsten Jordan, Luis Barrancos and others.

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

// Game loader - centralizes game state deserialization from XML
// subsim (C)+(W) Thorsten Jordan. SEE LICENSE

#include "game_loader.h"
#include "airplane.h"
#include "convoy.h"
#include "datadirs.h"
#include "depth_charge.h"
#include "game.h"
#include "gun_shell.h"
#include "player_info.h"
#include "ship.h"
#include "submarine.h"
#include "terrain.h"
#include "torpedo.h"
#include "water.h"
#include "xml.h"
#include "global_data.h"
#include "texts.h"
#include <algorithm>
#include <memory>
#include <sstream>

using std::string;


void game_loader::load(game &g, const std::string &filename) {
    bool is_de = (texts::get_language_code() == "de");
    set_loading_progress(0.02f, is_de ? "Lese Missionsdatei..." : "Reading mission data...");

    xml_doc doc(filename);
    doc.load();
    xml_elem sg = doc.first_child();

    // Load state first (time needed for checks during loading)
    if (sg.has_child("state")) {
        xml_elem gst = sg.child("state");
        g.time = gst.attrf("time");
        g.mytrails->set_last_trail_time(gst.attrf("last_trail_time"));
        if (gst.has_child("equipment_date"))
            g.equipment_date.load(gst.child("equipment_date"));
        g.myvisibility->set_max_distance(gst.attrf("max_view_dist"));
    }

    set_loading_progress(0.08f, is_de ? "Berechne Meeres-Simulation..." : "Simulating ocean...");
    dftd_startup_log("game_loader: Initializing water simulation...");
    g.mywater = std::make_unique<water>(g.time, g.config);
    dftd_startup_log("game_loader: Water simulation initialized.");

    set_loading_progress(0.20f, is_de ? "Generiere Geländedaten..." : "Generating terrain data...");
    dftd_startup_log("game_loader: Initializing terrain...");
    g.myheightgen = std::make_unique<terrain<Sint16>>(get_map_dir() + "terrain/terrain.xml",
                                                      get_map_dir() + "terrain/", TERRAIN_NR_LEVELS + 1);
    dftd_startup_log("game_loader: Terrain initialized.");

    // Create entities from XML
    if (sg.has_child("ships")) {
        xml_elem sh = sg.child("ships");
        unsigned total_ships = 0;
        for (xml_elem::iterator it = sh.iterate("ship"); !it.end(); it.next()) ++total_ships;

        unsigned cur_ship = 0;
        for (xml_elem::iterator it = sh.iterate("ship"); !it.end(); it.next(), ++cur_ship) {
            std::string ship_type = it.elem().attr("type");
            float prog = 0.25f + 0.40f * (float(cur_ship) / float(std::max(1u, total_ships)));
            std::ostringstream oss;
            if (is_de) {
                oss << "Lade Schiffe (" << (cur_ship + 1) << "/" << total_ships << ": " << ship_type << ")...";
            } else {
                oss << "Loading ships (" << (cur_ship + 1) << "/" << total_ships << ": " << ship_type << ")...";
            }
            set_loading_progress(prog, oss.str());

            xml_doc spec(data_file().get_filename(ship_type));
            spec.load();
            g.ships.push_back(std::make_unique<ship>(g, spec.first_child()));
        }
    }

    if (sg.has_child("submarines")) {
        xml_elem su = sg.child("submarines");
        unsigned total_subs = 0;
        for (xml_elem::iterator it = su.iterate("submarine"); !it.end(); it.next()) ++total_subs;

        unsigned cur_sub = 0;
        for (xml_elem::iterator it = su.iterate("submarine"); !it.end(); it.next(), ++cur_sub) {
            std::string sub_type = it.elem().attr("type");
            float prog = 0.65f + 0.10f * (float(cur_sub) / float(std::max(1u, total_subs)));
            std::ostringstream oss;
            if (is_de) oss << "Lade U-Boot (" << sub_type << ")...";
            else oss << "Loading submarine (" << sub_type << ")...";
            set_loading_progress(prog, oss.str());

            xml_doc spec(data_file().get_filename(sub_type));
            spec.load();
            g.submarines.push_back(std::make_unique<submarine>(g, spec.first_child()));
        }
    }

    if (sg.has_child("airplanes")) {
        xml_elem ap = sg.child("airplanes");
        for (xml_elem::iterator it = ap.iterate("airplane"); !it.end(); it.next()) {
            xml_doc spec(data_file().get_filename(it.elem().attr("type")));
            spec.load();
            g.airplanes.push_back(std::make_unique<airplane>(g, spec.first_child()));
        }
    }

    if (sg.has_child("torpedoes")) {
        xml_elem tp = sg.child("torpedoes");
        for (xml_elem::iterator it = tp.iterate("torpedo"); !it.end(); it.next()) {
            xml_doc spec(data_file().get_filename(it.elem().attr("type")));
            spec.load();
            g.torpedoes.push_back(std::make_unique<torpedo>(g, spec.first_child(), torpedo::setup()));
        }
    }

    if (sg.has_child("depth_charges")) {
        xml_elem dc = sg.child("depth_charges");
        for (xml_elem::iterator it = dc.iterate("depth_charge"); !it.end(); it.next()) {
            g.depth_charges.push_back(std::make_unique<depth_charge>(g));
        }
    }

    if (sg.has_child("gun_shells")) {
        xml_elem gs = sg.child("gun_shells");
        for (xml_elem::iterator it = gs.iterate("gun_shell"); !it.end(); it.next()) {
            g.gun_shells.push_back(std::make_unique<gun_shell>(g));
        }
    }

    if (sg.has_child("convoys")) {
        xml_elem cv = sg.child("convoys");
        for (xml_elem::iterator it = cv.iterate("convoy"); !it.end(); it.next()) {
            g.convoys.push_back(std::make_unique<convoy>(g));
        }
    }

    set_loading_progress(0.78f, is_de ? "Initialisiere Schiffssysteme..." : "Initializing ship systems...");
    // Load entity state
    if (sg.has_child("ships")) {
        unsigned k = 0;
        xml_elem sh = sg.child("ships");
        for (xml_elem::iterator it = sh.iterate("ship"); !it.end(); it.next(), ++k) {
            if (k < g.ships.size()) {
                g.ships[k]->load(it.elem());
            }
        }
    }

    if (sg.has_child("submarines")) {
        unsigned k = 0;
        xml_elem su = sg.child("submarines");
        for (xml_elem::iterator it = su.iterate("submarine"); !it.end(); it.next(), ++k) {
            if (k < g.submarines.size()) {
                g.submarines[k]->load(it.elem());
            }
        }
    }

    if (g.airplanes.size() > 0 && sg.has_child("airplanes")) {
        unsigned k = 0;
        for (xml_elem::iterator it = sg.child("airplanes").iterate("airplane"); !it.end(); it.next(), ++k) {
            if (k < g.airplanes.size()) g.airplanes[k]->load(it.elem());
        }
    }

    if (g.torpedoes.size() > 0 && sg.has_child("torpedoes")) {
        unsigned k = 0;
        for (xml_elem::iterator it = sg.child("torpedoes").iterate("torpedo"); !it.end(); it.next(), ++k) {
            if (k < g.torpedoes.size()) g.torpedoes[k]->load(it.elem());
        }
    }

    if (g.depth_charges.size() > 0 && sg.has_child("depth_charges")) {
        unsigned k = 0;
        for (xml_elem::iterator it = sg.child("depth_charges").iterate("depth_charge"); !it.end(); it.next(), ++k) {
            if (k < g.depth_charges.size()) g.depth_charges[k]->load(it.elem());
        }
    }

    if (g.gun_shells.size() > 0 && sg.has_child("gun_shells")) {
        unsigned k = 0;
        for (xml_elem::iterator it = sg.child("gun_shells").iterate("gun_shell"); !it.end(); it.next(), ++k) {
            if (k < g.gun_shells.size()) g.gun_shells[k]->load(it.elem());
        }
    }

    if (g.convoys.size() > 0 && sg.has_child("convoys")) {
        unsigned k = 0;
        for (xml_elem::iterator it = sg.child("convoys").iterate("convoy"); !it.end(); it.next(), ++k) {
            if (k < g.convoys.size()) g.convoys[k]->load(it.elem());
        }
    }

    set_loading_progress(0.92f, is_de ? "Konfiguriere Spieler & Sensoren..." : "Configuring player & sensors...");
    if (sg.has_child("player")) {
        xml_elem pl = sg.child("player");
        g.player = g.load_ptr(pl.attru("ref"));
    }

    if (sg.has_child("sunken_ships")) {
        g.myscoring->load(sg);
    }
    if (sg.has_child("pings")) {
        g.mypings->load(sg);
    }
    if (sg.has_child("player_info")) {
        g.playerinfo = player_info(sg.child("player_info"));
    }

    set_loading_progress(1.00f, is_de ? "Laden abgeschlossen" : "Loading completed");
}
