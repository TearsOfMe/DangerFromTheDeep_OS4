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

// main program
// subsim (C)+(W) Thorsten Jordan. SEE LICENSE

#ifdef WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

#if defined(__amigaos4__)
static const char __attribute__((used)) amiga_version_tag[] =
    "\0$VER: DangerFromTheDeep 0.5.1 (14.10.2026) AmigaOS 4 Port by TearsOfMe";
#endif

#include <time.h>

#include "oglext/OglExt.h"
#include <glu.h>

#include "cfg.h"
#include "credits.h"
#include "altivec_detect.h"
#include "game_event.h"
#include "datadirs.h"
#include "date.h"
#include "faulthandler.h"
#include "filehelper.h"
#include "game.h"
#include "game_editor.h"
#include "global_data.h"
#include "highscorelist.h"
#include "image.h"
#include "keys.h"
#include "log.h"
#include "model.h"
#include "audio_backend.h"
#include "music.h"
#include "mymain.cpp"
#include "scoring_manager.h"
#include "ship.h"
#include "system.h"
#include "texts.h"
#include "texture.h"
#include "user_interface.h"
#include "vector3.h"
#include "widget.h"
#include <iostream>
#include <sstream>

#ifndef WIN32
// broken under windows... TODO
#include "dftdtester/tests.h"
#endif

#ifdef __amigaos4__
static const char* __attribute__((used)) stack_cookie = "$STACK: 4194304\n";
#endif

using std::unique_ptr;

/* fixme: 2006/12/02
   about caches:
   especially for images we need a cache like this:
   - basic functionality like objcache with ref and unref
   - unref'd images are kept as objects
   - when new images are ref'd old objects are occaisonally removed.
   How to decide about removal:
   - We have to limit RAM/Video-Ram usage:
   - Each image has a cost, that is width*height*bpp.
   - Each unref'd image has an age, a timestamp taken when unref'd lastly.
   - Compute total cost with some formula
   - Remove image with most cost, if total ram usage of unref'd images is above limit.
   So we have:
   - global limit for unref'd images, good starting point 4MB.
   - A formula to compute cost, c(s, t)  where s is size in bytes and t is age in milliseconds.
   We should have that few images to just keep a simple list for management.
   What about the cost formula?
   - Size matters
   - time is only to get rid of old, but small images.
   Try this formula:
   cost(s, t) = s + t^2/32.
   So after ca. 10 seconds the cost of an 1x1 image is as high as an fullscreen 3MB image.
   The cache should have a "clean_up" function to instantly remove all cleaned up images.
*/

highscorelist hsl_mission, hsl_career;
static constexpr const char *HSL_MISSION_NAME = "mission.hsc";
static constexpr const char *HSL_CAREER_NAME = "career.hsc";

// a dirty hack
void menu_notimplemented() {
    widget w(0, 0, 1024, 768, "", 0, "titlebackgr.jpg");
    widget_menu *wm = new widget_menu(0, 0, 400, 40, texts::get(110));
    w.add_child(wm);
    wm->add_entry(texts::get(20), new widget_caller_arg_button<widget, void (widget::*)(int), int>(&w, &widget::close, 0, 0, 0, 0, 0));
    wm->align(0, 0);
    w.run(0, false);
}

//
// save game directory and helper functions
//
string savegamedirectory =
#if defined(__amigaos4__)
    "PROGDIR:save/";
#elif defined(WIN32)
    "./save/";
#else
    (getenv("HOME") ? (string(getenv("HOME")) + "/.dangerdeep/") : "./save/");
#endif

string get_savegame_name_for(const string &descr, map<string, string> &savegames) {
    unsigned num = 1;
    for (map<string, string>::iterator it = savegames.begin(); it != savegames.end(); ++it) {
        if (it->second == descr)
            return savegamedirectory + it->first;
        unsigned num2 = unsigned(atoi((it->first.substr(5, 4)).c_str()));
        if (num2 >= num)
            num = num2 + 1;
    }
    char tmp[20];
    sprintf(tmp, "save_%04u.dftd", num);
    return savegamedirectory + tmp;
}

bool is_savegame_name(const string &s) {
    if (s.length() != 14)
        return false;
    if (s.substr(0, 5) != "save_")
        return false;
    if (s.substr(9, 7) != ".dftd")
        return false;
    for (int i = 5; i < 9; ++i)
        if (s[i] < '0' || s[i] > '9')
            return false;
    return true;
}

//
// loading, saving games
//
class loadsavequit_dialogue : public widget {
    widget_edit *gamename;
    widget_list *gamelist;
    widget_button *btnload, *btnsave, *btndel, *btnquit, *btncancel;
    const game *mygame;
    bool gamesaved;
    map<string, string> savegames;
    string gamefilename_to_load;
    void load();
    void save();
    void erase();
    void quit();
    void cancel() { close(0); } // return 0 for cancel/return, 1 for quit (if saving is enabled), 2 for loaded
    void update_list();

  public:
    string get_gamefilename_to_load() const { return gamefilename_to_load; }
    widget_edit *get_gamename() const { return gamename; }
    loadsavequit_dialogue(const game *g); // give 0 to disable saving
};

loadsavequit_dialogue::loadsavequit_dialogue(const game *g) : widget(0, 0, 1024, 768, texts::get(177), 0, "depthcharge.jpg"), mygame(g), gamesaved(false) {
    add_child(new widget_text(40, 40, 0, 0, texts::get(178)));
    gamename = new widget_edit(200, 40, 684, 40, "");
    add_child(gamename);
    widget_menu *wm = new widget_menu(40, 700, 180, 40, "", true);
    add_child(wm);
    btnload = wm->add_entry(texts::get(118), new widget_caller_button<loadsavequit_dialogue, void (loadsavequit_dialogue::*)()>(this, &loadsavequit_dialogue::load));
    if (mygame)
        btnsave = wm->add_entry(texts::get(119), new widget_caller_button<loadsavequit_dialogue, void (loadsavequit_dialogue::*)()>(this, &loadsavequit_dialogue::save));
    btndel = wm->add_entry(texts::get(179), new widget_caller_button<loadsavequit_dialogue, void (loadsavequit_dialogue::*)()>(this, &loadsavequit_dialogue::erase));
    if (mygame)
        btnquit = wm->add_entry(texts::get(120), new widget_caller_button<loadsavequit_dialogue, void (loadsavequit_dialogue::*)()>(this, &loadsavequit_dialogue::quit));
    btncancel = wm->add_entry(texts::get(mygame ? 121 : 20), new widget_caller_button<loadsavequit_dialogue, void (loadsavequit_dialogue::*)()>(this, &loadsavequit_dialogue::cancel));
    wm->adjust_buttons(944);
    struct lsqlist : public widget_list {
        void on_sel_change() {
            loadsavequit_dialogue *lsq = dynamic_cast<loadsavequit_dialogue *>(parent);
            if (lsq && lsq->get_gamename()) {
                lsq->get_gamename()->set_text(get_selected_entry());
                widget::set_focussed(lsq->get_gamename());
            }
        }
        lsqlist(int x, int y, int w, int h) : widget_list(x, y, w, h) {}
        ~lsqlist() {}
    };
    gamelist = new lsqlist(40, 100, 944, 580);
    add_child(gamelist);

    update_list();

    gamename->set_text(gamelist->get_selected_entry());
}

void loadsavequit_dialogue::load() {
    gamefilename_to_load = get_savegame_name_for(gamename->get_text(), savegames);
    // fixme: ask: replace this game?
    std::unique_ptr<widget> w(create_dialogue_ok(texts::get(185), texts::get(180) + gamename->get_text() + texts::get(181)));
    w->run();
    close(2); // load and close
}

void loadsavequit_dialogue::save() {
    string gname = gamename->get_text();
    if (gname.empty()) {
        gname = "savegame";
        gamename->set_text(gname);
    }
    string fn = get_savegame_name_for(gname, savegames);
    FILE *f = fopen(fn.c_str(), "rb");
    if (f) {
        fclose(f);
        std::unique_ptr<widget> w(create_dialogue_ok_cancel(texts::get(182), texts::get_replace(183, gname)));
        int ok = w->run();
        w.reset();
        if (!ok)
            return;
    }
    gamesaved = true;
    make_dir(savegamedirectory);
    mygame->save(fn, gname);
    std::unique_ptr<widget> w(create_dialogue_ok(texts::get(186), texts::get(180) + gname + texts::get(187)));
    w->run();
    update_list();
}

void loadsavequit_dialogue::erase() {
    std::unique_ptr<widget> w(create_dialogue_ok_cancel(texts::get(182), texts::get(188) + gamename->get_text() + texts::get(189)));
    int ok = w->run();
    w.reset();
    if (ok) {
        string fn = get_savegame_name_for(gamename->get_text(), savegames);
        remove(fn.c_str());
        int s = gamelist->get_selected() - 1;
        update_list();
        if (s < 0)
            s = 0;
        gamelist->set_selected(s);
        gamename->set_text(gamelist->get_selected_entry());
    }
}

void loadsavequit_dialogue::quit() {
    if (!gamesaved) {
        std::unique_ptr<widget> w(create_dialogue_ok_cancel(texts::get(182), texts::get(190)));
        int q = w->run();
        if (q)
            close(1);
    } else {
        close(1);
    }
}

void loadsavequit_dialogue::update_list() {
    savegames.clear();

    // read save games in directory
    {
        directory savegamedir(savegamedirectory);
        while (true) {
            string e = savegamedir.read();
            if (e.empty())
                break;
            if (is_savegame_name(e)) {
                string descr = mygame->read_description_of_savegame(savegamedirectory + e);
                savegames[e] = descr;
            }
        }
    }

    gamelist->clear();

    unsigned sel = 0;
    for (map<string, string>::iterator it = savegames.begin(); it != savegames.end(); ++it) {
        gamelist->append_entry(it->second);
        if (it->second == gamename->get_text())
            gamelist->set_selected(sel);
        ++sel;
    }

    if (savegames.size() == 0) {
        btnload->disable();
        btndel->disable();
    } else {
        btnload->enable();
        btndel->enable();
    }
}

//
// show hall of fame
//
void show_halloffame(const highscorelist &hsl) {
    if (sys().is_quit_requested())
        return;
    widget w(0, 0, 1024, 768, texts::get(197), 0, "krupp_docks.jpg");
    w.add_child(new widget(40, 50, 944, 640, string("")));
    w.add_child(new widget_caller_arg_button<widget, void (widget::*)(int), int>(&w, &widget::close, 1, (1024 - 128) / 2, 768 - 32 - 16, 128, 32, texts::get(105)));
    hsl.show(&w);
    w.run(0, false);
}
void show_halloffame_mission() { show_halloffame(hsl_mission); }
void show_halloffame_career() { show_halloffame(hsl_career); }

//
// check if a game is good enough for the high score list
//
void check_for_highscore(const game &gm) {
    if (sys().is_quit_requested())
        return;
    unsigned totaltons = 0;
    const list<sink_record> &sunken_ships = gm.get_sunken_ships();
    for (list<sink_record>::const_iterator it = sunken_ships.begin(); it != sunken_ships.end(); ++it) {
        totaltons += it->tons;
    }
    highscorelist &hsl = (/* check if game is career or mission fixme */ true) ? hsl_mission : hsl_career;
    unsigned points = totaltons /* compute points from tons etc here fixme */;

    widget w(0, 0, 1024, 768, texts::get(197), 0, "krupp_docks.jpg");
    widget *wbox = new widget(40, 50, 944, 640, string(""));
    w.add_child(wbox);
    w.add_child(new widget_caller_arg_button<widget, void (widget::*)(int), int>(&w, &widget::close, 1, (1024 - 128) / 2, 768 - 32 - 16, 128, 32, texts::get(105)));
    unsigned pos = hsl.get_listpos_for(points);
    if (hsl.is_good_enough(points)) {
        std::string txt = texts::get(199);
        if (pos == 0)
            txt += "\n\n" + texts::get(201);
        w.add_child(new widget_text(400, 200, 0, 0, txt));
        w.run(0, false);
        hsl.record(points, gm.get_player_info().name);
    } else {
        w.add_child(new widget_text(400, 200, 0, 0, texts::get(198)));
        w.run(0, false);
    }
    if (!sys().is_quit_requested())
        show_halloffame(hsl);
}

