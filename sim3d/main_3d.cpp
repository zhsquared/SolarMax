// SolarMax — 3D Sky-Dome Simulator (raylib).
//
// A real 3D view of the sun crossing the sky dome and the panel tracking it,
// driven by the ACTUAL firmware math (lib/solar_math) and control brain
// (lib/tracker_core). Built to demonstrate correctness against textbook sun
// paths: it defaults to the EQUATOR (lat 0, lon 0), where the answers are
// self-evident and match sun_paths.jpg —
//     equinox noon  -> sun at the zenith (90 deg)
//     June solstice -> arc 23.5 deg to the NORTH  (noon elev 66.5 deg)
//     Dec  solstice -> arc 23.5 deg to the SOUTH  (noon elev 66.5 deg)
//
// Orbit the camera with the left mouse button; zoom with the wheel.
// Build: see sim3d/README.md.

#include "raylib.h"
#include "raymath.h"

#include "tracker_core.h"
#include "sky_scene.h"
#include "simclock.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

// Dome radius in world units. Sky directions (unit) are scaled by this.
static const float R = 3.0f;

// Map an East-North-Up vector to raylib world space: East=+X, Up=+Y, North=-Z.
static Vector3 enu(V3 v, float s = R) { return { v.e * s, v.u * s, -v.n * s }; }

// Draw a polyline through ENU points scaled to the dome.
static void drawArc(const std::vector<V3>& pts, Color col, float s = R) {
    for (size_t i = 1; i < pts.size(); ++i)
        DrawLine3D(enu(pts[i-1], s), enu(pts[i], s), col);
}

// Clean UI font (a real TTF loaded from the OS; falls back to raylib's default).
// TX() is a drop-in for TX() that renders with it.
static Font gUI;
static void TX(const char* t, int x, int y, int fs, Color c) {
    DrawTextEx(gUI, t, { (float)x, (float)y }, (float)fs, (float)fs*0.06f, c);
}
// Measured width of a string in the UI font, and a right-aligned draw (text ends
// at rightX). Using the real width means labels never collide regardless of font.
static int  txtW(const char* t, int fs) { return (int)MeasureTextEx(gUI, t, (float)fs, (float)fs*0.06f).x; }
static void TXR(const char* t, int rightX, int y, int fs, Color c) { TX(t, rightX - txtW(t, fs), y, fs, c); }

// Load the first available clean monospaced system font; keep default on failure.
static Font loadUIFont() {
    const char* cands[] = {
#if defined(_WIN32)
        "C:\\Windows\\Fonts\\consola.ttf", "C:\\Windows\\Fonts\\segoeui.ttf", "C:\\Windows\\Fonts\\arial.ttf",
#elif defined(__APPLE__)
        "/System/Library/Fonts/SFNSMono.ttf", "/System/Library/Fonts/Menlo.ttc", "/System/Library/Fonts/Supplemental/Arial.ttf",
#else
        "/usr/share/fonts/truetype/dejavu/DejaVuSansMono.ttf", "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
#endif
    };
    for (const char* p : cands) {
        if (!FileExists(p)) continue;
        Font f = LoadFontEx(p, 64, nullptr, 0);        // bake at high res
        if (f.texture.id != 0 && f.glyphCount > 0) {
            // We draw the HUD at 12-20px, i.e. always *smaller* than the 64px
            // atlas. Mipmaps + trilinear let the GPU sample a pre-shrunk copy so
            // each glyph stroke stays an even brightness (no minification sparkle).
            GenTextureMipmaps(&f.texture);
            SetTextureFilter(f.texture, TEXTURE_FILTER_TRILINEAR);
            return f;
        }
    }
    return GetFontDefault();
}

// A 2D label anchored at a 3D point (billboarded via world->screen projection).
static void label3D(Camera cam, V3 dir, float s, const char* txt, Color col, int fs = 18) {
    Vector2 sp = GetWorldToScreen(enu(dir, s), cam);
    TX(txt, (int)sp.x, (int)sp.y, fs, col);
}

