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

// global data
// subsim (C)+(W) Thorsten Jordan. SEE LICENSE

#include "global_data.h"
#include "datadirs.h"
#include "font.h"
#include "global_constants.h"
#include "image.h"
#include "log.h"
#include "model.h"
#include "primitives.h"
#include "system.h"
#include "system_defines.h"
#include "texts.h"
#include "texture.h"
#include <algorithm>
#include <iomanip>
#include <list>
#include <sstream>
#include <stdexcept>

using namespace std;

// same with version string
string get_program_version() {
    return string(VERSION);
}

global_data::global_data()
    : modelcache(get_data_dir()),
      imagecache(get_image_dir()),
      texturecache(get_texture_dir())
// soundcache(get_sound_dir())
{
}

// display loading progress
list<string> loading_screen_messages;
unsigned starttime;
static std::unique_ptr<objcachet<image>::reference> loading_background_ref;
static float current_loading_progress = 0.0f;
static std::string current_loading_status = "";
static int loading_step_count = 0;
static int loading_expected_steps = 10;

global_data::~global_data() {
    loading_background_ref.reset();
    font_arial = nullptr;
    font_jphsl = nullptr;
    font_vtremington10 = nullptr;
    font_vtremington12 = nullptr;
    font_typenr16 = nullptr;
}

font *font_arial = 0, *font_jphsl = 0, *font_vtremington10 = 0, *font_vtremington12 = 0,
     *font_typenr16 = 0;

// Helper to draw filled rectangles in 2D OpenGL mode
static void draw_loading_rect(float x, float y, float w, float h, const colorf &col) {
    if (w <= 0.0f || h <= 0.0f)
        return;
    glDisable(GL_CULL_FACE);
    primitives::quad(vector2f(x, y + h), vector2f(x + w, y), col).render();
}