//
// show results after a game ended
//
void show_results_for_game(const game &gm) {
    if (sys().is_quit_requested())
        return;
    widget w(0, 0, 1024, 768, texts::get(124), 0, "sunken_destroyer.jpg");
    widget_list *wl = new widget_list(64, 64, 1024 - 64 - 64, 768 - 64 - 64);
    wl->set_column_width((1024 - 2 * 64) / 4);
    w.add_child(wl);
    w.add_child(new widget_caller_arg_button<widget, void (widget::*)(int), int>(&w, &widget::close, 1, (1024 - 128) / 2, 768 - 32 - 16, 128, 32, texts::get(105)));
    unsigned totaltons = 0;
    const list<sink_record> &sunken_ships = gm.get_sunken_ships();
    for (list<sink_record>::const_iterator it = sunken_ships.begin(); it != sunken_ships.end(); ++it) {
        ostringstream oss;
        oss << texts::numeric_from_date(it->dat) << "\t"
            << it->descr << "\t\t"
            << it->tons << " BRT";
        totaltons += it->tons;
        wl->append_entry(oss.str());
    }
    ostringstream os;
    os << "total: " << totaltons;
    wl->append_entry(os.str());
    w.run(0, false);
}

// main play loop
// fixme: clean this up!!!
static float s_display_fps = 0.0f;

game::run_state game__exec(game &gm, user_interface &ui) {
    dftd_startup_log("game__exec: starting...");
    unsigned frames = 1;
    unsigned lasttime = sys().millisec();
    unsigned lastframes = 1;
    double fpstime = 0;
    double totaltime = 0;
    double measuretime = 0.5; // Update FPS calculation every 0.5s

    dftd_startup_log("game__exec: resuming all sound...");
    ui.resume_all_sound();

    dftd_startup_log("game__exec: drawing initial frame with ui.display()...");
    // draw one initial frame
    ui.display();
    dftd_startup_log("game__exec: initial frame display finished.");

    ui.request_abort(false);

    dftd_startup_log("game__exec: entering running loop...");
    dftd_set_in_game_loop(true);
    while (gm.get_run_state() == game::running && !ui.abort_requested() && !sys().is_quit_requested()) {
        try {
            dftd_startup_log("game__exec: loop frame %u, polling event queue...", frames);
            list<game_event> events = sys().poll_event_queue();
            dftd_startup_log("game__exec: loop frame %u, %u events polled, processing input...", frames, (unsigned)events.size());

            // maybe limit input processing to 30 fps
            ui.process_input(events);
            dftd_startup_log("game__exec: loop frame %u, input processed.", frames);

            if (ui.abort_requested() || sys().is_quit_requested()) {
                dftd_startup_log("game__exec: abort or quit requested after input, exiting game loop.");
                break;
            }

            // this time_scaling is bad. hits may get computed wrong when time
            // scaling is too high. fixme
            unsigned thistime = sys().millisec();
            if (gm.get_freezetime_start() > 0)
                throw error("freeze_time() called without unfreeze_time() call");
            lasttime += gm.process_freezetime();
            unsigned time_scale = ui.time_scaling();
            double delta_time = (thistime - lasttime) / 1000.0; // * time_scale;
            totaltime += (thistime - lasttime) / 1000.0;
            lasttime = thistime;

            // next simulation step
            if (!ui.paused()) {
                dftd_startup_log("game__exec: loop frame %u, simulating (scale=%u, dt=%f)...", frames, time_scale, delta_time);
                for (unsigned j = 0; j < time_scale; ++j) {
                    gm.simulate(time_scale == 1 ? delta_time : (1.0 / 30.0));
                    // evaluate events of game, because they are cleared
                    // by next call of game::simulate and new ones are
                    // generated
                    const std::list<std::unique_ptr<event>> &events = gm.get_events();
                    for (auto it = events.begin(); it != events.end(); ++it) {
                        (*it)->evaluate(ui);
                    }
                }
                dftd_startup_log("game__exec: loop frame %u, simulate complete.", frames);
            } else {
                // When paused, yield CPU time instead of burning cycles rendering at maximum rate
                ::thread::sleep(15);
            }

            // fixme: make use of game::job interface, 3600/256 = 14.25 secs job period
            ui.set_time(gm.get_time());
            dftd_startup_log("game__exec: loop frame %u, calling ui.display()...", frames);
            ui.display();
            dftd_startup_log("game__exec: loop frame %u, ui.display() complete.", frames);

            // Record and compute fps
            if (totaltime - fpstime >= measuretime) {
                s_display_fps = static_cast<float>((frames - lastframes) / (totaltime - fpstime));
                fpstime = totaltime;
#if !defined(DFTD_NO_DEBUG_LOG)
                float refl_ms = 0.0f, foam_ms = 0.0f, wat_ms = 0.0f, terr_ms = 0.0f;
                extern void freeview_get_and_reset_timings(float &refl_ms, float &foam_ms, float &water_ms, float &terr_ms);
                freeview_get_and_reset_timings(refl_ms, foam_ms, wat_ms, terr_ms);
                dftd_startup_log("game__exec: fps %.1f (refl=%.1fms foam=%.1fms water=%.1fms terr=%.1fms)",
                                 s_display_fps, refl_ms, foam_ms, wat_ms, terr_ms);
#endif
                lastframes = frames;
            }

            // Draw in-game FPS counter if enabled in options
            if (cfg::instance().getb("show_fps") && font_arial) {
                char fps_buf[32];
                snprintf(fps_buf, sizeof(fps_buf), "FPS: %.1f", s_display_fps);
                sys().prepare_2d_drawing();
                font_arial->print(10, 10, fps_buf, color(255, 230, 50), true);
                sys().unprepare_2d_drawing();
            }

            ++frames;

            dftd_startup_log("game__exec: loop frame %u, swapping buffers...", frames - 1);
            sys().swap_buffers();
            dftd_startup_log("game__exec: loop frame %u, buffer swap complete.", frames - 1);
        } catch (const std::exception &e) {
            dftd_startup_log("game__exec: FATAL EXCEPTION in frame %u: %s", frames, e.what());
            break;
        } catch (...) {
            dftd_startup_log("game__exec: FATAL UNKNOWN EXCEPTION in frame %u!", frames);
            break;
        }
    }
    dftd_set_in_game_loop(false);

    ui.pause_all_sound();

    return gm.get_run_state(); // if player is killed, end game (1), else show menu (0)
}

//
// start and run a game, handle load/save (game menu), show results after game's end, delete game
//
void run_game(std::unique_ptr<game> gm) {
    // clear memory of menu widgets
    widget::unref_all_backgrounds();

    auto gametheme = std::make_unique<widget::theme>("widgetelements_game.png", "widgeticons_game.png",
                                                     font_vtremington12, color(182, 146, 137), color(240, 217, 127), color(64, 64, 64));
    dftd_startup_log("run_game: Starting game...");
    bool is_de = (texts::get_language_code() == "de");
    reset_loading_screen(is_de ? "Lade Feindfahrt..." : "Loading mission...", 10);
    // embrace user interface generation with right theme set!
    std::unique_ptr<widget::theme> tmp = widget::replace_theme(std::move(gametheme));
    dftd_startup_log("run_game: Creating user interface...");
    std::unique_ptr<user_interface> ui;
    try {
        ui.reset(user_interface::create(*gm));
    } catch (const std::exception &e) {
        dftd_startup_log("run_game: FATAL: user_interface::create threw exception: %s", e.what());
        throw;
    } catch (...) {
        dftd_startup_log("run_game: FATAL: user_interface::create threw unknown exception!");
        throw;
    }
    dftd_startup_log("run_game: User interface created successfully.");
    ui->add_message(cpu_has_altivec() ? "SYSTEM: CPU AltiVec/VMX acceleration ACTIVE" : "SYSTEM: CPU scalar mode (no AltiVec)");
    gametheme = widget::replace_theme(std::move(tmp));
    dftd_startup_log("run_game: Entering game loop...");
    while (true) {
        if (sys().is_quit_requested())
            break;
        dftd_startup_log("run_game: Replacing theme for game loop...");
        tmp = widget::replace_theme(std::move(gametheme));
        dftd_startup_log("run_game: Calling game__exec(*gm, *ui)...");
        game::run_state state = game__exec(*gm, *ui);
        dftd_startup_log("run_game: game__exec returned %d.", (int)state);
        gametheme = widget::replace_theme(std::move(tmp));

        if (sys().is_quit_requested())
            break;

        // if (state == 2) break;
        // SDL_ShowCursor(SDL_ENABLE);
        if (state != game::running) {
            //			if (state == game::mission_complete)

            if (state == game::player_killed) {
                music::instance().play_track(1, 500);
                widget w(0, 0, 1024, 768, "", 0, "killed.jpg");
                widget_menu *wm = new widget_menu(0, 0, 400, 40, texts::get(103));
                w.add_child(wm);
                wm->add_entry(texts::get(105), new widget_caller_arg_button<widget, void (widget::*)(int), int>(&w, &widget::close, 0, 0, 0, 0, 0));
                wm->align(0, 0);
                w.run(0, false);
            }

            //			if (state == game::contact_lost)

            // widget* w = widget::create_dialogue_ok_cancel("Quit game?", "");
            // int q = w->run();
            // delete w;
            // if (q == 1)
            break;
        } else {
            music::instance().play_track(1, 500);
            loadsavequit_dialogue dlg(gm.get());
            int q = dlg.run(0, false, dlg.get_gamename());
            if (sys().is_quit_requested())
                break;
            // replace game and ui if new game was loaded
            if (q == 2) {
                // fixme: ui doesn't need to get replaced, just give pointer to new
                // game to old ui, clear ui values and messages, finished...
                // this safes time to recompute map/water/sky etc.
                // this can only work if old and new game have same type
                // of player (and thus same type of ui)
                gm.reset();
                ui.reset();
                gm = std::make_unique<game>(cfg::instance(), log::instance(), dlg.get_gamefilename_to_load());
                // embrace user interface generation with right theme set!
                tmp = widget::replace_theme(std::move(gametheme));
                ui.reset(user_interface::create(*gm));
                gametheme = widget::replace_theme(std::move(tmp));
            }
            // replace ui after loading!!!!
            if (q == 1) {
                music::instance().play_track(1, 500);
                break;
            }
            if (q == 0) {
                // music::instance()._fade_out(1000);
            }
        }
        // SDL_ShowCursor(SDL_DISABLE);
    }
    if (!sys().is_quit_requested()) {
        show_results_for_game(*gm);
        check_for_highscore(*gm);
        // restore menu widgets
        widget::ref_all_backgrounds();
    }
}

//
// start and run a game editor, handle load/save (game menu), delete game
//
void run_game_editor(std::unique_ptr<game> gm) {
    // clear memory of menu widgets
    widget::unref_all_backgrounds();

    auto gametheme = std::make_unique<widget::theme>("widgetelements_game.png", "widgeticons_game.png",
                                                     font_vtremington12, color(182, 146, 137), color(240, 217, 127), color(64, 64, 64));
    reset_loading_screen();
    // embrace user interface generation with right theme set!
    std::unique_ptr<widget::theme> tmp = widget::replace_theme(std::move(gametheme));
    std::unique_ptr<user_interface> ui(user_interface::create(*gm));
    gametheme = widget::replace_theme(std::move(tmp));
    // game is initially running, so pause it.
    ui->toggle_pause();
    while (true) {
        if (sys().is_quit_requested())
            break;
        tmp = widget::replace_theme(std::move(gametheme));
        // 2006-12-01 doc1972 we should do some checks of the state if game exits
        /*game::run_state state =*/game__exec(*gm, *ui);
        gametheme = widget::replace_theme(std::move(tmp));

        if (sys().is_quit_requested())
            break;

        music::instance().play_track(1, 500);
        loadsavequit_dialogue dlg(gm.get());
        int q = dlg.run(0, false, dlg.get_gamename());
        // replace game and ui if new game was loaded
        if (q == 2) {
            // fixme: ui doesn't need to get replaced, just give pointer to new
            // game to old ui, clear ui values and messages, finished...
            // this safes time to recompute map/water/sky etc.
            // as long as class game holds a pointer to ui this is more difficult or
            // won't work.
            gm.reset();
            ui.reset();
            gm = std::make_unique<game_editor>(cfg::instance(), log::instance(), dlg.get_gamefilename_to_load());
            // embrace user interface generation with right theme set!
            tmp = widget::replace_theme(std::move(gametheme));
            ui.reset(user_interface::create(*gm));
            gametheme = widget::replace_theme(std::move(tmp));
        }
        // replace ui after loading!!!!
        if (q == 1) {
            music::instance().play_track(1, 500);
            break;
        }
        if (q == 0) {
            // music::instance()._fade_out(1000);
        }
    }

    // restore menu widgets
    widget::ref_all_backgrounds();
}

/** choose player data
    @returns false when cancelled
*/

class widget_image_select : public widget {
    std::list<std::string> imagenames;
    std::string extension;
    std::list<std::string>::iterator current;

