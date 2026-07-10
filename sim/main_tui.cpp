// SolarMax interactive terminal simulator (FTXUI).
// Runs the REAL control brain (lib/tracker_core) on a simulated sun + wind, so
// you can scrub time, jump to solstice/equinox, change wind and roof angle, and
// watch the panel track. Two views: a flat side view and a rotatable 3D
// wireframe sky dome (press 'v' to switch). Build: see sim/README.md.

#include <ftxui/component/component.hpp>
#include <ftxui/component/screen_interactive.hpp>
#include <ftxui/dom/elements.hpp>
#include <ftxui/dom/canvas.hpp>

#include "tracker_core.h"
#include "sky_scene.h"     // shared 3D geometry (also used by the raylib sim)
#include "simclock.h"      // portable UTC clock (Windows-safe, no timegm)

#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

using namespace ftxui;

static std::string fmt(const char* f, double a) { char b[64]; snprintf(b, sizeof(b), f, a); return b; }

int main() {
    // ── Live state (shared between the update thread and the renderer) ──────────
    std::mutex   mtx;
    TrackerCore  core;
    TrackerConfig cfg;          // lat/lon/axis/wind thresholds - editable live
    TrackerStep  last{};        // most recent decision (read by renderer)

    int    tzOffset    = -7;    // Arizona (display only)
    double epoch       = epochOf(2026, 6, 21, 12, 15);  // sim time, UTC seconds
    double speed       = 120.0; // simulated seconds per real second
    bool   playing     = true;
    float  wind        = 5.0f;
    float  panelActual = 0.0f;  // eased toward target to animate the motor
    int    viewMode    = 1;     // 0 = side view, 1 = 3D wireframe dome, 2 = angle/rate plot
    int    graphMode   = 0;     // within plot view: 0 = angle, 1 = rate
    double yaw = -35 * M_PI/180, pitch = 22 * M_PI/180;   // 3D camera angles

    // Cached reference sun-path arcs (recomputed when lat/lon changes).
    std::vector<V3> arcJune, arcEqx, arcDec;
    float arcLat = 1e9f, arcLon = 1e9f;
    // Cached panel angle/rate-over-time curve (recomputed when day or site changes).
    std::vector<DaySample> curve;
    NoonInfo noon{};             // ideal fixed set-angle for the current date
    int   cDay = -1; float cLat=1e9f, cLon=1e9f, cTilt=1e9f, cAz=1e9f;
    auto checks = equatorChecks();

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
            if (playing) epoch += dt * speed;

            SimDate g = fromEpoch(epoch);
            TrackerInputs in;
            in.year = g.year; in.month = g.month; in.day = g.day;
            in.hourUTC = hourUTCof(g);
            in.windMph = wind; in.timeValid = true;

            last = core.step(cfg, in);

            // Rebuild the reference arcs with the REAL math if the site moved.
            if (cfg.lat != arcLat || cfg.lon != arcLon) {
                arcJune = sunPathArc(2026, 6, 21, cfg.lat, cfg.lon);
                arcEqx  = sunPathArc(2026, 3, 20, cfg.lat, cfg.lon);
                arcDec  = sunPathArc(2026,12, 21, cfg.lat, cfg.lon);
                arcLat = cfg.lat; arcLon = cfg.lon;
            }
            int pday = g.year*10000 + g.month*100 + g.day;
            if (pday!=cDay || cfg.lat!=cLat || cfg.lon!=cLon || cfg.axisTilt!=cTilt || cfg.axisAzimuth!=cAz) {
                curve = panelDayCurve(g.year, g.month, g.day, cfg.lat, cfg.lon,
                                      cfg.axisTilt, cfg.axisAzimuth, cfg.panelMin, cfg.panelMax);
                noon  = solarNoon(g.year, g.month, g.day, cfg.lat, cfg.lon);
                cDay=pday; cLat=cfg.lat; cLon=cfg.lon; cTilt=cfg.axisTilt; cAz=cfg.axisAzimuth;
            }

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
    auto sideCanvas = [&](const TrackerStep& s){
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

    // ── 3D wireframe sky-dome canvas (rotatable) ───────────────────────────────
    auto domeCanvas = [&](const TrackerStep& s){
        const int W = 110, H = 76;
        Canvas c(W, H);
        const double cx = W/2.0, cy = H*0.60, scale = 30.0;
        const double cy_ = std::cos(yaw), sy_ = std::sin(yaw);
        const double cp = std::cos(pitch), sp = std::sin(pitch);

        // Project an ENU direction (scaled by r) to canvas pixels.
        auto proj = [&](V3 p, double r, int& X, int& Y){
            double e = p.e*r, n = p.n*r, u = p.u*r;
            double e1 =  e*cy_ + n*sy_;      // yaw about Up
            double n1 = -e*sy_ + n*cy_;
            double u2 = -n1*sp + u*cp;       // pitch about right axis
            X = (int)std::lround(cx + e1*scale);
            Y = (int)std::lround(cy - u2*scale);
        };
        auto lineDir = [&](V3 a, double ra, V3 b, double rb, Color col){
            int x1,y1,x2,y2; proj(a,ra,x1,y1); proj(b,rb,x2,y2);
            c.DrawPointLine(x1,y1,x2,y2,col);
        };
        auto polyline = [&](const std::vector<V3>& pts, Color col){
            for (size_t i=1;i<pts.size();++i) lineDir(pts[i-1],1.0,pts[i],1.0,col);
        };

        // Ground ring + compass cross + labels.
        std::vector<V3> ring;
        for (int a=0;a<=360;a+=8) ring.push_back(skyDir(0,a));
        polyline(ring, Color::GrayDark);
        lineDir(skyDir(0,90),1.0, skyDir(0,270),1.0, Color::GrayDark);   // E-W
        lineDir(skyDir(0, 0),1.0, skyDir(0,180),1.0, Color::GrayDark);   // N-S
        auto lbl=[&](int az,const char* t){ int x,y; proj(skyDir(0,az),1.08,x,y); c.DrawText(x,y,t,Color::GrayLight); };
        lbl(0,"N"); lbl(90,"E"); lbl(180,"S"); lbl(270,"W");

        // Faint dome: elevation rings + meridians.
        for (int el=30; el<=60; el+=30){
            std::vector<V3> rg; for (int a=0;a<=360;a+=12) rg.push_back(skyDir(el,a));
            polyline(rg, Color::GrayDark);
        }
        for (int a=0;a<360;a+=45){
            std::vector<V3> mer; for (int el=0;el<=90;el+=9) mer.push_back(skyDir(el,a));
            polyline(mer, Color::GrayDark);
        }

        // Reference sun paths (same legend as sun_paths.jpg).
        polyline(arcJune, Color::Magenta);   // June solstice
        polyline(arcEqx,  Color::Blue);      // equinoxes
        polyline(arcDec,  Color::Red);       // December solstice

        // Live sun: ray from origin + marker.
        if (s.sun.aboveHorizon){
            V3 sd = skyDir(s.sun.elevation, s.sun.azimuth);
            lineDir((V3){0,0,0},0.0, sd,1.0, Color::Yellow);
            int sx,sy; proj(sd,1.0,sx,sy); c.DrawText(sx-3,sy-2,"(*)",Color::Yellow);
        }

        // Panel plate (quad) + normal.
        V3 q[4]; panelQuad(panelActual, cfg.axisTilt, cfg.axisAzimuth, 0.6f, 0.4f, q);
        for (int i=0;i<4;++i) lineDir(q[i],1.0,q[(i+1)%4],1.0,Color::Green);
        V3 nrm = panelNormal(panelActual, cfg.axisTilt, cfg.axisAzimuth);
        lineDir((V3){0,0,0},0.0, nrm,0.9, Color::Cyan);
        return canvas(std::move(c));
    };

    // ── Panel angle / rate vs time plot ────────────────────────────────────────
    auto graphCanvas = [&](bool rateMode, const SimDate& g){
        const int W = 120, H = 64;
        Canvas c(W, H);
        int x0 = 8, y0 = 3, pw = W - 12, ph = H - 10;
        double ymin = rateMode ? 0.0 : -95.0, ymax = rateMode ? 24.0 : 95.0;
        auto X  = [&](double h){ return (int)std::lround(x0 + h/24.0*pw); };
        auto Yv = [&](double v){ return (int)std::lround(y0 + ph - (v-ymin)/(ymax-ymin)*ph); };

        c.DrawPointLine(x0, y0+ph, x0+pw, y0+ph, Color::GrayDark);        // baseline
        if (rateMode) {
            int yr = Yv(15.0);
            c.DrawPointLine(x0, yr, x0+pw, yr, Color::Yellow);           // constant-speed ref
            c.DrawText(x0+2, yr-4, "15/hr ref", Color::Yellow);
        } else {
            for (double lv : {-30.0, 30.0}) c.DrawPointLine(x0, Yv(lv), x0+pw, Yv(lv), Color::Red);
            c.DrawPointLine(x0, Yv(0), x0+pw, Yv(0), Color::GrayDark);
            c.DrawText(x0+2, Yv(30)-4, "+/-30 limit", Color::Red);
        }
        for (int hh=0; hh<=24; hh+=6) c.DrawText(X(hh)-2, y0+ph+2, std::to_string(hh), Color::GrayLight);

        if (!rateMode) {                                                 // faint ideal
            int px=-1, py=-1;
            for (auto& d : curve){ if(!d.up){px=-1;continue;} int X1=X(d.hourUTC),Y1=Yv(d.ideal);
                if(px>=0) c.DrawPointLine(px,py,X1,Y1,Color::GrayDark); px=X1; py=Y1; }
        }
        int px=-1, py=-1;                                                // main curve
        for (auto& d : curve){
            if(!d.up || (rateMode && std::isnan(d.rate))){ px=-1; continue; }
            double v = rateMode ? std::fabs(d.rate) : d.clamped;
            int X1=X(d.hourUTC), Y1=Yv(v);
            if(px>=0) c.DrawPointLine(px,py,X1,Y1, rateMode?Color::Cyan:Color::Green);
            px=X1; py=Y1;
        }
        int cx = X(hourUTCof(g)); c.DrawPointLine(cx, y0, cx, y0+ph, Color::Yellow);   // now
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

        SimDate g = fromEpoch(epoch);
        int lh = (g.hour + 24 + tzOffset) % 24;
        char ds[64]; snprintf(ds,sizeof(ds), "%04d-%02d-%02d", g.year, g.month, g.day);
        char ls[16]; snprintf(ls,sizeof(ls), "%02d:%02d", lh, g.minute);
        char us[16]; snprintf(us,sizeof(us), "%02d:%02d", g.hour, g.minute);

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
            separator(),
            text(std::string("Set panel tilt ") + fmt("%.0f", noon.tilt) +
                 " deg face " + ((noon.az > 90 && noon.az < 270) ? "S" : "N"))
                 | color(Color::Yellow) | bold,
            text("  ideal fixed tilt for this date") | dim,
        }));

        // Verification vs known equator sun positions. A row turns green when the
        // live sun matches it - at the equator, hit presets 1/2/3/4 to land on one.
        bool atEquator = std::fabs(cfg.lat) < 0.6f && std::fabs(cfg.lon) < 0.6f;
        Elements vrows;
        for (auto& ck : checks) {
            bool hit = false;
            if (atEquator && s.sun.aboveHorizon) {
                bool elOk;
                if      (ck.expElev >= 89.0f) elOk = s.sun.elevation > 87.0f;           // zenith
                else if (ck.expElev <= 1.0f)  elOk = s.sun.elevation < 5.0f;            // horizon
                else                          elOk = std::fabs(s.sun.elevation - ck.expElev) < 2.5f;
                float ad = std::fabs(std::fmod(s.sun.azimuth - ck.expAz + 540.0f, 360.0f) - 180.0f);
                bool azOk = (ck.expAz < 0) || (ad < 3.0f);
                hit = elOk && azOk;
            }
            auto row = text(fmt(std::string(ck.label).append(" %4.1f").c_str(), ck.expElev));
            vrows.push_back(hit ? (row | color(Color::Green) | bold) : (row | dim));
        }
        vrows.push_back(text(atEquator ? "press 1-4 to land on a check"
                                       : "press 0 for the equator demo") | dim);
        auto verify = window(text(" Verify (equator) "), vbox(std::move(vrows)));

        auto settings = window(text(" Settings "), hbox({
            vbox({
                text(fmt("Latitude     %7.2f", cfg.lat)),
                text(fmt("Longitude    %7.2f", cfg.lon)),
            }),
            separator(),
            vbox({
                text(fmt("Axis tilt    %6.1f deg", cfg.axisTilt)),
                text(fmt("Axis azimuth %6.1f deg", cfg.axisAzimuth)),
            }),
        }));

        std::string vname; Element skyEl;
        if (viewMode == 0) {
            vname = " Side view (W <-> E):  (*) sun   = panel   | normal ";
            skyEl = sideCanvas(s);
        } else if (viewMode == 1) {
            vname = " 3D sky dome (o/p yaw  n/m pitch):  (*) sun  paths J/E/D=magenta/blue/red ";
            skyEl = domeCanvas(s);
        } else {
            vname = graphMode ? " Panel speed |deg/hr| vs time  (g: show angle) "
                              : " Panel angle vs time  (dim=ideal green=clamped, g: show speed) ";
            skyEl = graphCanvas(graphMode, g);
        }
        auto sky = window(text(vname), skyEl);

        auto help = window(text(" Controls "), vbox({
            text("space play/pause  j/k speed  , . scrub  v view (side/3D/plot)  g angle|speed"),
            text("- = wind   [ ] axis tilt   < > axis azimuth   a/z latitude"),
            text("1 summer  2 winter  3 equinox  4 sunrise  0 equator   q quit"),
        }) );

        return vbox({
            text("  SolarMax - Sun Tracking Simulator  ") | bold | center,
            hbox({ status | size(WIDTH,EQUAL,40), sky | flex, verify | size(WIDTH,EQUAL,32) }),
            settings,
            help,
        });
    });

    auto withKeys = CatchEvent(renderer, [&](Event ev){
        std::lock_guard<std::mutex> lk(mtx);
        if (ev == Event::Character('q') || ev == Event::Escape) { running=false; screen.Exit(); return true; }
        if (ev == Event::Character(' ')) { playing = !playing; return true; }
        if (ev == Event::Character('v')) { viewMode = (viewMode+1) % 3; return true; }
        if (ev == Event::Character('g')) { graphMode ^= 1; return true; }
        if (ev == Event::Character('j')) { speed = std::max(1.0,   speed/1.5);  return true; }
        if (ev == Event::Character('k')) { speed = std::min(5000.0,speed*1.5);  return true; }
        if (ev == Event::Character(',') || ev == Event::ArrowLeft)  { epoch -= 600; return true; }
        if (ev == Event::Character('.') || ev == Event::ArrowRight) { epoch += 600; return true; }
        if (ev == Event::Character('-')) { wind = std::max(0.0f,  wind-1.0f); return true; }
        if (ev == Event::Character('=') || ev == Event::Character('+')) { wind = std::min(60.0f, wind+1.0f); return true; }
        if (ev == Event::Character('[')) { cfg.axisTilt = std::max(-45.0f, cfg.axisTilt-1.0f); return true; }
        if (ev == Event::Character(']')) { cfg.axisTilt = std::min( 45.0f, cfg.axisTilt+1.0f); return true; }
        if (ev == Event::Character('<')) { cfg.axisAzimuth = std::fmod(cfg.axisAzimuth-5.0f+360.0f,360.0f); return true; }
        if (ev == Event::Character('>')) { cfg.axisAzimuth = std::fmod(cfg.axisAzimuth+5.0f,360.0f); return true; }
        if (ev == Event::Character('a')) { cfg.lat = std::min( 66.0f, cfg.lat+1.0f); return true; }
        if (ev == Event::Character('z')) { cfg.lat = std::max(-66.0f, cfg.lat-1.0f); return true; }
        // 3D camera rotation
        if (ev == Event::Character('o')) { yaw   -= 8*M_PI/180; return true; }
        if (ev == Event::Character('p')) { yaw   += 8*M_PI/180; return true; }
        if (ev == Event::Character('n')) { pitch = std::max( 2*M_PI/180, pitch-6*M_PI/180); return true; }
        if (ev == Event::Character('m')) { pitch = std::min(88*M_PI/180, pitch+6*M_PI/180); return true; }
        // presets - equator solar noon is 12:00 UTC at lon 0
        if (ev == Event::Character('1')) { epoch = epochOf(2026, 6,21,12,0); return true; } // summer solstice noon
        if (ev == Event::Character('2')) { epoch = epochOf(2026,12,21,12,0); return true; } // winter solstice noon
        if (ev == Event::Character('3')) { epoch = epochOf(2026, 3,20,12,0); return true; } // equinox noon
        if (ev == Event::Character('4')) { epoch = epochOf(2026, 3,20, 6,10); return true; } // equinox sunrise
        if (ev == Event::Character('0')) { cfg.lat=0; cfg.lon=0; epoch=epochOf(2026,3,20,12,0); return true; } // equator demo
        return false;
    });

    screen.Loop(withKeys);
    running = false;
    if (worker.joinable()) worker.join();
    return 0;
}