void display_loading_screen() {
    glClearColor(0, 0, 0, 0);
    glClear(GL_COLOR_BUFFER_BIT);
    sys().prepare_2d_drawing();

    // Ensure blending is enabled for transparent layers
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    // display a nice loading image in the background (keep cached to avoid reloading from disk each step)
    if (!loading_background_ref) {
        loading_background_ref = std::make_unique<objcachet<image>::reference>(imagecache(), "entryscreen.png");
    }
    if (loading_background_ref && loading_background_ref->get()) {
        (*loading_background_ref)->draw(0, 0);
    }

#if !defined(DFTD_NO_DEBUG_LOG)
    // In debug builds: also display detailed diagnostic step timings in the upper area
    if (font_arial) {
        unsigned fh = font_arial->get_height();
        unsigned y = 10;
        for (list<string>::const_iterator it = loading_screen_messages.begin();
             it != loading_screen_messages.end(); ++it) {
            font_arial->print(15, y, *it, color(200, 200, 200), true);
            y += fh;
            if (y > 580)
                break;
        }
    }
#endif

    // Render progress bar and percentage
    if (font_arial) {
        const float bar_w = 640.0f;
        const float bar_h = 24.0f;
        const float bar_x = (1024.0f - bar_w) / 2.0f; // 192.0f
        const float bar_y = 680.0f;

        // 1. Outer drop shadow
        draw_loading_rect(bar_x - 4, bar_y - 4, bar_w + 8, bar_h + 8, colorf(0.0f, 0.0f, 0.0f, 0.70f));

        // 2. Brass / metallic outer bezel frame
        draw_loading_rect(bar_x - 2, bar_y - 2, bar_w + 4, bar_h + 4, colorf(0.48f, 0.42f, 0.32f, 0.95f));

        // 3. Frame 3D bevels (light at top/left, dark at bottom/right)
        draw_loading_rect(bar_x - 2, bar_y - 2, bar_w + 4, 1.5f, colorf(0.78f, 0.72f, 0.55f, 0.95f));
        draw_loading_rect(bar_x - 2, bar_y - 2, 1.5f, bar_h + 4, colorf(0.78f, 0.72f, 0.55f, 0.95f));
        draw_loading_rect(bar_x - 2, bar_y + bar_h + 0.5f, bar_w + 4, 1.5f, colorf(0.20f, 0.16f, 0.10f, 0.95f));
        draw_loading_rect(bar_x + bar_w + 0.5f, bar_y - 2, 1.5f, bar_h + 4, colorf(0.20f, 0.16f, 0.10f, 0.95f));

        // 4. Dark recessed track interior
        draw_loading_rect(bar_x, bar_y, bar_w, bar_h, colorf(0.06f, 0.08f, 0.10f, 0.92f));

        // 5. Progress bar fill
        float pct = std::clamp(current_loading_progress, 0.0f, 1.0f);
        float fill_w = bar_w * pct;
        if (fill_w > 1.0f) {
            // Upper half of fill (warm gold highlight)
            draw_loading_rect(bar_x, bar_y, fill_w, bar_h * 0.5f, colorf(0.88f, 0.74f, 0.35f, 0.95f));

            // Lower half of fill (shaded brass for 3D depth)
            draw_loading_rect(bar_x, bar_y + bar_h * 0.5f, fill_w, bar_h * 0.5f, colorf(0.70f, 0.54f, 0.20f, 0.95f));

            // Top glint line
            draw_loading_rect(bar_x, bar_y, fill_w, 1.5f, colorf(1.0f, 0.96f, 0.75f, 0.90f));

            // Dynamic activity shimmer wave sweeping across the filled bar:
            // This visually confirms calculation is actively happening even between progress jumps!
            float cycle_pos = fmodf(static_cast<float>(sys().millisec()) / 1400.0f, 1.0f);
            float shimmer_x = bar_x + fill_w * cycle_pos;
            float shimmer_w = std::min(60.0f, fill_w * 0.40f);
            float s_left = std::max(bar_x, shimmer_x - shimmer_w * 0.5f);
            float s_right = std::min(bar_x + fill_w, shimmer_x + shimmer_w * 0.5f);
            if (s_right > s_left) {
                draw_loading_rect(s_left, bar_y, s_right - s_left, bar_h, colorf(1.0f, 1.0f, 0.95f, 0.35f));
                float c_left = std::max(bar_x, shimmer_x - shimmer_w * 0.2f);
                float c_right = std::min(bar_x + fill_w, shimmer_x + shimmer_w * 0.2f);
                if (c_right > c_left) {
                    draw_loading_rect(c_left, bar_y, c_right - c_left, bar_h, colorf(1.0f, 1.0f, 1.0f, 0.28f));
                }
            }
        }

        // Restore face culling state
        glCullFace(GL_FRONT);
        glEnable(GL_CULL_FACE);

        // 6. Current status text centered above the progress bar
        if (!current_loading_status.empty()) {
            font_arial->print_hc(512, static_cast<int>(bar_y - 26.0f),
                                 current_loading_status,
                                 color(240, 220, 140), true);
        }

        // 7. Percentage text centered inside the progress bar
        int percent_val = static_cast<int>(pct * 100.0f + 0.5f);
        ostringstream oss;
        oss << percent_val << " %";
        font_arial->print_c(512, static_cast<int>(bar_y + bar_h * 0.5f),
                            oss.str(),
                            color(255, 255, 255), true);

        // 8. Animated calculation activity indicator below the progress bar
        bool is_de = (texts::get_language_code() == "de");
        unsigned anim_step = (sys().millisec() / 300) % 4;
        string dots(anim_step + 1, '.');
        ostringstream calc_oss;
        calc_oss << (is_de ? "Berechnung läuft" : "Calculating") << dots;
        float pulse = 0.65f + 0.35f * sinf(static_cast<float>(sys().millisec()) * 0.005f);
        font_arial->print_hc(512, static_cast<int>(bar_y + bar_h + 10.0f),
                             calc_oss.str(),
                             color(static_cast<Uint8>(180.0f * pulse),
                                   static_cast<Uint8>(205.0f * pulse),
                                   static_cast<Uint8>(230.0f * pulse)), true);
    }

    sys().unprepare_2d_drawing();
    sys().swap_buffers();
}

void reset_loading_screen(const string &initial_msg, int expected_steps) {
    loading_screen_messages.clear();
    current_loading_progress = 0.0f;
    loading_step_count = 0;
    loading_expected_steps = (expected_steps > 0) ? expected_steps : 10;

    if (!initial_msg.empty()) {
        current_loading_status = initial_msg;
    } else {
        bool is_de = (texts::get_language_code() == "de");
        current_loading_status = is_de ? "Lade..." : "Loading...";
    }

    loading_screen_messages.push_back(current_loading_status);
    log_info(current_loading_status);
    display_loading_screen();
    starttime = sys().millisec();
}

void add_loading_screen(const string &msg, float progress) {
    unsigned tm = sys().millisec();
    unsigned deltatime = tm - starttime;
    starttime = tm;
    ostringstream oss;
    oss << msg << " (" << deltatime << "ms)";
    loading_screen_messages.push_back(oss.str());
    log_info(oss.str());

    current_loading_status = msg;
    loading_step_count++;

    if (progress >= 0.0f) {
        current_loading_progress = std::clamp(progress, 0.0f, 1.0f);
    } else {
        float auto_p = static_cast<float>(loading_step_count) / static_cast<float>(loading_expected_steps);
        current_loading_progress = std::clamp(std::max(current_loading_progress, auto_p), 0.0f, 0.95f);
    }
    display_loading_screen();
}