  public:
    widget_image_select(int x, int y, int w, int h, const std::string &ext_,
                        const std::list<std::string> &imagenames_,
                        widget *parent_ = 0)
        : widget(x, y, w, h, "", parent_), imagenames(imagenames_),
          extension(ext_), current(imagenames.begin()) {
        if (imagenames.empty())
            throw error("can't use widget_image_select with empty list");
        background.load(imagecache(), *current + extension);
        add_child(new widget_text(20, 20, 0, 0, texts::get(117)));
    }
    virtual const std::string &get_current_imagename() const { return *current; }
    void next(int direction) {
        if (direction > 0) {
            current++;
            if (current == imagenames.end())
                current = imagenames.begin();
        } else {
            if (current == imagenames.begin())
                current = imagenames.end();
            current--;
        }
        background.load(imagecache(), *current + extension);
        redraw();
    }
    void draw() const {
        redrawme = false;
        vector2i p = get_pos();
        int bw = int(background->get_width());
        int bh = int(background->get_height());
        background->draw(p.x + size.x / 2 - bw / 2, p.y + size.y / 2 - bh / 2);
    }
    virtual void select_by_nr(unsigned n) {
        std::list<std::string>::iterator next = imagenames.begin();
        for (; n > 0; --n) {
            ++next;
            if (next == imagenames.end())
                next = imagenames.begin();
        }
        if (next != current) {
            current = next;
            background.load(imagecache(), *current + extension);
            redraw();
        }
    }
    virtual unsigned get_selected() const {
        std::list<std::string>::const_iterator it = imagenames.begin();
        unsigned n = 0;
        while (it != current) {
            ++n;
            ++it;
        }
        return n;
    }
};

class widget_button_next : public widget_button {
    int direction;
    widget_image_select *attached_widget;

  public:
    widget_button_next(int x, int y, int w, int h, int dir,
                       widget_image_select *att, const std::string &text_,
                       const std::string &bg_image_, widget *parent_ = 0)
        : widget_button(x, y, w, h, text_, parent_, bg_image_) {
        direction = dir;
        attached_widget = att;
    }
    void draw() const {
        redrawme = false;
        vector2i p = get_pos();
        int bw = int(background->get_width());
        int bh = int(background->get_height());

        colorf col = colorf(1.0, 1.0, 1.0, 1.0);
        if (mouseover != this)
            col = colorf(1.0, 1.0, 1.0, 0.75);
        background->draw(p.x + size.x / 2 - bw / 2, p.y + size.y / 2 - bh / 2, col);
    }
    void on_release() {
        pressed = false;
        attached_widget->next(direction);
    }
};

// used for easier access of data later
struct flotilla {
    unsigned nr;
    // later: type
    std::string insignia;
    std::string base;
    // later: operation areas
    std::vector<unsigned> subnrs;
    std::string description;
};

void show_flotilla_description(const std::string &infopopupdescr) {
    std::unique_ptr<widget> w(widget::create_dialogue_ok(0, "", infopopupdescr, 1024 * 3 / 4, 768 * 3 / 4));
    std::vector<Uint8> tmp(3);
    tmp[0] = 16;
    tmp[1] = 8;
    texture t(tmp, 1, 1, GL_RGB, texture::NEAREST, texture::REPEAT);
    w->set_background(&t);
    w->run();
}

bool choose_player_info(player_info &pi, const std::string &subtype, const date &gamedate) {
    widget w(0, 0, 1024, 768, "", 0, "playerselection_background.jpg");
    widget *w2 = new widget(40, 40, 500, 640, "");
    w.add_child(w2);
    w2->add_child(new widget_text(20, 20, 0, 0, texts::get(200)));
    widget_edit *wplayername = new widget_edit(20, 50, 460, 30, "Heinz Mustermann");
    w2->add_child(wplayername);

    std::vector<flotilla> availableflotillas;

    xml_doc flotilladb(get_data_dir() + "flotillas/available.xml");
    flotilladb.load();
    xml_elem eflotillas = flotilladb.child("flotillas");
    // compute which flotillas are available by time and submarine type
    // for every flotilla present a list of submarine IDs
    for (xml_elem::iterator it = eflotillas.iterate("flotilla"); !it.end(); it.next()) {
        xml_elem flot = it.elem();
        bool avail = false;
        std::string base = flot.attr("base");
        flotilla ft;
        for (xml_elem::iterator itt = flot.iterate("timeperiod"); !itt.end(); itt.next()) {
            xml_elem tp = itt.elem();
            date dfr = date(tp.attr("from"));
            date dut = date(tp.attr("until"));
            if (dfr <= gamedate && gamedate <= dut) {
                // flotilla is available by date
                for (xml_elem::iterator its = tp.iterate("subs"); !its.end(); its.next()) {
                    xml_elem subs = its.elem();
                    if (subtype.substr(std::string("submarine_").length()) == subs.attr("type")) {
                        // submarines are available
                        std::istringstream iss(subs.child_text());
                        while (!iss.eof()) {
                            unsigned nr;
                            iss >> nr;
                            ft.subnrs.push_back(nr);
                        }
                        if (!ft.subnrs.empty()) {
                            // list of available subs is not empty
                            avail = true;
                            break;
                        }
                    }
                }
                if (avail) {
                    if (tp.has_attr("base"))
                        base = tp.attr("base");
                    break;
                }
            }
        }
        if (avail) {
            ft.nr = flot.attru("nr");
            ft.insignia = flot.attr("sign");
            ft.base = base;
            ft.description = "not available, fix me";
            for (xml_elem::iterator itt = flot.iterate("description"); !itt.end(); itt.next()) {
                if (itt.elem().attr("lang") == texts::get_language_code()) {
                    ft.description = itt.elem().child_text();
                    break;
                }
            }
            availableflotillas.push_back(ft);
        }
    }
    if (availableflotillas.empty()) {
        log_warning("No flotilla available with these settings");
        return false;
    }
    // remove dummy flotilla if we have others
    if (availableflotillas.size() > 1) {
        for (std::vector<flotilla>::iterator it = availableflotillas.begin();
             it != availableflotillas.end(); ++it) {
            if (it->nr == 99) {
                availableflotillas.erase(it);
                break;
            }
        }
    }

    struct emblemselect : widget_image_select {
        widget_list *flst;
        emblemselect(int x, int y, int w, int h,
                     const std::string &ext_,
                     const std::list<std::string> &imagenames_)
            : widget_image_select(x, y, w, h, ext_, imagenames_), flst(0) {}
        void on_release() {
            widget_image_select::on_release();
            if (flst)
                flst->set_selected(get_selected());
        }
    };
    std::list<std::string> emblems;
    for (unsigned i = 0; i < availableflotillas.size(); ++i) {
        emblems.push_back(availableflotillas[i].insignia);
    }
    emblemselect *wemblem = new emblemselect(764 - 220 / 2, 572 - 32 - 300 / 2, 220, 300, ".png", emblems);

    struct flotlist : public widget_list {
        widget_image_select *wis;
        widget_list *wsns;
        widget_text *baseloc;
        const std::vector<flotilla> &availableflotillas;
        widget_button *infobut;
        std::string &infobutdesc;
        void on_sel_change() {
            wis->select_by_nr(std::max(0, get_selected()));
            wsns->clear();
            // iterate list of available flotillas and offer sub numbers
            int s = get_selected();
            if (s >= 0) {
                const std::vector<unsigned> &l = availableflotillas[s].subnrs;
                for (unsigned i = 0; i < l.size(); ++i) {
                    std::ostringstream oss;
                    oss << "U " << l[i];
                    wsns->append_entry(oss.str());
                }
                baseloc->set_text(availableflotillas[s].base);
                infobut->enable();
                infobutdesc = availableflotillas[s].description;
            } else {
                infobut->disable();
            }
        }
        flotlist(int x, int y, int w, int h, widget_image_select *w_, widget_list *ws,
                 widget_text *bl, const std::vector<flotilla> &af,
                 widget_button *ib, std::string &ibd)
            : widget_list(x, y, w, h), wis(w_), wsns(ws), baseloc(bl),
              availableflotillas(af), infobut(ib), infobutdesc(ibd) {}
    };
    widget_list *wsubnumber = new widget_list(20, 420, 460, 200);
    widget_text *baselocation = new widget_text(20, 350, 0, 0, "");
    w2->add_child(baselocation);
    std::string infopopupdescr;
    widget_button *infobutton = new widget_func_arg_button<void (*)(const std::string &), const std::string &>(&show_flotilla_description, infopopupdescr, 300, 320, 180, 40, texts::get(161));
    w2->add_child(infobutton);
    flotlist *wflotilla = new flotlist(20, 110, 460, 200, wemblem, wsubnumber,
                                       baselocation, availableflotillas,
                                       infobutton, infopopupdescr);
    std::string flotname = texts::get(164);
    for (unsigned i = 0; i < availableflotillas.size(); ++i) {
        std::string fn = flotname;
        fn.replace(fn.find("#"), 1, str(availableflotillas[i].nr));
        wflotilla->append_entry(fn);
    }
    w2->add_child(new widget_text(20, 80, 0, 0, texts::get(175)));
    w2->add_child(wflotilla);
    wemblem->flst = wflotilla;
    w2->add_child(new widget_text(20, 320, 0, 0, texts::get(163)));

    w2->add_child(new widget_text(20, 380, 0, 0, texts::get(176)));

    w2->add_child(wsubnumber);

    std::list<std::string> playerphotos;
    for (unsigned i = 1; i <= 11; ++i)
        playerphotos.push_back(std::string("player_photo") + str(i));

    w.add_child(new widget_text(661 + 20, 40 + 30, 0, 0, texts::get(162)));

    widget_image_select *wplayerphoto = new widget_image_select(661, 40 + 45, 205, 300, ".jpg|png", playerphotos);
    w.add_child(wplayerphoto);

    widget_button_next *widget_image_left_btn = new widget_button_next(661 - 35, 40 + 150, 25, 80, -1, wplayerphoto, "", "BG_btn_left.png");
    w.add_child(widget_image_left_btn);

    widget_button_next *widget_image_right_btn = new widget_button_next(661 + 215, 40 + 150, 25, 80, 1, wplayerphoto, "", "BG_btn_right.png");
    w.add_child(widget_image_right_btn);

    w.add_child(wemblem);

    widget_menu *wm = new widget_menu(40, 700, 0, 40, "", true);
    w.add_child(wm);
    wm->add_entry(texts::get(20), new widget_caller_arg_button<widget, void (widget::*)(int), int>(&w, &widget::close, 1, 70, 700, 400, 40));
    wm->add_entry(texts::get(19), new widget_caller_arg_button<widget, void (widget::*)(int), int>(&w, &widget::close, 2, 540, 700, 400, 40));
    wm->adjust_buttons(944);
    int result = w.run(0, false);
    if (result == 2) {
        pi.name = wplayername->get_text();
        pi.flotilla = availableflotillas[std::max(0, wflotilla->get_selected())].nr;
        pi.submarineid = wsubnumber->get_selected_entry();
        pi.photo = wplayerphoto->get_current_imagename().substr(std::string("player_photo").length());
        // log_debug(player_name<<","<<player_flotilla<<","<<player_subnumber<<","<<player_photo);
        return true;
    }
    return false;
}

