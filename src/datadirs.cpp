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

// global directory data
// subsim (C)+(W) Thorsten Jordan. SEE LICENSE

#include "datadirs.h"
#include "error.h"
#include "filehelper.h"
#include "log.h"
#include "system_defines.h"

// Note! this is a global variable and is inited before main,
// any data depening on that variable, like objcache, textures etc. etc.
// MUST be inited after main starts, NOT as global static variables themselves,
// or the static initiliazation order fiasco will result.
#if defined(__amigaos4__)
std::string global_datadir = "PROGDIR:data/";
#else
std::string global_datadir = DATADIR;
#endif

const std::string &get_data_dir() {
    return global_datadir;
}

// Note! call this only once, and very early in main()!
void set_data_dir(const std::string &datadir) {
    global_datadir = datadir;
}

std::string resolve_object_alias(const std::string &id) {
    if (id == "merchant_small") return "fortshipciv";
    if (id == "merchant_medium") return "fortship1941";
    if (id == "merchant_large") return "libertyship1941";
    if (id == "freighter_large") return "empireprotector";
    if (id == "freighter_medium") return "empiredabchick";
    if (id == "troopship_medium") return "fortship1943";
    if (id == "tanker_small" || id == "tanker_medium") return "tanker_kennebak";
    if (id == "corvette") return "FlowerCorvette_RN";
    if (id == "destroyer") return "destroyer_tribal";
    if (id == "submarine") return "submarine_VIIc";
    return id;
}

data_file_handler::data_file_handler() {
    // scan data dir for all .data files
    std::string dir = "objects/";
    parse_for_data_files(dir + "airplanes/", airplane_ids);
    parse_for_data_files(dir + "ships/", ship_ids);
    parse_for_data_files(dir + "submarines/", submarine_ids);
    parse_for_data_files(dir + "torpedoes/", torpedo_ids);
    parse_for_data_files(dir + "props/", prop_ids);
}

static const std::string data_file_ext = ".data";
void data_file_handler::parse_for_data_files(std::string dir, std::list<std::string> &idlist) {
    directory d(get_data_dir() + dir);
    for (std::string f = d.read(); !f.empty(); f = d.read()) {
        if (f[0] == '.' || f == "CVS") {
            // avoid . and .. entries, as well as hidden files, and CVS directories as well
            continue;
        } else if (is_directory(get_data_dir() + dir + f)) {
            parse_for_data_files(dir + f + "/", idlist);
        } else if (f.length() > data_file_ext.length() && f.substr(f.length() - data_file_ext.length()) == data_file_ext) {
            std::string id = f.substr(0, f.length() - data_file_ext.length());
            // 			log_info("found file " << dir << " for id " << id);
            data_files[id] = dir;
            idlist.push_back(id);
        }
    }
}

const std::string &data_file_handler::get_rel_path(const std::string &objectid) const {
    std::string id = resolve_object_alias(objectid);
    std::map<std::string, std::string>::const_iterator it = data_files.find(id);
    if (it != data_files.end()) {
        return it->second;
    }

    // Try case-insensitive match
    for (std::map<std::string, std::string>::const_iterator cit = data_files.begin(); cit != data_files.end(); ++cit) {
        if (cit->first.size() == id.size() && strcasecmp(cit->first.c_str(), id.c_str()) == 0) {
            return cit->second;
        }
    }

    dftd_startup_log("WARNING: data_file_handler::get_rel_path: missing object '%s' (alias '%s'), using fallback!",
                     objectid.c_str(), id.c_str());

    // Never crash or throw! Fall back to a valid ship or first entry in data_files
    if (!data_files.empty()) {
        std::map<std::string, std::string>::const_iterator fit = data_files.find("fortshipciv");
        if (fit != data_files.end()) {
            return fit->second;
        }
        return data_files.begin()->second;
    }

    static const std::string empty;
    return empty;
}

std::string data_file_handler::get_path(const std::string &objectid) const {
    return get_data_dir() + get_rel_path(objectid);
}

std::string data_file_handler::get_rel_filename(const std::string &objectid) const {
    std::string id = resolve_object_alias(objectid);
    std::map<std::string, std::string>::const_iterator it = data_files.find(id);
    if (it != data_files.end()) {
        return it->second + it->first + data_file_ext;
    }
    for (std::map<std::string, std::string>::const_iterator cit = data_files.begin(); cit != data_files.end(); ++cit) {
        if (cit->first.size() == id.size() && strcasecmp(cit->first.c_str(), id.c_str()) == 0) {
            return cit->second + cit->first + data_file_ext;
        }
    }
    // Fallback if not found
    if (!data_files.empty()) {
        std::map<std::string, std::string>::const_iterator fit = data_files.find("fortshipciv");
        if (fit != data_files.end()) {
            return fit->second + fit->first + data_file_ext;
        }
        return data_files.begin()->second + data_files.begin()->first + data_file_ext;
    }
    return get_rel_path(objectid) + objectid + data_file_ext;
}

std::string data_file_handler::get_filename(const std::string &objectid) const {
    return get_data_dir() + get_rel_filename(objectid);
}
