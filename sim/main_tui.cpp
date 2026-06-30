// SolarMax interactive terminal simulator (FTXUI).
// Runs the REAL control brain (lib/tracker_core) on a simulated sun + wind, so
// you can scrub time, jump to solstice/equinox, change wind and roof angle, and
// watch the panel track. Build: see sim/README.md.

#include <ftxui/component/component.hpp>
#include <ftxui/component/screen_interactive.hpp>
#include <ftxui/dom/elements.hpp>
#include <ftxui/dom/canvas.hpp>

#include "tracker_core.h"

#include <atomic>
#include <chrono>
#include <cmath>
#include <ctime>
#include <cstdio>
#include <mutex>
#include <string>
#include <thread>

using namespace ftxui;

static std::string fmt(const char* f, double a) { char b[64]; snprintf(b, sizeof(b), f, a); return b; }

int main() {
    // ── Live state (shared between the update thread and the renderer) ──────────
    std::mutex   mtx;
    TrackerCore  core;
    TrackerConfig cfg;          // lat/lon/axis/wind thresholds — editable live
    TrackerStep  last{};        // most recent decision (read by renderer)

    int    tzOffset    = -7;    // Arizona
    double speed       = 120.0; // simulated seconds per real second
    bool   playing     = true;
    float  wind        = 5.0f;
    float  panelActual = 0.0f;  // eased toward target to animate the motor

    auto mkEpoch = [](int y,int mo,int d,int h,int mi){
        std::tm t{}; t.tm_year=y-1900; t.tm_mon=mo-1; t.tm_mday=d; t.tm_hour=h; t.tm_min=mi;
        return timegm(&t);
    };
    std::atomic<time_t> simEpoch{ mkEpoch(2026,6,21,12,15) };

    auto screen = ScreenInteractive::Fullscreen();

    // ── Background clock/stepper: advances time, steps the brain, eases panel ───
    std::atomic<bool> running{true};
    std::thread worker([&]{
        auto prev = std::chrono::steady_clock::now();
        while (running) {
            std::this_thread::sleep_for(std::chrono::milliseconds(60));
            auto now = std::chrono::steady_clock::now();
            double dt = std::chrono::duration<double>(now - prev).count();
            prev = now;

            std::lock_guard<std::mutex> lk(mtx);
            if (playing) simEpoch = simEpoch.load() + (time_t)std::llround(dt * speed);

            std::tm g; time_t e = simEpoch.load(); gmtime_r(&e, &g);
            TrackerInputs in;
            in.year = g.tm_year+1900; in.month = g.tm_mon+1; in.day = g.tm_mday;
            in.hourUTC = g.tm_hour + g.tm_min/60.0 + g.tm_sec/3600.0;
            in.windMph = wind; in.timeValid = true;

            last = core.step(cfg, in);

            // Ease the physical panel toward the target (~60°/s real time) so
            // moves and preset jumps animate visibly.
            float maxStep = (float)(dt * 60.0);
            float err = last.targetAngle - panelActual;
            if (std::fabs(err) <= maxStep) panelActual = last.targetAngle;
            else panelActual += (err > 0 ? maxStep : -maxStep);

            screen.PostEvent(Event::Custom);
        }
    });

    // ── Side-view canvas: sun + panel in the E–W plane ─────────────────────────
    auto skyCanvas = [&](const TrackerStep& s){
        Canvas c(84, 64);
        int cx = 42, gy = 46; double Rpx = 26;
        c.DrawPointLine(2, gy, 82, gy, Color::GrayDark);          // ground
        c.DrawText(2,  gy+2, "W", Color::GrayLight);
        c.DrawText(78, gy+2, "E", Color::GrayLight);

        double elr = s.sun.elevation * M_PI/180, azr = s.sun.azimuth * M_PI/180;
        double dx = std::cos(elr)*std::sin(azr), dy = std::sin(elr);
        int sx = cx + (int)(dx*Rpx), sy = gy - (int)(dy*Rpx);
        if (s.sun.aboveHorizon) {
            c.DrawPointLine(cx, gy, sx, sy, Color::Yellow);       // ray to sun
            c.DrawText(sx-3, sy-2, "(*)", Color::Yellow);
        }

        double R = panelActual * M_PI/180, L = 22;                // panel surface
        int e1x = cx + (int)(std::cos(R)*L), e1y = gy - (int)(std::sin(R)*L);
        int e2x = cx - (int)(std::cos(R)*L), e2y = gy + (int)(std::sin(R)*L);
        c.DrawPointLine(e1x, e1y, e2x, e2y, Color::Green);
        int nx = cx + (int)(-std::sin(R)*18), ny = gy - (int)(std::cos(R)*18);
        c.DrawPointLine(cx, gy, nx, ny, Color::Cyan);            // panel normal
        return canvas(std::move(c));
    };

    auto stateColor = [](TrackerState st){
        switch (st) {
            case TS_TRACKING: return Color::Green;
            case TS_STOW:     return Color::Red;
            case TS_NIGHT:    return Color::Blue;
            case TS_INIT:     return Color::Yellow;
            default:          return Color::RedLight;
        }
    };

    auto renderer = Renderer([&]{
        std::lock_guard<std::mutex> lk(mtx);
        TrackerStep s = last;

        std::tm g; time_t e = simEpoch.load(); gmtime_r(&e, &g);
        int lh = (g.tm_hour + 24 + tzOffset) % 24;
        char ds[64]; snprintf(ds,sizeof(ds), "%04d-%02d-%02d", g.tm_year+1900, g.tm_mon+1, g.tm_mday);
        char ls[16]; snprintf(ls,sizeof(ls), "%02d:%02d", lh, g.tm_min);
        char us[16]; snprintf(us,sizeof(us), "%02d:%02d", g.tm_hour, g.tm_min);

        bool stow = wind >= cfg.windStowMph;
        auto windBar = gauge(std::min(1.0f, wind/40.0f)) | color(stow?Color::Red:Color::Green) | size(WIDTH,EQUAL,16);

        auto status = window(text(" Status "), vbox({
            text(std::string("Date  ") + ds),
            text(std::string("Local ") + ls + "   UTC " + us +
                 (playing? "   [PLAYING]" : "   [PAUSED]")),
            text(fmt("Speed %.0fx", speed)),
            separator(),
            hbox({text("State  "), text(trackerStateName(s.state)) | bold | color(stateColor(s.state))}),
            separator(),
            text(fmt("Sun elevation  %6.1f deg", s.sun.elevation)),
            text(fmt("Sun azimuth    %6.1f deg", s.sun.azimuth)),
            text(std::string("Sun  ") + (s.sun.aboveHorizon ? "UP" : "below horizon")),
            separator(),
            text(fmt("Panel target   %+6.1f deg", s.targetAngle)),
            text(fmt("Panel actual   %+6.1f deg", panelActual)),
            separator(),
            hbox({text(fmt("Wind %4.0f mph ", wind)), windBar}),
            text(fmt("  stow >= %.0f", cfg.windStowMph)) | dim,
        }));

        auto settings = window(text(" Settings "), vbox({
            text(fmt("Latitude     %7.2f", cfg.lat)),
            text(fmt("Longitude    %7.2f", cfg.lon)),
            text(fmt("Axis tilt    %6.1f deg", cfg.axisTilt)),
            text(fmt("Axis azimuth %6.1f deg", cfg.axisAzimuth)),
        }));

        auto sky = window(text(" Side view (W <-> E):  (*) sun   = panel   | normal "), skyCanvas(s));

        auto help = window(text(" Controls "), vbox({
            text("space play/pause   j/k speed   , . scrub time (±10 min)"),
            text("- = wind            [ ] axis tilt    < > axis azimuth"),
            text("1 summer solstice  2 winter solstice  3 equinox  4 sunrise"),
            text("a/z latitude        q quit"),
        }) );

        return vbox({
            text("  SolarMax — Sun Tracking Simulator  ") | bold | center,
            hbox({ status | size(WIDTH,EQUAL,40), sky | flex }),
            settings,
            help,
        });
    });

    auto withKeys = CatchEvent(renderer, [&](Event ev){
        std::lock_guard<std::mutex> lk(mtx);
        if (ev == Event::Character('q') || ev == Event::Escape) { running=false; screen.Exit(); return true; }
        if (ev == Event::Character(' ')) { playing = !playing; return true; }
        if (ev == Event::Character('j')) { speed = std::max(1.0,   speed/1.5);  return true; }
        if (ev == Event::Character('k')) { speed = std::min(5000.0,speed*1.5);  return true; }
        if (ev == Event::Character(',') || ev == Event::ArrowLeft)  { simEpoch = simEpoch.load() - 600; return true; }
        if (ev == Event::Character('.') || ev == Event::ArrowRight) { simEpoch = simEpoch.load() + 600; return true; }
        if (ev == Event::Character('-')) { wind = std::max(0.0f,  wind-1.0f); return true; }
        if (ev == Event::Character('=') || ev == Event::Character('+')) { wind = std::min(60.0f, wind+1.0f); return true; }
        if (ev == Event::Character('[')) { cfg.axisTilt = std::max(-45.0f, cfg.axisTilt-1.0f); return true; }
        if (ev == Event::Character(']')) { cfg.axisTilt = std::min( 45.0f, cfg.axisTilt+1.0f); return true; }
        if (ev == Event::Character('<')) { cfg.axisAzimuth = std::fmod(cfg.axisAzimuth-5.0f+360.0f,360.0f); return true; }
        if (ev == Event::Character('>')) { cfg.axisAzimuth = std::fmod(cfg.axisAzimuth+5.0f,360.0f); return true; }
        if (ev == Event::Character('a')) { cfg.lat = std::min( 66.0f, cfg.lat+1.0f); return true; }
        if (ev == Event::Character('z')) { cfg.lat = std::max(-66.0f, cfg.lat-1.0f); return true; }
        if (ev == Event::Character('1')) { simEpoch = mkEpoch(2026, 6,21,19,30); return true; } // summer solstice noon
        if (ev == Event::Character('2')) { simEpoch = mkEpoch(2026,12,21,19,30); return true; } // winter solstice noon
        if (ev == Event::Character('3')) { simEpoch = mkEpoch(2026, 3,20,19,30); return true; } // equinox noon
        if (ev == Event::Character('4')) { simEpoch = mkEpoch(2026, 6,21,13, 0); return true; } // ~sunrise
        return false;
    });

    screen.Loop(withKeys);
    running = false;
    if (worker.joinable()) worker.join();
    return 0;
}