//
// create a custom convoy mission
//
void create_convoy_mission() {
    widget w(0, 0, 1024, 768, texts::get(9), 0, "scopewatcher.jpg");
    w.add_child(new widget_text(40, 60, 0, 0, texts::get(16)));
    widget_list *wsubtype = new widget_list(40, 90, 200, 200);
    w.add_child(wsubtype);
    w.add_child(new widget_text(280, 60, 0, 0, texts::get(84)));
    widget_list *wcvsize = new widget_list(280, 90, 200, 200);
    w.add_child(wcvsize);
    w.add_child(new widget_text(520, 60, 0, 0, texts::get(88)));
    widget_list *wescortsize = new widget_list(520, 90, 200, 200);
    w.add_child(wescortsize);
    w.add_child(new widget_text(760, 60, 0, 0, texts::get(90)));
    widget_list *wtimeofday = new widget_list(760, 90, 200, 200);
    w.add_child(wtimeofday);
    w.add_child(new widget_text(40, 310, 0, 0, texts::get(62)));
    widget_list *wtimeperiod = new widget_list(40, 340, 640, 200);
    w.add_child(wtimeperiod);
    wsubtype->append_entry(texts::get(17));
    //	wsubtype->append_entry(texts::get(174));
    //	wsubtype->append_entry(texts::get(18));
    wsubtype->append_entry(texts::get(800));
    wsubtype->append_entry(texts::get(801));
    wsubtype->append_entry(texts::get(802));
    wsubtype->append_entry(texts::get(803));
    wcvsize->append_entry(texts::get(85));
    wcvsize->append_entry(texts::get(86));
    wcvsize->append_entry(texts::get(87));
    wescortsize->append_entry(texts::get(89));
    wescortsize->append_entry(texts::get(85));
    wescortsize->append_entry(texts::get(86));
    wescortsize->append_entry(texts::get(87));
    wtimeofday->append_entry(texts::get(91));
    wtimeofday->append_entry(texts::get(92));
    wtimeofday->append_entry(texts::get(93));
    wtimeofday->append_entry(texts::get(94));
    wtimeperiod->append_entry(texts::get(63));
    wtimeperiod->append_entry(texts::get(64));
    wtimeperiod->append_entry(texts::get(65));
    wtimeperiod->append_entry(texts::get(66));
    wtimeperiod->append_entry(texts::get(67));
    wtimeperiod->append_entry(texts::get(68));
    wtimeperiod->append_entry(texts::get(69));
    wtimeperiod->append_entry(texts::get(70));

    widget_menu *wm = new widget_menu(40, 700, 0, 40, "", true);
    w.add_child(wm);
    wm->add_entry(texts::get(20), new widget_caller_arg_button<widget, void (widget::*)(int), int>(&w, &widget::close, 1, 70, 700, 400, 40));
    wm->add_entry(texts::get(19), new widget_caller_arg_button<widget, void (widget::*)(int), int>(&w, &widget::close, 2, 540, 700, 400, 40));
    wm->adjust_buttons(944);
    while (true) {
        if (sys().is_quit_requested())
            break;
        int result = w.run(0, false);
        if (sys().is_quit_requested())
            break;
        if (result == 2) { // start game
            string st;
            switch (wsubtype->get_selected()) {
            case 0:
                st = "submarine_VIIc";
                break;
                //			case 1: st = "submarine_IXc40"; break;
                //			case 2: st = "submarine_XXI"; break;
            case 1:
                st = "submarine_IIa";
                break;
            case 2:
                st = "submarine_IIb";
                break;
            case 3:
                st = "submarine_IIc";
                break;
            case 4:
                st = "submarine_IId";
                break;
            }

            // compute mission time (date)
            date datebegin, dateend;
            switch (wtimeperiod->get_selected()) {
            case 0:
                datebegin = date(1939, 9, 1);
                dateend = date(1940, 5, 31);
                break;
            case 1:
                datebegin = date(1940, 6, 1);
                dateend = date(1941, 3, 31);
                break;
            case 2:
                datebegin = date(1941, 4, 1);
                dateend = date(1941, 12, 31);
                break;
            case 3:
                datebegin = date(1942, 1, 1);
                dateend = date(1942, 6, 30);
                break;
            case 4:
                datebegin = date(1942, 7, 1);
                dateend = date(1942, 12, 31);
                break;
            case 5:
                datebegin = date(1943, 1, 1);
                dateend = date(1943, 5, 31);
                break;
            case 6:
                datebegin = date(1943, 6, 1);
                dateend = date(1944, 6, 30);
                break;
            case 7:
                datebegin = date(1944, 7, 1);
                dateend = date(1945, 5, 8);
                break;
            }
            double tpr = rnd();
            double time = datebegin.get_time() * (1.0 - tpr) + dateend.get_time() * tpr;
            time = floor(time / 86400) * 86400; // set to begin of day
            date gamedate = date(unsigned(time));

            // show player gui screen
            // use strings for all data, more extendable
            player_info pi;
            bool ok = choose_player_info(pi, st, gamedate);
            if (!ok)
                continue;

            // reset loading screen here to show user we are doing something
            // fixme: give data to game! player data. maybe combine that to a struct!
            reset_loading_screen();
            run_game(std::make_unique<game>(cfg::instance(), log::instance(), st,
                                            wcvsize->get_selected(),
                                            wescortsize->get_selected(),
                                            wtimeofday->get_selected(),
                                            gamedate,
                                            pi));
            break;
        } else {
            break;
        }
    }
}

//
// choose a historical mission
//
void choose_historical_mission() {
    vector<string> missions;

    // read missions
    unsigned nr_missions = 0;
    {
        directory missiondir(get_mission_dir());
        while (true) {
            string e = missiondir.read();
            if (e.empty())
                break;
            if (e.length() > 4 && e.substr(e.length() - 4) == ".xml") {
                missions.push_back(e);
                ++nr_missions;
            }
        }
    }

    // read descriptions, set up windows
    widget w(0, 0, 1024, 768, texts::get(10), 0, "sunderland.jpg");
    vector<string> descrs;
    struct msnlist : public widget_list {
        const vector<string> &descrs;
        widget_text *wdescr;
        void on_sel_change() {
            int sel = get_selected();
            if (sel >= 0 && sel < int(descrs.size()))
                wdescr->set_text(descrs[sel]);
            else
                wdescr->set_text("");
        }
        msnlist(int x, int y, int w, int h, const vector<string> &descrs_, widget_text *wdescr_) : widget_list(x, y, w, h), descrs(descrs_), wdescr(wdescr_) {}
        ~msnlist() {}
    };
    widget_text *wdescr = new widget_text(40, 380, 1024 - 80, 300, "", 0, true);
    widget_list *wmission = new msnlist(40, 60, 1024 - 80, 300, descrs, wdescr);
    w.add_child(wmission);
    w.add_child(wdescr);
    // Note:
    // Missions have the same format like savegames, except that the head xml node
    // has an additional child node <description> with multi-lingual descriptions of the mission.
    for (unsigned i = 0; i < nr_missions; ++i) {
        string short_desc;
        string long_desc;
        try {
            xml_doc doc(get_mission_dir() + missions[i]);
            doc.load();
            xml_elem edftdmission = doc.child("dftd-mission");
            if (edftdmission.has_child("description")) {
                xml_elem edescription = edftdmission.child("description");
                string first_short;
                string en_short;
                for (xml_elem::iterator it = edescription.iterate("short"); !it.end(); it.next()) {
                    string l = it.elem().attr("lang");
                    string t = it.elem().child_text();
                    if (first_short.empty()) first_short = t;
                    if (l == "en") en_short = t;
                    if (l == texts::get_language_code()) {
                        short_desc = t;
                        break;
                    }
                }
                if (short_desc.empty()) {
                    short_desc = !en_short.empty() ? en_short : first_short;
                }

                string first_long;
                string en_long;
                for (xml_elem::iterator it = edescription.iterate("long"); !it.end(); it.next()) {
                    string l = it.elem().attr("lang");
                    string t = it.elem().child_text();
                    if (first_long.empty()) first_long = t;
                    if (l == "en") en_long = t;
                    if (l == texts::get_language_code()) {
                        long_desc = t;
                        break;
                    }
                }
                if (long_desc.empty()) {
                    long_desc = !en_long.empty() ? en_long : first_long;
                }
            }
        } catch (...) {
        }

        if (short_desc.empty()) short_desc = missions[i];
        if (long_desc.empty()) long_desc = short_desc;

        wmission->append_entry(short_desc);
        descrs.push_back(long_desc);
    }
    wmission->on_sel_change();

    widget_menu *wm = new widget_menu(40, 700, 0, 40, "", true);
    w.add_child(wm);
    wm->add_entry(texts::get(20), new widget_caller_arg_button<widget, void (widget::*)(int), int>(&w, &widget::close, 1, 70, 700, 400, 40));
    wm->add_entry(texts::get(19), new widget_caller_arg_button<widget, void (widget::*)(int), int>(&w, &widget::close, 2, 70, 700, 400, 40));
    wm->adjust_buttons(944);
    int result = w.run(0, false);
    if (result == 2) { // start game
        int sel = wmission->get_selected();
        if (sel < 0 || sel >= int(missions.size())) {
            return;
        }
        bool is_de = (texts::get_language_code() == "de");
        reset_loading_screen(is_de ? "Lade Mission..." : "Loading mission...", 25);
        std::unique_ptr<game> gm;
        try {
            gm = std::make_unique<game>(cfg::instance(), log::instance(), get_mission_dir() + missions[sel]);
        } catch (const std::exception &e) {
            dftd_startup_log("ERROR loading game: %s", e.what());
            log_warning("error loading game: " << e.what());
            SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR,
                                     is_de ? "Fehler beim Laden" : "Load Error",
                                     e.what(), nullptr);
            return;
        } catch (...) {
            dftd_startup_log("ERROR loading game: unknown exception!");
            SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR,
                                     is_de ? "Fehler beim Laden" : "Load Error",
                                     is_de ? "Unbekannter Fehler beim Laden der Mission." : "Unknown error while loading mission.",
                                     nullptr);
            return;
        }
        run_game(std::move(gm));
    }
}

//
// choose a saved game
//
void choose_saved_game() {
    loadsavequit_dialogue dlg(0);
    int q = dlg.run(0, false, dlg.get_gamename());
    if (q == 0)
        return;
    if (q == 2) {
        bool is_de = (texts::get_language_code() == "de");
        reset_loading_screen(is_de ? "Lade Spielstand..." : "Loading saved game...", 25);
        std::unique_ptr<game> gm;
        try {
            gm = std::make_unique<game>(cfg::instance(), log::instance(), dlg.get_gamefilename_to_load());
        } catch (const std::exception &e) {
            dftd_startup_log("ERROR loading saved game: %s", e.what());
            log_warning("error loading saved game: " << e.what());
            SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR,
                                     is_de ? "Fehler beim Laden" : "Load Error",
                                     e.what(), nullptr);
            return;
        } catch (...) {
            dftd_startup_log("ERROR loading saved game: unknown exception!");
            SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR,
                                     is_de ? "Fehler beim Laden" : "Load Error",
                                     is_de ? "Unbekannter Fehler beim Laden des Spielstands." : "Unknown error while loading saved game.",
                                     nullptr);
            return;
        }
        run_game(std::move(gm));
    }
}

void menu_single_mission() {
    widget w(0, 0, 1024, 768, "", 0, "titlebackgr.jpg");
    widget_menu *wm = new widget_menu(0, 0, 400, 40, texts::get(21));
    w.add_child(wm);
    //	wm->add_entry(texts::get(8), new widget_func_button<void (*)()>(&menu_notimplemented, 0, 0, 0, 0));
    wm->add_entry(texts::get(9), new widget_func_button<void (*)()>(&create_convoy_mission, 0, 0, 0, 0));
    wm->add_entry(texts::get(10), new widget_func_button<void (*)()>(&choose_historical_mission, 0, 0, 0, 0));
    wm->add_entry(texts::get(118), new widget_func_button<void (*)()>(&choose_saved_game, 0, 0, 0, 0));
    wm->add_entry(texts::get(11), new widget_caller_arg_button<widget, void (widget::*)(int), int>(&w, &widget::close, 0, 0, 0, 0, 0));
    wm->align(0, 0);
    w.run(0, false);
}

void menu_mission_editor() {
    widget w(0, 0, 1024, 768, texts::get(222), 0, "scopewatcher.jpg");
    w.add_child(new widget_text(40, 60, 944, 0, texts::get(223)));

    /*
            w.add_child(new widget_text(40, 60, 0, 0, texts::get(16)));
            widget_list* wsubtype = new widget_list(40, 90, 200, 200);
            w.add_child(wsubtype);
            wsubtype->append_entry(texts::get(17));
            wsubtype->append_entry(texts::get(174));
            wsubtype->append_entry(texts::get(18));
    */

    widget_menu *wm = new widget_menu(40, 700, 0, 40, "", true);
    w.add_child(wm);
    wm->add_entry(texts::get(20), new widget_caller_arg_button<widget, void (widget::*)(int), int>(&w, &widget::close, 1, 540, 700, 400, 40));
    wm->add_entry(texts::get(222), new widget_caller_arg_button<widget, void (widget::*)(int), int>(&w, &widget::close, 2, 70, 700, 400, 40));
    wm->adjust_buttons(944);
    int result = w.run(0, false);
    if (result == 2) { // start editor
                       /*
                                       string st;
                                       switch (wsubtype->get_selected()) {
                                               case 0: st = "submarine_VIIc"; break;
                                               case 1: st = "submarine_IXc40"; break;
                                               case 2: st = "submarine_XXI"; break;
                                       }
                       */
        // reset loading screen here to show user we are doing something
        reset_loading_screen();
        run_game_editor(std::make_unique<game_editor>(cfg::instance(), log::instance(), date(1939, 9, 1) /*st*/));
    }
}