void set_loading_progress(float progress, const string &msg) {
    if (!msg.empty()) {
        current_loading_status = msg;
        log_info(msg);
    }
    current_loading_progress = std::clamp(progress, 0.0f, 1.0f);
    display_loading_screen();
}

string get_time_string(double tm) {
    unsigned seconds = unsigned(floor(myfmod(tm, 86400)));
    unsigned hours = seconds / 3600;
    unsigned minutes = (seconds % 3600) / 60;
    seconds = seconds % 60;
    ostringstream oss;
    oss << setw(2) << setfill('0') << hours << ":"
        << setw(2) << setfill('0') << minutes << ":"
        << setw(2) << setfill('0') << seconds;
    return oss.str();
}

#define CA 0.0003
void jacobi_amp(double u, double k, double &sn, double &cn) {
    double emc = 1.0 - k * k;

    double a, b, c, d = 0.0, dn;
    double em[14], en[14];
    int i, ii, l, bo;

    if (emc) {
        bo = (emc < 0.0);
        if (bo) {
            d = 1.0 - emc;
            emc /= -1.0 / d;
            u *= (d = sqrt(d));
        }
        a = 1.0;
        dn = 1.0;
        for (i = 1; i <= 13; i++) {
            l = i;
            em[i] = a;
            en[i] = (emc = sqrt(emc));
            c = 0.5 * (a + emc);
            if (fabs(a - emc) <= CA * a)
                break;
            emc *= a;
            a = c;
        }
        u *= c;
        sn = sin(u);
        cn = cos(u);
        if (sn) {
            a = (cn) / (sn);
            c *= a;
            for (ii = l; ii >= 1; ii--) {
                b = em[ii];
                a *= c;
                c *= (dn);
                dn = (en[ii] + a) / (b + a);
                a = c / b;
            }
            a = 1.0 / sqrt(c * c + 1.0);
            sn = (sn >= 0.0 ? a : -a);
            cn = c * sn;
        }
        if (bo) {
            cn = dn;
            sn /= d;
        }
    } else {
        cn = 1.0 / cosh(u);
        sn = tanh(u);
    }
}
#undef CA

vector2f transform_real_to_geo(vector2f &pos) {
    double sn, cn, r;
    vector2f coord;

    jacobi_amp(pos.y / WGS84_A, WGS84_K, sn, cn);
    r = sqrt((WGS84_B * WGS84_B) / (1.0 - WGS84_K * WGS84_K * cn * cn));
    coord.x = (180.0 * pos.x) / (M_PI * r);
    coord.y = (asin(sn) * 180.0) / M_PI;

    return coord;
}

static double transform_nautic_coord_to_real(const string &s, char minus, char plus, int degmax) {
    if (s.length() < 2)
        throw error(string("nautic coordinate invalid ") + s);
    char sign = s[s.length() - 1];
    if (sign != minus && sign != plus)
        throw error(string("nautic coordinate (direction sign) invalid ") + s);
    // find separator
    string::size_type st = s.find("/");
    if (st == string::npos)
        throw error(string("no separator in position string ") + s);
    string degrees = s.substr(0, st);
    string minutes = s.substr(st + 1, s.length() - st - 2);
    int deg = atoi(degrees.c_str());
    if (deg < 0 || deg > degmax)
        throw error(string("degrees are not in range [0...180/360] in position string ") + s);
    int mts = atoi(minutes.c_str());
    if (mts < 0 || mts > 59)
        throw error(string("minutes are not in [0...59] in position string ") + s);
    return (sign == minus ? -1 : 1) * ((DEGREE_IN_METERS * deg) + (MINUTE_IN_METERS * mts));
}

double transform_nautic_posx_to_real(const string &s) {
    return transform_nautic_coord_to_real(s, 'W', 'E', 180);
}

double transform_nautic_posy_to_real(const string &s) {
    return transform_nautic_coord_to_real(s, 'S', 'N', 90);
}

void save_pgm(const char *fn, unsigned w, unsigned h, const Uint8 *d, unsigned stride) {
    std::ofstream osg(fn);
    if (!osg.good())
        throw error(std::string("Can't open output file ") + fn);
    osg << "P5\n";
    osg << w << " " << h << "\n255\n";
    if (!stride)
        stride = w;
    for (unsigned y = 0; y < h; ++y) {
        osg.write((const char *)d, w);
        d += stride;
    }
}