int main() {
    // ── Window ──────────────────────────────────────────────────────────────
    SetConfigFlags(FLAG_MSAA_4X_HINT | FLAG_WINDOW_RESIZABLE);
    InitWindow(1180, 760, "SolarMax — 3D Sky-Dome Tracker");
    SetTargetFPS(60);
    SetExitKey(KEY_NULL);            // we handle ESC ourselves (cancel edit vs quit)
    gUI = loadUIFont();

    // ── Control brain + live state ──────────────────────────────────────────
    TrackerCore   core;
    TrackerConfig cfg;
    cfg.lat = 0.0f; cfg.lon = 0.0f;                 // default: the equator demo
    TrackerStep   last{};

    double epoch      = epochOf(2026, 3, 20, 12, 0); // equinox, solar noon (UTC)
    double speed      = 240.0;                        // sim seconds per real second
    bool   playing    = true;
    float  wind       = 5.0f;
    float  panelActual = 0.0f;

    // Reference arcs (recomputed whenever latitude changes) — the REAL math.
    float lastLat = 1e9f;
    std::vector<V3> arcJune, arcEqx, arcDec;
    auto rebuildArcs = [&]{
        arcJune = sunPathArc(2026, 6, 21, cfg.lat, cfg.lon);
        arcEqx  = sunPathArc(2026, 3, 20, cfg.lat, cfg.lon);
        arcDec  = sunPathArc(2026,12, 21, cfg.lat, cfg.lon);
        lastLat = cfg.lat;
    };

    auto checks = equatorChecks();

    // Panel angle/rate-over-time plot, recomputed when the day or site changes.
    int graphMode = 1;                  // 0 = off, 1 = angle vs time, 2 = rate vs time
    std::vector<DaySample> curve;
    NoonInfo monthNoon{};               // ideal tilt for the upcoming month (what you'd set)
    bool  autoTilt = false;             // demo: drive the axis tilt to the monthly value
    int cDay = -1; float cLat=1e9f, cLon=1e9f, cTilt=1e9f, cAz=1e9f;
    int cMon = -1;                      // cache key for the monthly recompute

    // ── Editable inputs: date, time (UTC), latitude, longitude ────────────────
    int  focusField = -1;               // -1 = none, 0 = date, 1 = time, 2 = lat, 3 = lon
    char ibuf[24] = {0};                // edit buffer for the focused field
    bool wantQuit = false;
    auto initBuf = [&](int i){          // fill the buffer from the current value
        SimDate d = fromEpoch(epoch);
        if      (i==0) snprintf(ibuf,sizeof ibuf,"%04d-%02d-%02d", d.year,d.month,d.day);
        else if (i==1) snprintf(ibuf,sizeof ibuf,"%02d:%02d", d.hour,d.minute);
        else if (i==2) snprintf(ibuf,sizeof ibuf,"%.2f", cfg.lat);
        else if (i==3) snprintf(ibuf,sizeof ibuf,"%.2f", cfg.lon);
    };
    auto commitField = [&](int i){      // parse the buffer and apply it
        SimDate d = fromEpoch(epoch); int a,bb,c;
        if      (i==0){ if(sscanf(ibuf,"%d-%d-%d",&a,&bb,&c)==3){ bb=std::min(12,std::max(1,bb)); c=std::min(31,std::max(1,c)); epoch=epochOf(a,bb,c,d.hour,d.minute);} }
        else if (i==1){ if(sscanf(ibuf,"%d:%d",&a,&bb)==2){ a=std::min(23,std::max(0,a)); bb=std::min(59,std::max(0,bb)); epoch=epochOf(d.year,d.month,d.day,a,bb);} }
        else if (i==2){ cfg.lat = std::min( 85.0f, std::max(-85.0f,  (float)atof(ibuf))); }
        else if (i==3){ cfg.lon = std::min(180.0f, std::max(-180.0f, (float)atof(ibuf))); }
    };

    // ── Orbit camera ────────────────────────────────────────────────────────
    Camera3D cam{};
    cam.up = { 0, 1, 0 };
    cam.fovy = 45.0f;
    cam.projection = CAMERA_PERSPECTIVE;
    float camAz = 40 * DEG2RAD, camEl = 22 * DEG2RAD, camDist = 8.5f;

    while (!WindowShouldClose() && !wantQuit) {
        float ft = GetFrameTime();

        // Input-panel geometry (shared by the mouse handler and the renderer).
        const int ix=10, iy=362, iw=318, rowH=28, ipH=34+4*rowH+24;
        Rectangle fr[4];
        for (int i=0;i<4;i++) fr[i] = Rectangle{ (float)(ix+66), (float)(iy+34+i*rowH), 150.0f, 22.0f };
        Vector2 mp = GetMousePosition();
        bool overInput = CheckCollisionPointRec(mp, Rectangle{(float)ix,(float)iy,(float)iw,(float)ipH});

        // ── Input ────────────────────────────────────────────────────────────
        // Click a field to edit it; click elsewhere to commit and unfocus.
        if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
            int hit=-1; for(int i=0;i<4;i++) if(CheckCollisionPointRec(mp,fr[i])){hit=i;break;}
            if (hit>=0)              { if(focusField>=0 && focusField!=hit) commitField(focusField); focusField=hit; initBuf(hit); }
            else if (focusField>=0)  { commitField(focusField); focusField=-1; }
        }

        if (focusField >= 0) {
            // Typing into a field. Allow digits and separators; Enter applies.
            int len=(int)strlen(ibuf), ch=GetCharPressed();
            while (ch>0) { if(len<20 && strchr("0123456789-.:",ch)){ ibuf[len++]=(char)ch; ibuf[len]=0; } ch=GetCharPressed(); }
            if (IsKeyPressed(KEY_BACKSPACE) && len>0) ibuf[len-1]=0;
            if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_KP_ENTER)) { commitField(focusField); focusField=-1; }
            if (IsKeyPressed(KEY_TAB))    { commitField(focusField); focusField=(focusField+1)%4; initBuf(focusField); }
            if (IsKeyPressed(KEY_ESCAPE)) { focusField=-1; }        // cancel edit
        } else {
            // Global keyboard shortcuts (suppressed while a field is focused).
            if (IsKeyPressed(KEY_SPACE)) playing = !playing;
            if (IsKeyPressed(KEY_K)) speed = std::min(20000.0, speed * 1.5);
            if (IsKeyPressed(KEY_J)) speed = std::max(1.0,     speed / 1.5);
            if (IsKeyPressed(KEY_RIGHT)) epoch += 600;      // +10 min
            if (IsKeyPressed(KEY_LEFT))  epoch -= 600;      // -10 min
            if (IsKeyPressed(KEY_EQUAL) || IsKeyPressed(KEY_KP_ADD))      wind = std::min(60.0f, wind + 1.0f);
            if (IsKeyPressed(KEY_MINUS) || IsKeyPressed(KEY_KP_SUBTRACT)) wind = std::max(0.0f,  wind - 1.0f);
            if (IsKeyPressed(KEY_T)) autoTilt = !autoTilt;             // toggle auto seasonal tilt
            if (!autoTilt) {                                           // manual axis tilt (disabled while auto)
                if (IsKeyPressed(KEY_RIGHT_BRACKET)) cfg.axisTilt = std::min( 45.0f, cfg.axisTilt + 1.0f);
                if (IsKeyPressed(KEY_LEFT_BRACKET))  cfg.axisTilt = std::max(-45.0f, cfg.axisTilt - 1.0f);
                if (IsKeyPressed(KEY_PERIOD)) cfg.axisAzimuth = std::fmod(cfg.axisAzimuth + 5.0f, 360.0f);
                if (IsKeyPressed(KEY_COMMA))  cfg.axisAzimuth = std::fmod(cfg.axisAzimuth - 5.0f + 360.0f, 360.0f);
            }
            if (IsKeyPressed(KEY_A)) cfg.lat = std::min( 85.0f, cfg.lat + 1.0f);
            if (IsKeyPressed(KEY_Z)) cfg.lat = std::max(-85.0f, cfg.lat - 1.0f);
            if (IsKeyPressed(KEY_G)) graphMode = (graphMode + 1) % 3;   // cycle plot: off/angle/rate
            // Presets (equator solar noon = 12:00 UTC at lon 0).
            if (IsKeyPressed(KEY_ONE))   epoch = epochOf(2026, 6, 21, 12, 0);  // June solstice noon
            if (IsKeyPressed(KEY_TWO))   epoch = epochOf(2026,12, 21, 12, 0);  // Dec  solstice noon
            if (IsKeyPressed(KEY_THREE)) epoch = epochOf(2026, 3, 20, 12, 0);  // Equinox noon (zenith)
            if (IsKeyPressed(KEY_FOUR))  epoch = epochOf(2026, 3, 20,  6,10);  // Equinox sunrise (due E)
            if (IsKeyPressed(KEY_ZERO) || IsKeyPressed(KEY_E)) {               // reset to equator demo
                cfg.lat = 0.0f; cfg.lon = 0.0f; epoch = epochOf(2026, 3, 20, 12, 0);
            }
            if (IsKeyPressed(KEY_ESCAPE)) wantQuit = true;
        }

        // Mouse orbit + zoom (not while dragging on the input panel).
        if (IsMouseButtonDown(MOUSE_BUTTON_LEFT) && !overInput) {
            Vector2 d = GetMouseDelta();
            camAz -= d.x * 0.006f;
            camEl  = Clamp(camEl + d.y * 0.006f, 3 * DEG2RAD, 88 * DEG2RAD);
        }
        camDist = Clamp(camDist - GetMouseWheelMove() * 0.6f, 4.0f, 16.0f);
        cam.position = { std::cos(camEl) * std::sin(camAz) * camDist,
                         std::sin(camEl) * camDist,
                         std::cos(camEl) * std::cos(camAz) * camDist };
        cam.target = { 0, R * 0.35f, 0 };

        // ── Advance simulated time + step the REAL brain ──────────────────────
        if (playing && focusField < 0) epoch += (double)ft * speed;   // paused while typing
        if (cfg.lat != lastLat) rebuildArcs();

        SimDate now = fromEpoch(epoch);

        // Monthly set-angle: the efficiency-optimal fixed tilt for the *upcoming*
        // month, taken at noon on the 15th of the next month (you adjust once a
        // month, ahead of time — e.g. in December you set for mid-January).
        int nmY = now.year, nmM = now.month + 1;
        if (nmM > 12) { nmM = 1; nmY++; }
        int packedMon = nmY*100 + nmM;
        if (packedMon!=cMon || cfg.lat!=cLat || cfg.lon!=cLon) {
            monthNoon = solarNoon(nmY, nmM, 15, cfg.lat, cfg.lon);
            cMon = packedMon;
        }

        // AUTO-TILT (demo): drive the axis to the monthly value + face the equator,
        // so the panel visibly tracks the sun correctly. Off = manual (real usage).
        if (autoTilt && monthNoon.valid) {
            cfg.axisTilt    = monthNoon.tilt;
            cfg.axisAzimuth = (monthNoon.az > 90 && monthNoon.az < 270) ? 0.0f : 180.0f;
        }

        int packedDay = now.year*10000 + now.month*100 + now.day;
        if (packedDay!=cDay || cfg.lat!=cLat || cfg.lon!=cLon || cfg.axisTilt!=cTilt || cfg.axisAzimuth!=cAz) {
            curve = panelDayCurve(now.year, now.month, now.day, cfg.lat, cfg.lon,
                                  cfg.axisTilt, cfg.axisAzimuth, cfg.panelMin, cfg.panelMax);
            cDay=packedDay; cLat=cfg.lat; cLon=cfg.lon; cTilt=cfg.axisTilt; cAz=cfg.axisAzimuth;
        }

        TrackerInputs in;
        in.year = now.year; in.month = now.month; in.day = now.day;
        in.hourUTC = hourUTCof(now);
        in.windMph = wind; in.timeValid = true;
        last = core.step(cfg, in);

        // Ease the physical panel toward target (~60 deg/s) so motion is visible.
        float maxStep = ft * 60.0f, err = last.targetAngle - panelActual;
        if (std::fabs(err) <= maxStep) panelActual = last.targetAngle;
        else panelActual += (err > 0 ? maxStep : -maxStep);

        V3 sunD = skyDir(last.sun.elevation, last.sun.azimuth);
        V3 nrm  = panelNormal(panelActual, cfg.axisTilt, cfg.axisAzimuth);

        // ══ DRAW ══════════════════════════════════════════════════════════════
        BeginDrawing();
        ClearBackground(Color{ 12, 16, 26, 255 });

        BeginMode3D(cam);
            // Ground disc + compass cross.
            DrawCircle3D({0,0,0}, R, {1,0,0}, 90.0f, Fade(GREEN, 0.5f));
            DrawLine3D(enu({-1,0,0}), enu({1,0,0}), Fade(LIGHTGRAY, 0.5f)); // E-W
            DrawLine3D(enu({0,-1,0}), enu({0,1,0}), Fade(LIGHTGRAY, 0.5f)); // N-S
            for (int a = 0; a < 360; a += 30) {                            // faint ground rays
                V3 p = skyDir(0, a);
                DrawLine3D({0,0,0}, enu(p), Fade(DARKGRAY, 0.4f));
            }

            // Dome wireframe: elevation rings + azimuth meridians.
            for (int el = 15; el <= 75; el += 15) {
                std::vector<V3> ring;
                for (int a = 0; a <= 360; a += 10) ring.push_back(skyDir(el, a));
                drawArc(ring, Fade(SKYBLUE, 0.18f));
            }
            for (int a = 0; a < 360; a += 30) {
                std::vector<V3> mer;
                for (int el = 0; el <= 90; el += 6) mer.push_back(skyDir(el, a));
                drawArc(mer, Fade(SKYBLUE, 0.15f));
            }

            // Reference sun paths (match sun_paths.jpg legend).
            drawArc(arcJune, Color{ 235, 70, 160, 255 });   // June solstice — magenta
            drawArc(arcEqx,  Color{ 90, 130, 255, 255 });   // Equinoxes    — blue
            drawArc(arcDec,  Color{ 235, 70,  60, 255 });   // Dec solstice — red

            // Live sun: sphere + ray from origin + drop line to ground.
            if (last.sun.aboveHorizon) {
                Vector3 sp = enu(sunD);
                DrawLine3D({0,0,0}, sp, Fade(YELLOW, 0.6f));
                DrawSphere(sp, 0.16f, YELLOW);
                DrawLine3D(sp, { sp.x, 0, sp.z }, Fade(YELLOW, 0.25f));
                DrawSphere({ sp.x, 0.01f, sp.z }, 0.05f, Fade(YELLOW, 0.5f));
            }

            // Panel plate (translucent, drawn from both sides) + normal arrow.
            V3 q[4]; panelQuad(panelActual, cfg.axisTilt, cfg.axisAzimuth, 1.15f, 0.8f, q);
            Vector3 c0 = enu(q[0],1), c1 = enu(q[1],1), c2 = enu(q[2],1), c3 = enu(q[3],1);
            Color face = Fade(Color{ 60, 200, 120, 255 }, 0.55f);
            DrawTriangle3D(c0, c1, c2, face); DrawTriangle3D(c0, c2, c3, face);
            DrawTriangle3D(c0, c2, c1, face); DrawTriangle3D(c0, c3, c2, face);
            DrawLine3D(c0, c1, DARKGREEN); DrawLine3D(c1, c2, DARKGREEN);
            DrawLine3D(c2, c3, DARKGREEN); DrawLine3D(c3, c0, DARKGREEN);
            Vector3 nTip = enu(nrm, 1.6f);
            DrawLine3D({0,0,0}, nTip, SKYBLUE);
            DrawSphere(nTip, 0.09f, SKYBLUE);

            // Rotation axis (thin, through the origin).
            V3 ax = axisDir(cfg.axisTilt, cfg.axisAzimuth);
            DrawLine3D(enu(ax, -1.3f), enu(ax, 1.3f), Fade(ORANGE, 0.7f));
        EndMode3D();

        // Compass labels (projected).
        label3D(cam, skyDir(0,  90), R*1.04f, "E", WHITE);
        label3D(cam, skyDir(0, 270), R*1.04f, "W", WHITE);
        label3D(cam, skyDir(0,   0), R*1.04f, "N", WHITE);
        label3D(cam, skyDir(0, 180), R*1.04f, "S", WHITE);

        // ── HUD ───────────────────────────────────────────────────────────────
        auto stateCol = [&](TrackerState s){
            switch (s){ case TS_TRACKING: return GREEN; case TS_STOW: return RED;
                        case TS_NIGHT: return SKYBLUE;  case TS_INIT: return YELLOW;
                        default: return MAROON; } };

        DrawRectangle(0, 0, 336, 352, Fade(BLACK, 0.55f));
        int y = 12;
        auto line = [&](const char* s, Color c = WHITE){ TX(s, 14, y, 18, c); y += 23; };
        char b[128];
        TX("SolarMax  3D Sky Dome", 14, y, 20, WHITE); y += 28;
        snprintf(b,sizeof b,"%04d-%02d-%02d  %02d:%02d UTC", now.year,now.month,now.day,now.hour,now.minute); line(b);
        snprintf(b,sizeof b,"%s   %s   %.0fx", trackerStateName(last.state), playing?"PLAY":"PAUSE", speed);
        TX(b,14,y,18,stateCol(last.state)); y+=23;
        snprintf(b,sizeof b,"lat %.1f  lon %.1f", cfg.lat, cfg.lon); line(b);
        snprintf(b,sizeof b,"axis tilt %.0f  az %.0f", cfg.axisTilt, cfg.axisAzimuth); line(b);
        y += 6;
        snprintf(b,sizeof b,"sun  elev %5.1f  az %5.1f", last.sun.elevation, last.sun.azimuth); line(b, YELLOW);
        snprintf(b,sizeof b,"panel target %+5.1f", last.targetAngle); line(b);
        snprintf(b,sizeof b,"panel actual %+5.1f", panelActual);      line(b, SKYBLUE);
        snprintf(b,sizeof b,"wind %.0f mph  (stow >= %.0f)", wind, cfg.windStowMph);
        line(b, wind >= cfg.windStowMph ? RED : GREEN);
        y += 6;
        // Panel tilt angles. Two different things:
        //  * ideal now — the live tilt to point straight at the sun this instant
        //    (90 - elevation, facing the sun); changes continuously. A reference.
        //  * set <month> — the seasonal tilt the operator dials in for the upcoming
        //    month (|lat - decl|); what AUTO-TILT applies. Changes ~once a month.
        static const char* MON[]     = {"Jan","Feb","Mar","Apr","May","Jun",
                                        "Jul","Aug","Sep","Oct","Nov","Dec"};
        static const char* COMPASS[] = {"N","NE","E","SE","S","SW","W","NW"};
        const char* fM = (monthNoon.az > 90 && monthNoon.az < 270) ? "S" : "N";
        TX("PANEL TILT (deg from horizontal)", 14, y, 16, WHITE); y += 21;
        if (last.sun.elevation > 0.0f) {
            int ci = ((int)std::lround(last.sun.azimuth / 45.0f)) % 8; if (ci < 0) ci += 8;
            snprintf(b,sizeof b,"  ideal now  %2.0f  face %s", 90.0f - last.sun.elevation, COMPASS[ci]);
        } else {
            snprintf(b,sizeof b,"  ideal now  -- (sun down)");
        }
        TX(b, 14, y, 15, WHITE); y += 19;
        snprintf(b,sizeof b,"  set %s    %2.0f  face %s", MON[nmM-1], monthNoon.tilt, fM);
        TX(b, 14, y, 16, ORANGE); y += 21;
        snprintf(b,sizeof b,"  AUTO-TILT %s   [T]", autoTilt ? "ON" : "OFF");
        TX(b, 14, y, 15, autoTilt ? GREEN : WHITE); y += 20;

        // ── Editable input panel (date / time / location) ─────────────────────
        DrawRectangle(ix, iy, iw, ipH, Fade(BLACK, 0.55f));
        TX("INPUT  (click a field, type, Enter)", ix+10, iy+8, 15, WHITE);
        {
            const char* labs[4] = {"Date","Time","Lat","Lon"};
            SimDate d = fromEpoch(epoch);
            char cur[4][24];
            snprintf(cur[0],24,"%04d-%02d-%02d", d.year,d.month,d.day);
            snprintf(cur[1],24,"%02d:%02d UTC",  d.hour,d.minute);
            snprintf(cur[2],24,"%.2f", cfg.lat);
            snprintf(cur[3],24,"%.2f", cfg.lon);
            bool blink = std::fmod(GetTime(), 1.0) < 0.5;
            for (int i=0;i<4;i++) {
                bool foc = (focusField==i);
                TX(labs[i], ix+12, (int)fr[i].y+3, 15, WHITE);
                DrawRectangleRec(fr[i], Fade(foc?DARKBLUE:BLACK, 0.6f));
                DrawRectangleLinesEx(fr[i], foc?2.0f:1.0f, foc?SKYBLUE:Fade(WHITE,0.4f));
                TX(foc ? ibuf : cur[i], (int)fr[i].x+6, (int)fr[i].y+3, 15, foc?SKYBLUE:WHITE);
                if (foc && blink) TX("_", (int)fr[i].x+6+txtW(ibuf,15), (int)fr[i].y+2, 15, SKYBLUE);
            }
            TX("Enter apply   Tab next   Esc cancel", ix+12, iy+ipH-19, 12, WHITE);
        }

        // Verification panel (known equator positions). A row lights green when
        // the live sun matches it — hit presets 1/2/3/4 at the equator to check.
        bool atEquator = std::fabs(cfg.lat) < 0.6f && std::fabs(cfg.lon) < 0.6f;
        int vw = 384, vx = GetScreenWidth() - vw - 14, vy = 10;
        DrawRectangle(vx, vy, vw, 26 + (int)checks.size()*22 + 30, Fade(BLACK, 0.55f));
        TX("VERIFY vs known sun (equator)", vx+12, vy+8, 18, WHITE);
        int ry = vy + 34;
        for (auto& ck : checks) {
            bool hit = false;
            if (atEquator && last.sun.aboveHorizon) {
                bool elOk;
                if      (ck.expElev >= 89.0f) elOk = last.sun.elevation > 87.0f;        // zenith
                else if (ck.expElev <= 1.0f)  elOk = last.sun.elevation < 5.0f;         // horizon
                else                          elOk = std::fabs(last.sun.elevation - ck.expElev) < 2.5f;
                float ad = std::fabs(std::fmod(last.sun.azimuth - ck.expAz + 540.0f, 360.0f) - 180.0f);
                bool azOk = (ck.expAz < 0) || (ad < 3.0f);
                hit = elOk && azOk;
            }
            snprintf(b,sizeof b,"%-21s el %4.1f", ck.label, ck.expElev);
            TX(b, vx+12, ry, 16, hit ? GREEN : WHITE);
            if (hit) TXR("< MATCH", vx+vw-12, ry, 16, GREEN);
            ry += 22;
        }
        TX(atEquator ? "press 1/2/3/4 to check" : "press 0 for equator demo",
                 vx+12, ry+4, 14, WHITE);

        // ── Panel angle / rate over time plot ─────────────────────────────────
        if (graphMode) {
            bool rateMode = (graphMode == 2);
            int gw = 580, gh = 176, gx = 14, gy = GetScreenHeight() - 30 - gh - 8;
            DrawRectangle(gx, gy, gw, gh, Fade(BLACK, 0.62f));
            int px = gx + 46, py = gy + 26, pw = gw - 62, ph = gh - 48;
            float ymin, ymax;
            if (rateMode) {
                // Auto-fit the Y axis to the speed curve so its shape is visible
                // (the values only span a few deg/hr). Keep the 15/hr line in view,
                // and enforce a minimum window so the flat equator case doesn't
                // zoom into rounding noise.
                float lo = 1e9f, hi = -1e9f;
                for (auto& d : curve) if (d.up && !std::isnan(d.rate)) {
                    float r = std::fabs(d.rate); lo = std::min(lo, r); hi = std::max(hi, r);
                }
                if (lo > hi) { lo = 0.0f; hi = 24.0f; }          // no daylight samples
                lo = std::min(lo, 15.0f); hi = std::max(hi, 15.0f);
                float margin = std::max(0.8f, (hi - lo) * 0.15f);
                ymin = lo - margin; ymax = hi + margin;
                const float MINWIN = 6.0f;
                if (ymax - ymin < MINWIN) { float mid=(ymin+ymax)*0.5f; ymin=mid-MINWIN*0.5f; ymax=mid+MINWIN*0.5f; }
                if (ymin < 0.0f) ymin = 0.0f;
            } else { ymin = -95.0f; ymax = 95.0f; }
            auto X = [&](float h){ return px + h/24.0f*pw; };
            auto Y = [&](float v){ return py + ph - (v-ymin)/(ymax-ymin)*ph; };

            TX(rateMode ? "panel speed |deg/hr| vs UTC" : "panel angle (deg) vs UTC",
               gx+8, gy+6, 16, WHITE);
            // Compact legend (angle mode): faint = ideal, bright = actual motor.
            if (!rateMode) {
                int lx = gx + 252, ly = gy + 13;
                DrawLine(lx, ly, lx+18, ly, Fade(GREEN,0.4f)); TX("ideal", lx+22, gy+6, 13, WHITE);
                int lx2 = lx + 22 + txtW("ideal",13) + 16;
                DrawLine(lx2, ly, lx2+18, ly, GREEN);          TX("motor", lx2+22, gy+6, 13, WHITE);
            }

            if (rateMode) {
                float yr = Y(15.0f);
                DrawLine(px,(int)yr,px+pw,(int)yr, ORANGE);
                TXR("15/hr = constant-speed reference", px+pw,(int)yr-16, 13, ORANGE);
                TXR(TextFormat("%.0f", ymax), px-5, py-3,      12, WHITE);   // zoomed Y-axis
                TXR(TextFormat("%.0f", ymin), px-5, py+ph-10,  12, WHITE);
            } else {
                DrawLine(px,(int)Y(0),px+pw,(int)Y(0), Fade(GRAY,0.5f));
                for (float lv : {-30.0f, 30.0f}) DrawLine(px,(int)Y(lv),px+pw,(int)Y(lv), Fade(RED,0.55f));
                TX("+/-30 limit", px+2,(int)Y(30)-14, 12, RED);
            }
            for (int hh=0; hh<=24; hh+=6) {
                float xx=X((float)hh); DrawLine((int)xx,py+ph,(int)xx,py+ph+4, GRAY);
                TX(TextFormat("%02d",hh),(int)xx-8,py+ph+6, 12, WHITE);
            }

            // ideal (faint) then clamped/rate (bright)
            if (!rateMode) {
                bool f=true; Vector2 pv{};
                for (auto& d : curve){ if(!d.up){f=true;continue;} Vector2 c{X(d.hourUTC),Y(d.ideal)};
                    if(!f) DrawLineEx(pv,c,1,Fade(GREEN,0.35f)); pv=c; f=false; }
            }
            bool f=true; Vector2 pv{};
            for (auto& d : curve){
                if(!d.up || (rateMode && std::isnan(d.rate))){ f=true; continue; }
                float v = rateMode ? std::fabs(d.rate) : d.clamped;
                Vector2 c{X(d.hourUTC),Y(v)};
                if(!f) DrawLineEx(pv,c,2, rateMode?SKYBLUE:GREEN);
                pv=c; f=false;
            }

            // current-time cursor + live readout
            float ch = (float)hourUTCof(now);
            DrawLine((int)X(ch),py,(int)X(ch),py+ph, Fade(YELLOW,0.7f));
            if (last.sun.aboveHorizon) {
                float i1 = (float)panelAngleForAxis(last.sun.elevation,last.sun.azimuth,cfg.axisTilt,cfg.axisAzimuth);
                SolarAngles s2 = calculateSolarPositionRaw(now.year,now.month,now.day,ch+0.1,cfg.lat,cfg.lon,cfg.axisTilt,cfg.axisAzimuth);
                float rateNow = s2.elevation>0 ? (float)((panelAngleForAxis(s2.elevation,s2.azimuth,cfg.axisTilt,cfg.axisAzimuth)-i1)/0.1) : 0.0f;
                float dv = rateMode ? std::fabs(rateNow) : last.targetAngle;
                DrawCircle((int)X(ch),(int)Y(dv), 4, YELLOW);
                TXR(rateMode ? TextFormat("now %.1f deg/hr", std::fabs(rateNow))
                             : TextFormat("now %+.1f deg", last.targetAngle),
                    gx+gw-12, gy+6, 15, YELLOW);
            }
        }

        // Controls strip.
        DrawRectangle(0, GetScreenHeight()-30, GetScreenWidth(), 30, Fade(BLACK,0.6f));
        TX("SPACE play  J/K speed  <-/-> scrub  -/= wind  [ ] tilt  ,/. azim  A/Z lat  "
                 "1-4 presets  0 equator  T auto-tilt  G plot  mouse orbit/zoom  ESC quit",
                 12, GetScreenHeight()-22, 13, WHITE);

        EndDrawing();
    }

    if (gUI.texture.id != GetFontDefault().texture.id) UnloadFont(gUI);
    CloseWindow();
    return 0;
}