void menu_select_language() {
    cfg &config = cfg::instance();

    widget w(0, 0, 1024, 768, "", 0, "titlebackgr.jpg");
    widget_menu *wm = new widget_menu(0, 0, 400, 40, texts::get(26));

    struct lgclist : public widget_list {
        cfg &config_ref;

        void on_sel_change() {
            texts::set_language(get_selected());
            config_ref.set("language", get_selected());
        }
        lgclist(int x, int y, int w, int h, cfg &cfg_ref) : widget_list(x, y, w, h), config_ref(cfg_ref) {}
    };

    widget_list *wlg = new lgclist(0, 0, 400, 400, config);
    unsigned nl = texts::get_nr_of_available_languages();
    for (unsigned i = 0; i < nl; ++i) {
        wlg->append_entry(texts::get(i, texts::languages));
    }
    wlg->set_selected(texts::get_current_language_nr());

    widget_button *wcb = new widget_caller_arg_button<widget, void (widget::*)(int), int>(&w, &widget::close, 0, 0, 0, 400, 40, texts::get(11));

    w.add_child(wm);
    w.add_child(wlg);
    w.add_child(wcb);
    wlg->align(0, 0);
    vector2i wlgp = wlg->get_pos();
    vector2i wlgs = wlg->get_size();
    wm->set_pos(vector2i(wlgp.x, wlgp.y - 60));
    wcb->set_pos(vector2i(wlgp.x, wlgp.y + wlgs.y + 20));

    w.run(0, false);
}

//
// options:
// - set resolution
// - enable bump mapping
// - enable wave foam
// - detail for map/terrain/water (wave tile detail, # of wave tiles, global terrain detail, wave bump map detail)
// - set fullscreen
// - invert mouse in view
// - set keys
//

void apply_mode(widget_list *wlg) {
    cfg &config = cfg::instance();

    unsigned width, height;

    string wks = wlg->get_selected_entry();

    height = atoi(wks.substr(wks.rfind("x") + 1).c_str());
    width = atoi(wks.substr(0, wks.rfind("x")).c_str());

    // try to set video mode BEFORE writing to config file, so that if video mode
    // is broken, user is not forced to same mode again on restart
    try {
        sys().set_video_mode(width, height, sys().is_fullscreen_mode(), true);
        config.set("screen_res_y", int(height));
        config.set("screen_res_x", int(width));
        glClearColor(0, 0, 0, 0);
    } catch (exception &e) {
        log_warning("Video mode setup failed: " << e.what());
    }
}

void menu_resolution() {
    const list<vector2i> &available_resolutions = sys().get_available_resolutions();

    widget w(0, 0, 1024, 768, "", 0, "titlebackgr.jpg");
    widget_menu *wm = new widget_menu(0, 0, 400, 40, texts::get(106));

    widget_list *wlg = new widget_list(0, 0, 400, 400);

    vector2i curr_res(sys().get_res_x(), sys().get_res_y());
    unsigned curr_entry = 0;
    unsigned i = 0;
    for (list<vector2i>::const_iterator it = available_resolutions.begin(); it != available_resolutions.end(); ++it) {
        wlg->append_entry(str(it->x) + "x" + str(it->y));
        if (*it == curr_res)
            curr_entry = i;
        ++i;
    }
    wlg->set_selected(curr_entry);

    widget_button *wcb = new widget_caller_arg_button<widget, void (widget::*)(int), int>(&w, &widget::close, 0, 0, 0, 400, 40, texts::get(20));
    w.add_child(new widget_func_arg_button<void (*)(widget_list *), widget_list *>(&apply_mode, wlg, 516, 604, 452, 40, texts::get(106)));

    w.add_child(wm);
    w.add_child(wlg);
    w.add_child(wcb);
    wlg->align(0, 0);
    vector2i wlgp = wlg->get_pos();
    vector2i wlgs = wlg->get_size();
    wm->set_pos(vector2i(wlgp.x, wlgp.y - 60));
    wcb->set_pos(vector2i(wlgp.x - 260, wlgp.y + wlgs.y + 20));
    w.run(0, false);
}

void configure_key(widget_list *wkeys) {
    cfg &config = cfg::instance();

    struct confkey_widget : public widget {
        widget_text *keyname;
        unsigned keynr;
        cfg &config_ref;

        void on_char(const key_sym &ks) override {
            if (ks.sym == SDLK_ESCAPE) {
                close(0);
                return;
            }
            bool ctrl = (ks.mod & (DFTD_KMOD_LCTRL | DFTD_KMOD_RCTRL)) != 0;
            bool alt = (ks.mod & (DFTD_KMOD_LALT | DFTD_KMOD_RALT | DFTD_KMOD_MODE /* Alt Gr */)) != 0;
            bool shift = (ks.mod & (DFTD_KMOD_LSHIFT | DFTD_KMOD_RSHIFT)) != 0;
            config_ref.set_key(keynr, ks.sym, ctrl, alt, shift);
            keyname->set_text(config_ref.getkey(keynr).get_name());
            redraw();
        }
        confkey_widget(int x, int y, int w, int h, const string &text_, widget *parent_,
                       const std::string &backgrimg, unsigned sel, cfg &cfg_ref)
            : widget(x, y, w, h, text_, parent_, backgrimg), keynr(sel), config_ref(cfg_ref) {
            keyname = new widget_text(40, 80, 432, 40, config_ref.getkey(keynr).get_name());
            add_child(keyname);
            add_child(new widget_text(40, 120, 432, 40, texts::get(217)));
        }
        ~confkey_widget() {}
    };
    unsigned sel = wkeys->get_selected();
    confkey_widget w(256, 256, 512, 256, texts::get(216), 0, "", sel, config);
    string wks = wkeys->get_selected_entry();
    wks = wks.substr(0, wks.find("\t"));
    w.add_child(new widget_text(40, 40, 432, 32, wks));
    w.run(0, true);
    wkeys->set_entry(sel, texts::get(sel + 600) + string("\t") + config.getkey(sel).get_name());
}

void menu_configure_keys() {
    cfg &config = cfg::instance();

    widget w(0, 0, 1024, 768, texts::get(214), 0, "titlebackgr.jpg");
    widget_list *wkeys = new widget_list(40, 50, 944, 640);
    wkeys->set_column_width(700);
    w.add_child(wkeys);

    for (unsigned i = 600; i < 600 + NR_OF_KEY_IDS; ++i) {
        cfg::key k = config.getkey(i - 600);
        wkeys->append_entry(texts::get(i) + string("\t") + k.get_name());
    }

    // fixme: handle undefined keys!
    // fixme: check for double keys!

    w.add_child(new widget_caller_arg_button<widget, void (widget::*)(int), int>(&w, &widget::close, 0, 40, 708, 452, 40, texts::get(20)));
    w.add_child(new widget_func_arg_button<void (*)(widget_list *), widget_list *>(&configure_key, wkeys, 532, 708, 452, 40, texts::get(215)));
    w.run(0, false);
}

void menu_opt_input() {
    widget w(0, 0, 1024, 768, "", 0, "titlebackgr.jpg");
    widget_menu *wm = new widget_menu(0, 0, 400, 40, texts::get(705));
    w.add_child(wm);

    wm->add_entry(texts::get(214), new widget_func_button<void (*)()>(&menu_configure_keys, 0, 0, 0, 0));
    wm->add_entry(texts::get(709), new widget_func_button<void (*)()>(&menu_notimplemented, 0, 0, 0, 0)); // TODO

    wm->add_entry(texts::get(11), new widget_caller_arg_button<widget, void (widget::*)(int), int>(&w, &widget::close, 0, 0, 0, 0, 0));
    wm->align(0, 0);
    w.run(0, false);
}

void menu_opt_audio() {
    menu_notimplemented(); // TODO
}
void menu_opt_video() {
    cfg &config = cfg::instance();

    unsigned wd = 400;
    unsigned gap = 112;
    unsigned x = 56;
    unsigned y = 150;
    unsigned right = x + wd + gap;

    // make widgets
    widget w(0, 0, 1024, 768, "", 0, "titlebackgr.jpg");
    widget_menu *wm = new widget_menu(x, y, wd, 40, texts::get(707));

    widget_button *resolution = new widget_func_button<void (*)()>(&menu_resolution, x, y + 60, wd, 40, texts::get(106));
    widget_checkbox *vsync = new widget_checkbox(right, y + 60, wd, 40, config.getb("vsync"), texts::get(720));

    widget_slider *terrain_lod = new widget_slider(x, y + 120, wd, 80, texts::get(112), 3, 9, config.geti("terrain_detail"), 1);
    widget_checkbox *tex_compress = new widget_checkbox(right, y + 120, wd, 40, config.getb("use_compressed_textures"), texts::get(721));
    bool is_de = (texts::get_language_code() == "de");
    widget_checkbox *show_fps = new widget_checkbox(right, y + 165, wd, 40, config.getb("show_fps"), is_de ? "FPS-Anzeige einblenden" : "Show FPS counter");

    widget_list *wfx_quality = new widget_list(x + (wd / 2), y + 220, wd / 2, 80);
    widget_text *wfx_quality_txt = new widget_text(x, y + 220, wd / 2, 20, texts::get(713));
    widget_list *w_postprocessing = new widget_list(right + (wd / 2), y + 220, wd / 2, 80);
    widget_text *w_postprocessing_txt = new widget_text(right, y + 220, wd / 2, 20, texts::get(714));

    widget_list *anisotropic_level = new widget_list(x + (wd / 2), y + 320, wd / 2, 80);
    widget_text *anisotropic_level_txt = new widget_text(x, y + 320, wd / 2, 20, texts::get(722));
    widget_list *anti_aliasing_level = new widget_list(right + (wd / 2), y + 320, wd / 2, 80);
    widget_text *anti_aliasing_level_txt = new widget_text(right, y + 320, wd / 2, 20, texts::get(723));

    widget_button *wcb = new widget_caller_arg_button<widget, void (widget::*)(int), int>(&w, &widget::close, 0, x, y + 420, wd, 40, texts::get(20));

    // insert values
    wfx_quality->append_entry(texts::get(710));
    wfx_quality->append_entry(texts::get(711));
    wfx_quality->append_entry(texts::get(712));
    wfx_quality->set_selected(config.geti("sfx_quality"));

    w_postprocessing->append_entry(texts::get(715));
    w_postprocessing->append_entry(texts::get(716));
    w_postprocessing->append_entry(texts::get(717));
    w_postprocessing->set_selected(config.geti("postprocessing"));

    float max_ani = 1.0;
    glGetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT, &max_ani);
    anisotropic_level->append_entry(texts::get(724));

    if (1.0f != max_ani) {
        unsigned count = 0;
        float base = 1.0f;

        while (base < max_ani) {
            base *= 2;
            count++;

            anisotropic_level->append_entry(str(base));

            if (config.getf("anisotropic_level") == base)
                anisotropic_level->set_selected(count);
        }
    }

    anti_aliasing_level->append_entry(texts::get(724));
    anti_aliasing_level->set_selected(0); // TODO: FIXME

    // add to root
    w.add_child(wm);

    w.add_child(resolution);
    w.add_child(vsync);

    w.add_child(terrain_lod);
    w.add_child(tex_compress);
    w.add_child(show_fps);

    w.add_child(w_postprocessing);
    w.add_child(w_postprocessing_txt);
    w.add_child(wfx_quality);
    w.add_child(wfx_quality_txt);

    w.add_child(anisotropic_level);
    w.add_child(anisotropic_level_txt);
    w.add_child(anti_aliasing_level);
    w.add_child(anti_aliasing_level_txt);

    w.add_child(wcb);

    w.run(0, false);

    // save settings
    config.set("vsync", vsync->is_checked());

    config.set("terrain_detail", terrain_lod->get_curr_value());
    config.set("use_compressed_textures", tex_compress->is_checked());
    config.set("show_fps", show_fps->is_checked());

    config.set("sfx_quality", wfx_quality->get_selected());
    config.set("postprocessing", w_postprocessing->get_selected());
    // TODO: need to update postproc. code to use this int instead of the previous two booleans.

    if (0 == anisotropic_level->get_selected()) {
        config.set("use_ani_filtering", false);
        config.set("anisotropic_level", 1.0f);
    } else {
        config.set("use_ani_filtering", true);

        glGetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT, &max_ani);
        unsigned max_list = anisotropic_level->get_listsize() - 1;
        unsigned selected = anisotropic_level->get_selected();

        for (unsigned ix = max_list; ix > selected; ix--)
            max_ani /= 2.0f;

        config.set("anisotropic_level", max_ani);
    }

    if (0 == anti_aliasing_level->get_selected()) {
        config.set("use_multisampling", false);
    } else {
        config.set("use_multisampling", true);
        // TODO - implement this, kind of held back by the lack of easy of detecting suitable/MAX values.
        // config.set("multisampling_level", ??? );
    }

    // TODO: something about this ? we need to convert or update existing code that uses the boolean use_hqsfx
    //	config.set("use_hqsfx", whqsfx->is_checked());
    //	glsl_shader::enable_hqsfx = whqsfx->is_checked();
}

void menu_opt_network() {
    menu_notimplemented(); // TODO
}

void menu_options() {
    widget w(0, 0, 1024, 768, "", 0, "titlebackgr.jpg");
    int retval = 1;
    do {
        w.remove_children();
        widget_menu *wm = new widget_menu(0, 0, 400, 40, texts::get(29));
        w.add_child(wm);

        wm->add_entry(texts::get(705), new widget_func_button<void (*)()>(&menu_opt_input, 0, 0, 0, 0));
        wm->add_entry(texts::get(706), new widget_func_button<void (*)()>(&menu_opt_audio, 0, 0, 0, 0));
        wm->add_entry(texts::get(707), new widget_func_button<void (*)()>(&menu_opt_video, 0, 0, 0, 0));
        wm->add_entry(texts::get(708), new widget_func_button<void (*)()>(&menu_opt_network, 0, 0, 0, 0));
        wm->add_entry(texts::get(26), new widget_caller_arg_button<widget, void (widget::*)(int), int>(&w, &widget::close, 2, 0, 0, 0, 0));

        wm->add_entry(texts::get(11), new widget_caller_arg_button<widget, void (widget::*)(int), int>(&w, &widget::close, 0, 0, 0, 0, 0));
        wm->align(0, 0);
        if (sys().is_quit_requested())
            break;
        retval = w.run(0, false);
        if (sys().is_quit_requested())
            break;
        if (retval == 2) {
            menu_select_language();
        }
    } while (retval == 2);
}

// vessel preview
class vessel_view {
    list<string> shipnames;
    list<string>::iterator current;
    set<string> modellayouts;
    set<string>::iterator currentlayout;
    widget_text *wdesc;
    // note! this is not destructed by this class...
    widget_3dview *w3d;
    std::unique_ptr<model> load_model() {
        xml_doc doc(data_file().get_filename(*current));
        doc.load();
        string mdlname = doc.first_child().child("classification").attr("modelname");
        for (xml_elem::iterator it = doc.first_child().child("description").iterate("near"); !it.end(); it.next()) {
            if (it.elem().attr("lang") == texts::get_language_code()) {
                if (wdesc) {
                    wdesc->set_text_and_resize(it.elem().child_text());
                    int y = wdesc->get_pos().y;
                    wdesc->align(0, -1);
                    wdesc->move_pos(vector2i(0, y));
                }
                break;
            }
        }
        auto mdl = std::make_unique<model>(data_file().get_path(*current) + mdlname);
        // register and set default layout.
        mdl->register_layout();
        mdl->set_layout();
        modellayouts.clear();
        mdl->get_all_layout_names(modellayouts);
        currentlayout = modellayouts.begin();
        return mdl;
    }

  public:
    vessel_view(widget_text *wdesc_ = 0)
        : current(shipnames.end()), wdesc(wdesc_), w3d(0) {
        color bgcol(50, 50, 150);
        shipnames = data_file().get_ship_list();
        list<string> tmp = data_file().get_submarine_list();
        shipnames.splice(shipnames.end(), tmp);
        tmp = data_file().get_airplane_list();
        shipnames.splice(shipnames.end(), tmp);
        current = shipnames.begin();
        w3d = new widget_3dview(20, 0, 1024 - 2 * 20, 700 - 32 - 16, load_model(), bgcol);
        vector3f lightdir = vector3f(angle(143).cos(), angle(143).sin(), angle(49.5).tan()).normal();
        w3d->set_light_dir(vector4f(lightdir.x, lightdir.y, lightdir.z, 0));
        w3d->set_light_color(color(233, 221, 171));
    }
    widget_3dview *get_w3d() { return w3d; }
    void next() {
        ++current;
        if (current == shipnames.end())
            current = shipnames.begin();
        w3d->set_model(load_model());
        w3d->redraw();
    }
    void previous() {
        if (current == shipnames.begin())
            current = shipnames.end();
        --current;
        w3d->set_model(load_model());
        w3d->redraw();
    }
    void switchlayout() {
        ++currentlayout;
        if (currentlayout == modellayouts.end())
            currentlayout = modellayouts.begin();
        // registering the same layout multiple times does not hurt, no problem
        w3d->get_model()->register_layout(*currentlayout);
        w3d->get_model()->set_layout(*currentlayout);
        w3d->redraw();
    }
};

void menu_show_vessels() {
    widget w(0, 0, 1024, 768, texts::get(24), 0, "threesubs.jpg");
    widget_text *wt = new widget_text(0, 50, 1024, 32, "", 0, true);
    w.add_child(wt);
    widget_menu *wm = new widget_menu(0, 700, 140, 32, "" /*texts::get(110)*/, true);
    w.add_child(wm);
    vessel_view vw(wt);
    w.add_child(vw.get_w3d());

    wm->add_entry(texts::get(115), new widget_caller_button<vessel_view, void (vessel_view::*)()>(&vw, &vessel_view::next));
    wm->add_entry(texts::get(116), new widget_caller_button<vessel_view, void (vessel_view::*)()>(&vw, &vessel_view::previous));
    // fixme: disable butten when there is only one layout
    wm->add_entry(texts::get(246), new widget_caller_button<vessel_view, void (vessel_view::*)()>(&vw, &vessel_view::switchlayout));
    wm->add_entry(texts::get(117), new widget_caller_arg_button<widget, void (widget::*)(int), int>(&w, &widget::close, 0));
    wm->adjust_buttons(984);

    w.run(0, false);
}

bool file_exists(const string &fn) {
    ifstream in(fn.c_str(), ios::in | ios::binary);
    return in.good();
}

bool set_dir(const string &dir, string &setdir) {
    // check if it is a directory.
    if (!is_directory(dir)) {
        return false;
    }
    // append separator if needed
    if (dir[dir.length() - 1] != '/') {
        setdir = dir + "/";
    } else {
        setdir = dir;
    }
    return true;
}

int mymain(list<string> &args) {
    // report critical errors (on Unix/Posix systems)
    install_segfault_handler();
    log_cpu_features();

#ifdef __amigaos4__
    // GL4ES configuration for AmigaOS 4
    setenv("LIBGL_ES", "2", 1);
    setenv("LIBGL_GL", "21", 1);
    setenv("LIBGL_NORMALIZE", "1", 1);

    if (get_data_dir() == "data/" || !is_directory(get_data_dir())) {
        set_data_dir("PROGDIR:data/");
    }
    dftd_startup_log("Data directory set to: %s", get_data_dir().c_str());
#endif

    string highscoredirectory =
#if defined(__amigaos4__)
        "PROGDIR:highscores/";
#elif defined(WIN32)
        "./highscores/";
#else
        // fixme: use global /var/games instead
        (getenv("HOME") ? (string(getenv("HOME")) + "/.dangerdeep/") : "./highscores/");
#endif

    string configdirectory =
#if defined(__amigaos4__)
        "PROGDIR:config/";
#elif defined(WIN32)
        "./config/";
#else
        (getenv("HOME") ? (string(getenv("HOME")) + "/.dangerdeep/") : "./config/");
#endif
    // command line argument parsing
    unsigned res_x = 0, res_y = 0;
#ifdef __amigaos4__
    bool fullscreen = false;
#else
    bool fullscreen = true;
#endif
    string cmdmissionfilename;
    bool runeditor = false;
    unsigned maxfps = 60;
    bool override_lang = false;
    bool use_sound = true;
    date editor_start_date(1939, 9, 1);
    // parse commandline
    for (list<string>::iterator it = args.begin(); it != args.end(); ++it) {
        if (*it == "--help") {
            cout << "*** Danger from the Deep ***\nusage:\n--help\t\tshow this\n"
                 << "--language \tuse the listed language CODEs from the common.cvs file. \"en\" is the default language\n"
                 << "--res X*Y\tuse resolution X horizontal, Y vertical.\n\t\tDefault is 1024*768. If no Y value is given, Y=3/4*X is assumed.\n"
                 << "--nofullscreen\tdon't use fullscreen\n"
                 << "--debug\t\tdebug mode: no fullscreen, resolution 800\n"
                 << "--editor\trun mission editor directly\n"
                 << "--editordate yyyy/mm/dd\tset start date for editor\n"
                 << "--mission fn\trun mission from file fn (just the filename in the mission directory)\n"
                 << "--nosound\tdon't use sound\n"
                 << "--datadir path\tset base directory of data, must point to a directory with subdirs images/ textures/ objects/ and so on. Default on Unix e.g. /usr/local/share/dangerdeep.\n"
                 << "--savegamedir path\tdirectory for savegames, default path depends on platform\n"
                 << "--highscoredir path\tdirectory for highscores, default path depends on platform\n"
                 << "--configdir path\tdirectory for configuration data, default path depends on platform\n"
#if !(defined(WIN32) || (defined(__APPLE__) && defined(__MACH__)) || defined(__amigaos4__))
                 << "--vsync\tsync to vertical retrace signal (for nvidia cards)\n"
#endif
                 << "--maxfps x\tset maximum fps to x frames per second (default 60). Use x=0 to disable fps limit.\n"
                 << "--consolelog\tcopy log output to current console\n";
            return 0;
        } else if (*it == "--nofullscreen") {
            fullscreen = false;
        } else if (*it == "--debug") {
            fullscreen = false;
            res_x = 800;
            res_y = 600;
        } else if (*it == "--mission") {
            list<string>::iterator it2 = it;
            ++it2;
            if (it2 != args.end()) {
                cmdmissionfilename = *it2;
                ++it;
            }
        } else if (*it == "--editor") {
            runeditor = true;
        } else if (*it == "--editordate") {
            list<string>::iterator it2 = it;
            ++it2;
            if (it2 != args.end()) {
                editor_start_date = date(*it2);
                ++it;
            }
        } else if (*it == "--consolelog") {
            log::copy_output_to_console = true;
        } else if (*it == "--nosound") {
            use_sound = false;
        } else if (*it == "--res") {
            list<string>::iterator it2 = it;
            ++it2;
            if (it2 != args.end()) {
                string::size_type st = it2->find("*");
                if (st == string::npos) {
                    // no "*" found, use y=3/4*x
                    res_x = atoi(it2->c_str());
                    res_y = 3 * res_x / 4;
                } else {
                    res_x = atoi(it2->substr(0, st).c_str());
                    res_y = atoi(it2->substr(st + 1).c_str());
                }
                ++it;
            }
        } else if (*it == "--datadir") {
            list<string>::iterator it2 = it;
            ++it2;
            if (it2 != args.end()) {
                string datadir = *it2;
                // check if it is a directory.
                if (!is_directory(datadir)) {
                    cout << "ERROR: data directory is no directory!\n";
                    return -1;
                }
                // append separator if needed
                if (datadir[datadir.length() - 1] != '/') {
                    datadir += "/";
                }
                // check if there are valid files in data directory.
                bool datadirseemsok = true;
                if (!is_directory(datadir + "fonts")) {
                    datadirseemsok = false;
                } else if (!is_directory(datadir + "images")) {
                    datadirseemsok = false;
                } else if (!is_directory(datadir + "missions")) {
                    datadirseemsok = false;
                } else if (!is_directory(datadir + "objects")) {
                    datadirseemsok = false;
                } else if (!is_directory(datadir + "shaders")) {
                    datadirseemsok = false;
                } else if (!is_directory(datadir + "sounds")) {
                    datadirseemsok = false;
                } else if (!is_directory(datadir + "texts")) {
                    datadirseemsok = false;
                } else if (!is_directory(datadir + "textures")) {
                    datadirseemsok = false;
                }
                if (!datadirseemsok) {
                    cout << "ERROR: data directory is missing crucial files!\n";
                    return -1;
                }
                set_data_dir(datadir);
                cout << "data directory set to \"" << datadir << "\"\n";
                ++it;
            }
        } else if (*it == "--savegamedir") {
            list<string>::iterator it2 = it;
            ++it2;
            if (it2 != args.end()) {
                if (!set_dir(*it2, savegamedirectory)) {
                    cout << "ERROR: savegame directory is no directory!\n";
                    return -1;
                }
                ++it;
            }
        } else if (*it == "--highscoredir") {
            list<string>::iterator it2 = it;
            ++it2;
            if (it2 != args.end()) {
                if (!set_dir(*it2, highscoredirectory)) {
                    cout << "ERROR: highscore directory is no directory!\n";
                    return -1;
                }
                ++it;
            }
        } else if (*it == "--configdir") {
            list<string>::iterator it2 = it;
            ++it2;
            if (it2 != args.end()) {
                if (!set_dir(*it2, configdirectory)) {
                    cout << "ERROR: config directory is no directory!\n";
                    return -1;
                }
                ++it;
            }
        } else if (*it == "--language") { // included 2006/11/14 by doc1972
            list<string>::iterator it2 = it;
            ++it2;
            if (it2 != args.end()) {
                texts::set_language(*it2);
                override_lang = true;
                ++it;
            }
#if !(defined(WIN32) || (defined(__APPLE__) && defined(__MACH__)) || defined(__amigaos4__))
        } else if (*it == "--vsync") {
            if (putenv((char *)"__GL_SYNC_TO_VBLANK=1") < 0)
                cout << "ERROR: vsync setting failed.\n";
            maxfps = 0;
#endif
        } else if (*it == "--maxfps") {
            list<string>::iterator it2 = it;
            ++it2;
            if (it2 != args.end()) {
                int mf = atoi(it2->c_str());
                if (mf >= 0)
                    maxfps = unsigned(mf);
                ++it;
            }
        } else {
            cout << "unknown parameter " << *it << ".\n";
        }
    }

    // parse configuration
    cfg &mycfg = cfg::instance();
    mycfg.register_option("screen_res_x", 1024);
    mycfg.register_option("screen_res_y", 768);
#ifdef __amigaos4__
    mycfg.register_option("fullscreen", false);
#else
    mycfg.register_option("fullscreen", true);
#endif
    mycfg.register_option("debug", false);
    mycfg.register_option("sound", true);

    mycfg.register_option("sfx_quality", 0);
    mycfg.register_option("postprocessing", 0);

    mycfg.register_option("use_hqsfx", true); // TODO remove
    mycfg.register_option("use_ani_filtering", false);
    mycfg.register_option("anisotropic_level", 1.0f);
    mycfg.register_option("use_compressed_textures", false);
    mycfg.register_option("multisampling_level", 0);
    mycfg.register_option("use_multisampling", false);
    mycfg.register_option("bloom_enabled", false); // TODO: remove
    mycfg.register_option("hdr_enabled", false);   // TODO: remove
    mycfg.register_option("hint_multisampling", 0);
    mycfg.register_option("hint_fog", 0);
    mycfg.register_option("hint_mipmap", 0);
    mycfg.register_option("hint_texture_compression", 0);
    mycfg.register_option("vsync", false);
    mycfg.register_option("show_fps", false);
    mycfg.register_option("water_detail", 128);
    mycfg.register_option("wave_fft_res", 128);
    mycfg.register_option("wave_phases", 256);
    mycfg.register_option("wavetile_length", 256.0f);
    mycfg.register_option("wave_tidecycle_time", 10.24f);
    mycfg.register_option("usex86sse", true);
    mycfg.register_option("language", 0);
    mycfg.register_option("cpucores", 1);
    mycfg.register_option("terrain_texture_resolution", 0.1f);
    mycfg.register_option("terrain_detail", 1);

    mycfg.register_key(key_names[KEY_ZOOM_MAP].name, SDLK_PLUS, 0, 0, 0);
    mycfg.register_key(key_names[KEY_UNZOOM_MAP].name, SDLK_MINUS, 0, 0, 0);
    mycfg.register_key(key_names[KEY_SHOW_GAUGES_SCREEN].name, SDLK_F1, 0, 0, 0);
    mycfg.register_key(key_names[KEY_SHOW_PERISCOPE_SCREEN].name, SDLK_F2, 0, 0, 0);
    mycfg.register_key(key_names[KEY_SHOW_UZO_SCREEN].name, SDLK_F3, 0, 0, 0);
    mycfg.register_key(key_names[KEY_SHOW_BRIDGE_SCREEN].name, SDLK_F4, 0, 0, 0);
    mycfg.register_key(key_names[KEY_SHOW_MAP_SCREEN].name, SDLK_F5, 0, 0, 0);
    mycfg.register_key(key_names[KEY_SHOW_TORPEDO_SCREEN].name, SDLK_F6, 0, 0, 0);
    mycfg.register_key(key_names[KEY_SHOW_DAMAGE_CONTROL_SCREEN].name, SDLK_F7, 0, 0, 0);
    mycfg.register_key(key_names[KEY_SHOW_LOGBOOK_SCREEN].name, SDLK_F8, 0, 0, 0);
    mycfg.register_key(key_names[KEY_SHOW_SUCCESS_RECORDS_SCREEN].name, SDLK_F9, 0, 0, 0);
    mycfg.register_key(key_names[KEY_SHOW_FREEVIEW_SCREEN].name, SDLK_F10, 0, 0, 0);
    mycfg.register_key(key_names[KEY_RUDDER_LEFT].name, SDLK_LEFT, 0, 0, 0);
    mycfg.register_key(key_names[KEY_RUDDER_HARD_LEFT].name, SDLK_LEFT, 0, 0, 1);
    mycfg.register_key(key_names[KEY_RUDDER_RIGHT].name, SDLK_RIGHT, 0, 0, 0);
    mycfg.register_key(key_names[KEY_RUDDER_HARD_RIGHT].name, SDLK_RIGHT, 0, 0, 1);
    mycfg.register_key(key_names[KEY_RUDDER_UP].name, SDLK_UP, 0, 0, 0);
    mycfg.register_key(key_names[KEY_RUDDER_HARD_UP].name, SDLK_UP, 0, 0, 1);
    mycfg.register_key(key_names[KEY_RUDDER_DOWN].name, SDLK_DOWN, 0, 0, 0);
    mycfg.register_key(key_names[KEY_RUDDER_HARD_DOWN].name, SDLK_DOWN, 0, 0, 1);
    mycfg.register_key(key_names[KEY_CENTER_RUDDERS].name, SDLK_RETURN, 0, 0, 0);
    mycfg.register_key(key_names[KEY_THROTTLE_LISTEN].name, SDLK_1, 0, 0, 0);
    mycfg.register_key(key_names[KEY_THROTTLE_SLOW].name, SDLK_2, 0, 0, 0);
    mycfg.register_key(key_names[KEY_THROTTLE_HALF].name, SDLK_3, 0, 0, 0);
    mycfg.register_key(key_names[KEY_THROTTLE_FULL].name, SDLK_4, 0, 0, 0);
    mycfg.register_key(key_names[KEY_THROTTLE_FLANK].name, SDLK_5, 0, 0, 0);
    mycfg.register_key(key_names[KEY_THROTTLE_STOP].name, SDLK_6, 0, 0, 0);
    mycfg.register_key(key_names[KEY_THROTTLE_REVERSE].name, SDLK_7, 0, 0, 0);
    mycfg.register_key(key_names[KEY_THROTTLE_REVERSEHALF].name, SDLK_8, 0, 0, 0);
    mycfg.register_key(key_names[KEY_THROTTLE_REVERSEFULL].name, SDLK_9, 0, 0, 0);
    mycfg.register_key(key_names[KEY_FIRE_TUBE_1].name, SDLK_1, 0, 0, 1);
    mycfg.register_key(key_names[KEY_FIRE_TUBE_2].name, SDLK_2, 0, 0, 1);
    mycfg.register_key(key_names[KEY_FIRE_TUBE_3].name, SDLK_3, 0, 0, 1);
    mycfg.register_key(key_names[KEY_FIRE_TUBE_4].name, SDLK_4, 0, 0, 1);
    mycfg.register_key(key_names[KEY_FIRE_TUBE_5].name, SDLK_5, 0, 0, 1);
    mycfg.register_key(key_names[KEY_FIRE_TUBE_6].name, SDLK_6, 0, 0, 1);
    mycfg.register_key(key_names[KEY_SELECT_TARGET].name, SDLK_SPACE, 0, 0, 0);
    mycfg.register_key(key_names[KEY_SCOPE_UP_DOWN].name, SDLK_0, 0, 0, 0);
    mycfg.register_key(key_names[KEY_CRASH_DIVE].name, SDLK_c, 0, 0, 0);
    mycfg.register_key(key_names[KEY_GO_TO_SNORKEL_DEPTH].name, SDLK_d, 0, 0, 0);
    mycfg.register_key(key_names[KEY_TOGGLE_SNORKEL].name, SDLK_f, 0, 0, 0);
    mycfg.register_key(key_names[KEY_SET_HEADING_TO_VIEW].name, SDLK_h, 0, 0, 0);
    mycfg.register_key(key_names[KEY_IDENTIFY_TARGET].name, SDLK_i, 0, 0, 0);
    mycfg.register_key(key_names[KEY_GO_TO_PERISCOPE_DEPTH].name, SDLK_p, 0, 0, 0);
    mycfg.register_key(key_names[KEY_GO_TO_SURFACE].name, SDLK_s, 0, 0, 0);
    mycfg.register_key(key_names[KEY_FIRE_TORPEDO].name, SDLK_t, 0, 0, 0);
    mycfg.register_key(key_names[KEY_SET_VIEW_TO_HEADING].name, SDLK_v, 0, 0, 0);
    mycfg.register_key(key_names[KEY_TOGGLE_ZOOM_OF_VIEW].name, SDLK_y, 0, 0, 0);
    mycfg.register_key(key_names[KEY_TURN_VIEW_LEFT].name, SDLK_COMMA, 0, 0, 0);
    mycfg.register_key(key_names[KEY_TURN_VIEW_LEFT_FAST].name, SDLK_COMMA, 0, 0, 1);
    mycfg.register_key(key_names[KEY_TURN_VIEW_RIGHT].name, SDLK_PERIOD, 0, 0, 0);
    mycfg.register_key(key_names[KEY_TURN_VIEW_RIGHT_FAST].name, SDLK_PERIOD, 0, 0, 1);
    mycfg.register_key(key_names[KEY_TIME_SCALE_UP].name, SDLK_KP_PLUS, 0, 0, 0);
    mycfg.register_key(key_names[KEY_TIME_SCALE_DOWN].name, SDLK_KP_MINUS, 0, 0, 0);
    mycfg.register_key(key_names[KEY_FIRE_DECK_GUN].name, SDLK_g, 0, 0, 0);
    mycfg.register_key(key_names[KEY_TOGGLE_RELATIVE_BEARING].name, SDLK_r, 0, 0, 0);
    mycfg.register_key(key_names[KEY_TOGGLE_MAN_DECK_GUN].name, SDLK_g, 0, 0, 1);
    mycfg.register_key(key_names[KEY_SHOW_TDC_SCREEN].name, SDLK_F11, 0, 0, 0);
    mycfg.register_key(key_names[KEY_TOGGLE_POPUP].name, SDLK_TAB, 0, 0, 0);
    mycfg.register_key(key_names[KEY_SHOW_TORPSETUP_SCREEN].name, SDLK_F12, 0, 0, 0);
    mycfg.register_key(key_names[KEY_SHOW_TORPEDO_CAMERA].name, SDLK_k, 0, 0, 0);
    mycfg.register_key(key_names[KEY_TAKE_SCREENSHOT].name, SDLK_PRINTSCREEN, 0, 0, 0);

    // mycfg.register_option("invert_mouse", false);
    // mycfg.register_option("ocean_res_x", 128);
    // mycfg.register_option("ocean_res_y", 128);
    // mycfg.register_option("", );

    // randomize (generador global usado por rnd() en global_data.h)
    seed_global_rnd(static_cast<unsigned>(time(nullptr)));

    // Check if crucial game assets exist in the data directory
    string check_datadir = get_data_dir();
    if (check_datadir.empty() || check_datadir[check_datadir.length() - 1] != '/') {
        check_datadir += "/";
    }
    bool datadir_valid = is_directory(check_datadir) &&
                         is_directory(check_datadir + "objects") &&
                         is_directory(check_datadir + "fonts") &&
                         is_directory(check_datadir + "textures");
    if (!datadir_valid) {
        dftd_startup_log("ERROR: Crucial game data missing in '%s'!", check_datadir.c_str());
        dftd_startup_log("Please ensure the full data package (DangerFromTheDeep_OS4.lha) is installed into the game directory.");
        string err_msg = "Crucial game data missing in:\n" + check_datadir +
                         "\n\nPlease ensure the full data package\n(DangerFromTheDeep_OS4.lha) is installed into the game directory.";
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Danger from the Deep Error", err_msg.c_str(), NULL);
        return -1;
    }

    // read data files
    dftd_startup_log("Scanning and reading data files from %s...", get_data_dir().c_str());
    data_file();
    dftd_startup_log("Data files scanned successfully.");

    // make sure the default values are stored if there is no config file,
    // and make sure all registered values are stored in it
    dftd_startup_log("Loading configuration from %s...", configdirectory.c_str());
    if (is_file(configdirectory + "config")) {
        mycfg.load(configdirectory + "config");
    } else {
        if (!is_directory(configdirectory)) {
            make_dir(configdirectory);
        }
        mycfg.save(configdirectory + "config");
    }

    //	mycfg.save("./testconf");

    glsl_shader::enable_hqsfx = mycfg.getb("use_hqsfx");

    // read screen resolution from config file if no override has been set by command line parameters
    if (res_x == 0) {
        res_x = mycfg.geti("screen_res_x");
        res_y = mycfg.geti("screen_res_y");
    }

    // Read language from options-file
    if (!override_lang)
        texts::set_language(mycfg.geti("language"));

    // fixme: also allow 1280x1024, set up gl viewport for 4:3 display
    // with black borders at top/bottom (height 2*32pixels)
    // weather conditions and earth curvature allow 30km sight at maximum.
    system::parameters params;
    params.near_z = 1.0;
    params.far_z = 30000.0 + 500.0;
    params.resolution_x = res_x;
    params.resolution_y = res_y;
    params.window_caption = texts::get(7);
    params.fullscreen = fullscreen;
    params.use_multisampling = mycfg.getb("use_multisampling");
    params.hint_multisampling = mycfg.geti("hint_multisampling");
    params.multisample_level = mycfg.geti("multisampling_level");
    params.hint_fog = mycfg.geti("hint_fog");
    params.hint_mipmap = mycfg.geti("hint_mipmap");
    params.hint_texture_compression = mycfg.geti("hint_texture_compression");
    params.vertical_sync = mycfg.getb("vsync");

    texture::use_compressed_textures = mycfg.getb("use_compressed_textures");
    texture::use_anisotropic_filtering = mycfg.getb("use_ani_filtering");
    texture::anisotropic_level = mycfg.getf("anisotropic_level");
    dftd_startup_log("Initializing display system (%ux%u, fullscreen=%d)...", res_x, res_y, fullscreen ? 1 : 0);
    system::create_instance(new class system(params));
    dftd_startup_log("Display system initialized successfully.");
    sys().set_screenshot_directory(savegamedirectory);
    sys().set_res_2d(1024, 768);
    sys().set_max_fps(maxfps);
    font_arial = &sys().register_font(get_font_dir(), "font_arial");
    bool is_de = (texts::get_language_code() == "de");
    reset_loading_screen(is_de ? "Initialisiere Spiel..." : "Initializing game...", 8);
    set_loading_progress(0.05f, is_de ? "Lade Schriftarten..." : "Loading fonts...");
    font_jphsl = &sys().register_font(get_font_dir(), "font_jphsl");
    font_vtremington10 = &sys().register_font(get_font_dir(), "font_vtremington10");
    font_vtremington12 = &sys().register_font(get_font_dir(), "font_vtremington12");
    font_typenr16 = &sys().register_font(get_font_dir(), "font_typenr16");
    widget::set_image_cache(&(imagecache()));
    set_loading_progress(0.20f, is_de ? "Prüfe 3D-Grafikfunktionen..." : "Checking 3D graphics...");

    // --------------------------------------------------------------------------------
    // check for shader/glsl support

#if !defined(WIN32)

    tests gltest = tests();
    int problems = gltest.do_gl_tests();
    std::string warnings;

    // check for fatal errors
    if (0 == problems) {
        // non fatal errors
        if (!gltest.warn_log.empty()) {
            ostringstream str_problems;
            str_problems << "Warnings (missing functionality):\n"; // TODO: use texts::get ?

            for (set<string>::const_iterator it = gltest.warn_log.begin(); it != gltest.warn_log.end(); it++)
                str_problems << "  " << it->c_str() << "\n";

            warnings = str_problems.str();
        }

        // fatal errors detected...
        if (!gltest.error_log.empty()) {
            ostringstream str_problems;

            str_problems << "Dangerdeep cannot run on this machine because the following tests failed:\n\n";

            for (set<string>::const_iterator it = gltest.error_log.begin(); it != gltest.error_log.end(); it++)
                str_problems << "  " << it->c_str() << "\n";

            str_problems << "\nPress any key to quit.";
            str_problems << "\n\n"
                         << warnings;

#ifdef __amigaos4__
#if !defined(DFTD_NO_DEBUG_LOG)
            std::cout << str_problems.str() << std::endl;
#endif
#else
            glClearColor(0, 0, 1, 0);
            glClear(GL_COLOR_BUFFER_BIT);
            sys().prepare_2d_drawing();
            font_arial->print(0, 0, str_problems.str());
            sys().unprepare_2d_drawing();
            sys().swap_buffers();
            bool quit = false;
            while (!quit) {
                list<game_event> events = sys().poll_event_queue();
                for (list<game_event>::iterator it = events.begin(); it != events.end(); ++it) {
                    if (it->type == event_type::KEY_DOWN) {
                        quit = true;
                    } else if (it->type == event_type::MOUSE_BUTTON_UP) {
                        quit = true;
                    }
                }
            }
#ifdef __amigaos4__
            exit(-1);
#else
            throw system::quit_exception(-1);
#endif
#endif
        }
    }

#endif // WIN32
       // --------------------------------------------------------------------------------

    log_info("Danger from the Deep");
    log_info("Copyright (C) 2003-2011  Thorsten Jordan, Luis Barrancos and others.");
    log_info("Version " << get_program_version());

    set_loading_progress(0.40f, is_de ? "Initialisiere Soundsystem..." : "Initializing sound system...");

    GLfloat lambient[4] = {0.1, 0.1, 0.1, 1};
    GLfloat ldiffuse[4] = {1, 1, 1, 1};
    GLfloat lposition[4] = {0, 0, 1, 0};
    glLightfv(GL_LIGHT0, GL_AMBIENT, lambient);
    glLightfv(GL_LIGHT0, GL_DIFFUSE, ldiffuse);
    glLightfv(GL_LIGHT0, GL_POSITION, lposition);
    glEnable(GL_LIGHT0);

    // create and start thread for music handling.
    music::create_instance(new music(use_sound));

#ifdef USE_SDL3
    // SDL3_mixer: MIX_CreateMixerDevice must be called from main thread
    audio_backend* be = get_audio_backend();
    if (be->init_audio_subsystem()) {
        const int audio_channels = 2;
        const int audio_buffers = 4096;
        be->open_audio(44100, 0, audio_channels, audio_buffers);
    }
#endif

    music::instance().start();
    set_loading_progress(0.55f, is_de ? "Lade Musikdaten..." : "Loading soundtrack...");
    music::instance().append_track("ImInTheMood.ogg");
    music::instance().append_track("Betty_Roche-Trouble_Trouble.ogg");
    music::instance().append_track("theme.ogg");
    music::instance().append_track("Auf_Feindfahrt_fast.ogg");
    music::instance().append_track("outside_underwater.ogg");
    music::instance().append_track("Auf_Feindfahrt_environmental.ogg");
    music::instance().append_track("loopable_seasurface.ogg");
    music::instance().append_track("loopable_seasurface_badweather.ogg");
    music::instance().append_track("Auf_Feindfahrt.ogg");
    set_loading_progress(0.70f, is_de ? "Lade Benutzeroberfläche..." : "Loading user interface...");
    // music::instance().set_playback_mode(music::PBM_SHUFFLE_TRACK);
    music::instance().play();

    widget::set_theme(std::make_unique<widget::theme>("widgetelements_menu.png", "widgeticons_menu.png",
                                                      font_typenr16,
                                                      color(182, 146, 137),
                                                      color(240, 217, 127) /*color(222, 208, 195)*/,
                                                      color(92, 72, 68)));

    auto metalbackground = std::make_unique<texture>(get_image_dir() + "metalbackground.jpg");
    sys().draw_console_with(font_arial, metalbackground.get());

    set_loading_progress(0.85f, is_de ? "Lade Spielstände & Bestenliste..." : "Loading highscores...");

    // try to make directories if they do not exist
    try {
        directory savegamedir(savegamedirectory);
    } catch (exception &e) {
        if (!make_dir(savegamedirectory))
            throw error("could not create save game directory.");
    }

    try {
        directory configdir(configdirectory);
    } catch (exception &e) {
        if (!make_dir(configdirectory))
            throw error("could not create config directory.");
    }

    try {
        directory highscoredir(highscoredirectory);
    } catch (exception &e) {
        if (!make_dir(highscoredirectory))
            throw error("could not create save game directory.");
    }

    // read highscores
    if (!file_exists(highscoredirectory + HSL_MISSION_NAME))
        highscorelist().save(highscoredirectory + HSL_MISSION_NAME);
    if (!file_exists(highscoredirectory + HSL_CAREER_NAME))
        highscorelist().save(highscoredirectory + HSL_CAREER_NAME);
    hsl_mission = highscorelist(highscoredirectory + HSL_MISSION_NAME);
    hsl_career = highscorelist(highscoredirectory + HSL_CAREER_NAME);

    set_loading_progress(1.00f, is_de ? "Bereit!" : "Ready!");

    // check if there was a mission given at the command line, or editor more etc.
    if (runeditor) {
        // reset loading screen here to show user we are doing something
        reset_loading_screen();
        run_game_editor(std::make_unique<game_editor>(cfg::instance(), log::instance(), editor_start_date));
    } else if (cmdmissionfilename.length() > 0) {
        // fixme: check here that the file exists or tinyxml faults with a embarassing error message
        std::unique_ptr<game> gm;
        bool ok = true;
        try {
            gm = std::make_unique<game>(cfg::instance(), log::instance(), get_mission_dir() + cmdmissionfilename);
        } catch (const std::exception &e) {
            dftd_startup_log("ERROR loading mission: %s", e.what());
            log_warning("error loading mission: " << e.what());
            // fixme: show dialogue!
            ok = false;
        } catch (...) {
            dftd_startup_log("ERROR loading mission: unknown exception!");
            ok = false;
        }
        if (ok) {
            // reset loading screen here to show user we are doing something
            reset_loading_screen();
            run_game(std::move(gm));
        }
    } else {
        int retval = 1;
        widget w(0, 0, 1024, 768, "", 0, "titlebackgr.jpg");
        do { // loop until menu is closed.
            if (sys().is_quit_requested())
                break;
            // main menu
            w.remove_children();

#if !defined(WIN32) && !defined(__amigaos4__)
            // opengl warnings
            if (0 != warnings.length()) {
                w.add_child(new widget_text(20, 20, 0, 0, warnings));
            }
#endif // WIN32

            // display version #
            w.add_child(new widget_text(5, 768 - 30, 0, 0, get_program_version()));

            widget_menu *wm = new widget_menu(0, 0, 400, 40, texts::get(104));
            wm->set_entry_spacing(8);
            w.add_child(wm);
            wm->add_entry(texts::get(21), new widget_func_button<void (*)()>(&menu_single_mission, 0, 0, 0, 0));
            // wm->add_entry(texts::get(23), new widget_func_button<void (*)()>(&menu_notimplemented /* career menu */, 0, 0, 0, 0));
            wm->add_entry(texts::get(222), new widget_func_button<void (*)()>(&menu_mission_editor, 0, 0, 0, 0));
            wm->add_entry(texts::get(24), new widget_func_button<void (*)()>(&menu_show_vessels, 0, 0, 0, 0));
            wm->add_entry(texts::get(25), new widget_func_button<void (*)()>(&show_halloffame_mission, 0, 0, 0, 0));
            wm->add_entry(texts::get(213), new widget_func_button<void (*)()>(&show_credits, 0, 0, 0, 0));
            wm->add_entry(texts::get(26), new widget_caller_arg_button<widget, void (widget::*)(int), int>(&w, &widget::close, 1, 0, 0, 0, 0));
            wm->add_entry(texts::get(29), new widget_func_button<void (*)()>(&menu_options, 0, 0, 0, 0));

            wm->add_entry(texts::get(30), new widget_caller_arg_button<widget, void (widget::*)(int), int>(&w, &widget::close, 0, 0, 0, 0, 0));
            wm->align(0, 0);
            retval = w.run(0, false);
            if (sys().is_quit_requested())
                break;
            if (retval == 1)
                menu_select_language();
        } while (retval != 0);
    }

    music::instance().stop(1000);

    hsl_mission.save(highscoredirectory + HSL_MISSION_NAME);
    hsl_career.save(highscoredirectory + HSL_CAREER_NAME);
    mycfg.save(configdirectory + "config");

    sys().draw_console_with(font_arial, nullptr);
    metalbackground.reset();
    widget::unref_all_backgrounds();

    data_file_handler::destroy_instance();
    cfg::destroy_instance();
    widget::set_theme(std::unique_ptr<widget::theme>(nullptr)); // clear allocated theme
    music::release_instance()->destruct();                      // kill thread
    global_data::destroy_instance();
    system::destroy_instance();

    return 0;
}
